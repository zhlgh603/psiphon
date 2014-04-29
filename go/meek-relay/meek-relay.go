package main

import (
    "os"
    "flag"
    "net/http"
    "log"
    "time"
    "fmt"
    "errors"
    "encoding/base64"
    "io"
    "io/ioutil"
    "encoding/json"
    "sync"
    "bitbucket.org/psiphon/psiphon-circumvention-system/go/utils/crypto"
)

const HTTPS_DEFAULT_PORT = 443
const HTTP_DEFAULT_PORT = 80
const maxSessionStaleness = 120 * time.Second

type Session struct {
    meekServer string
    meekSessionID string
    LastSeen time.Time
    serverPayload string
}

func (session *Session) Touch() {
    session.LastSeen = time.Now()
}

func (session *Session) Expired() bool {
    return time.Since(session.LastSeen) > maxSessionStaleness
}

type Relay struct {
    sessionMap map[string]*Session
    lock       sync.RWMutex
    crypto    *crypto.Crypto
    listenTLS bool
}

func (relay *Relay) ServeHTTP(w http.ResponseWriter, r *http.Request) {
    clientPayload := ""
    for _, c := range r.Cookies() {
        clientPayload = c.Value
        break
    }

    session , err := relay.GetSession(r, clientPayload)
    if err != nil {
        log.Printf("GetSession: %s", err)
        http.NotFound(w, r)
        return
    }

    fr, err := relay.buildRequest(r, session)
    if err != nil {
        log.Printf("buildRequest: %s", err)
        http.NotFound(w, r)
        relay.CloseSession(clientPayload)
        return
    }

    resp, err := http.DefaultTransport.RoundTrip(fr)
    if err != nil {
        log.Printf("RoundTrip: %s", err)
        http.Error(w, err.Error(), http.StatusInternalServerError)
        return
    }
    defer resp.Body.Close()
    for _, key := range reflectedHeaderFields {
        value := resp.Header.Get(key)
        if value != "" {
            w.Header().Add(key, value)
        }
    }
    w.WriteHeader(resp.StatusCode)
    io.Copy(w, resp.Body)
}

func (relay *Relay)buildRequest(r *http.Request, session *Session) (*http.Request, error) {

    cReq, err := http.NewRequest(r.Method, fmt.Sprintf("http://%s/", session.meekServer), r.Body)
    if err != nil {
        return nil, err
    }

    cookie := &http.Cookie{Name: "key", Value: session.serverPayload}
    cReq.AddCookie(cookie)

    for _, key := range reflectedHeaderFields {
        value := r.Header.Get(key)
        if value != "" {
            cReq.Header.Add(key, value)
        }
    }

    return cReq, nil
}

func (relay *Relay)buildServerPayload(r *http.Request, meekSessionID, psiphonServer string) (string, error) {
    payload := make(map[string]string)
    payload["s"] = meekSessionID
    payload["s"] = psiphonServer

    //we do not trust any injected headers in plain HTTP
    if !relay.listenTLS {
        payload["ip"] = r.RemoteAddr
    } else {
        //we are most likely in "fronting" mode, relying on 
        //headers provided by the fronting service

        //Cloudflare
        IP :=  r.Header.Get("Cf-Connecting-Ip")
        if len(IP) > 0 {
            payload["ip"] = IP
        } else {

            //GAE
            country := r.Header.Get("X-Appengine-Country")
            city := r.Header.Get("X-Appengine-City")
            if len(country) > 0 || len(city) > 0 {
                geo := make(map[string]string)
                if len(country) > 0 {
                    geo["r"] = country
                }
                if len(city) > 0 {
                    geo["c"] = city
                }
                j, err := json.Marshal(geo)
                if err != nil {
                    return "", err
                }
                payload["g"] = string(j)
            }
        }
    }

    j, err := json.Marshal(payload)
    if err != nil {
        return "", err
    }
    data, err := relay.crypto.Encrypt(j)
    if err != nil {
        return "", err
    }
    return base64.StdEncoding.EncodeToString(data), nil
}

func (relay *Relay) Start(s *http.Server, tls bool, certFilename string, keyFilename string) {
    relay.listenTLS = tls
    if relay.listenTLS {
        log.Fatal(s.ListenAndServeTLS(certFilename, keyFilename))
    } else {
        log.Fatal(s.ListenAndServe())
    }
}

func (relay *Relay) GetSession(r *http.Request, payload string)(*Session, error) {
    if len(payload) == 0{
        return nil, errors.New("GetSession: payload is empty")
    }
    relay.lock.Lock()
    defer relay.lock.Unlock()

    session, ok := relay.sessionMap[payload]

    if !ok {
        encrypted, err := base64.StdEncoding.DecodeString(payload)
        if err != nil {
            return nil, err
        }
        jsondata, err := relay.crypto.Decrypt(encrypted)
        if err != nil {
            return nil, err
        }

        s, m, p, err := decodeClientJSON(jsondata)
        if err != nil {
            return nil, err
        }
        sPayload, err := relay.buildServerPayload(r, s, p)
        if err != nil {
            return nil, err
        }

        session = &Session{
            meekServer: m,
            meekSessionID: s,
            serverPayload: sPayload,
        }
        session.Touch()
        relay.sessionMap[payload] = session
    }
    return session, nil
}

func (relay *Relay) CloseSession(sessionId string) {
    relay.lock.Lock()
    defer relay.lock.Unlock()
    _ , ok := relay.sessionMap[sessionId]
    if ok {
        delete(relay.sessionMap, sessionId)
    }
}

func (relay *Relay) ExpireSessions() {
    for {
        time.Sleep(maxSessionStaleness / 2)
        relay.lock.Lock()
        for sessionId, session := range relay.sessionMap {
            if session.Expired() {
                delete(relay.sessionMap, sessionId)
            }
        }
        relay.lock.Unlock()
    }
}

func NewRelay() *Relay {
    relay := new(Relay)
    relay.sessionMap = make(map[string]*Session)
    return relay
}

func decodeClientJSON(j []byte) (string, string,  string, error) {
    var f interface{}
    var s, m, p string

    err := json.Unmarshal(j, &f)
    if err != nil {
        return "", "", "", err
    }

    mm := f.(map[string]interface{})

    for k, v := range mm {
        if k == "s" {s = v.(string)}
        if k == "m" {m = v.(string)}
        if k == "p" {p = v.(string)}
    }

    if len(s) == 0 || len(p) == 0 || len(m) == 0 {
        err := fmt.Errorf("decodeClientJSON error decoding '%s'", string(j))
        return "", "", "", err
    }

    //order:  sessionId, meekServer, psiphonServer
    return s, m, p, nil
}

var reflectedHeaderFields = []string{
    "Content-Type",
}

func main() {
    var certFilename, keyFilename string
    var serverPublicKeyFilename, clientPrivateKeyFilename string
    var serverPublicKey, clientPrivateKey [32]byte
    var logFilename string
    var obfuscationKeyword string
    var port int
    var listenTLS bool

    flag.BoolVar(&listenTLS, "tls", false, "use HTTPS")
    flag.StringVar(&certFilename, "cert", "", "TLS certificate file (required with -tls)")
    flag.StringVar(&keyFilename, "key", "", "TLS private key file (required with -tls)")
    flag.StringVar(&logFilename, "log", "", "name of log file")
    flag.IntVar(&port, "port", 0, "port to listen on")
    flag.StringVar(&obfuscationKeyword, "obfskey", "", "obfuscation keyword")
    flag.StringVar(&serverPublicKeyFilename, "spubkey", "", "server public key file required to encrypt payload")
    flag.StringVar(&clientPrivateKeyFilename, "cprivkey", "", "client private key file required to decrypt payload")
    flag.Parse()

    if logFilename != "" {
        f, err := os.OpenFile(logFilename, os.O_CREATE|os.O_APPEND|os.O_WRONLY, 0600)
        if err != nil {
            log.Fatalf("error opening log file: %s", err)
        }
        defer f.Close()
        log.SetOutput(f)
    }

    if serverPublicKeyFilename == "" || clientPrivateKeyFilename == ""{
        log.Fatalf("cprivkey and spubkey are required, exiting now")
    } else {
        var err error
        var read []byte
        read, err = ioutil.ReadFile(serverPublicKeyFilename)
        if err != nil {
            log.Fatalf("error reading serverPublicKeyFilename: %s", err)
        }
        copy(serverPublicKey[:], read)

        read, err = ioutil.ReadFile(clientPrivateKeyFilename)
        if err != nil {
            log.Fatalf("error reading clientPrivateKeyFilename: %s", err)
        }
        copy(clientPrivateKey[:], read)
    }

    if clientPrivateKeyFilename == "" {
        log.Fatalf("client private key file name is required")
    }

    if listenTLS {
        if certFilename == "" || keyFilename == "" {
            log.Fatalf("The -cert and -key options are required with -tls.\n")
        }
        if(port == 0) {
            port = HTTPS_DEFAULT_PORT
        }
    } else {
        if certFilename != "" || keyFilename != "" {
            log.Fatalf("The -cert and -key options are not allowed without -tls.\n")
        }
        if(port == 0) {
            port = HTTP_DEFAULT_PORT
        }
    }

    relay := NewRelay()
    cr := crypto.New(obfuscationKeyword, serverPublicKey, serverPublicKey)
    relay.crypto  = cr

    s := &http.Server{
        Addr:           fmt.Sprintf(":%d", port),
        Handler:        relay,
        ReadTimeout:    10 * time.Second,
        WriteTimeout:   10 * time.Second,
    }

    go relay.ExpireSessions()
    relay.Start(s, listenTLS, certFilename, keyFilename)
}

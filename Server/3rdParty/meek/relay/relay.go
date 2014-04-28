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
    "crypto/sha1"
    "crypto/rc4"
    "crypto/rand"
    "encoding/binary"
    "encoding/json"
    "code.google.com/p/go.crypto/nacl/box"
    "sync"
)

const HTTPS_DEFAULT_PORT = 443
const HTTP_DEFAULT_PORT = 80
const OBFUSCATE_SEED_LENGTH = 16
const OBFUSCATE_KEY_LENGTH = 16
const OBFUSCATE_HASH_ITERATIONS = 6000
const OBFUSCATE_MAGIC_VALUE uint32 = 0x0BF5CA7E
const CLIENT_TO_SERVER_IV = "client_to_server"
const maxSessionStaleness = 120 * time.Second

type ClientData struct {
    meekSessionID string
    psiphonServer string
}

type Session struct {
    clientData *ClientData
    LastSeen time.Time
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
    crypto    *Crypto
    listenTLS bool
}

func (relay *Relay) ServeHTTP(w http.ResponseWriter, r *http.Request) {
    clientPayload := ""
    for _, c := range r.Cookies() {
        clientPayload = c.Value
        break
    }

    session , err := relay.GetSession(clientPayload)
    if err != nil {
        log.Printf("GetSession: %s", err)
        http.NotFound(w, r)
        return
    }

    clientData := session.clientData

    fr, err := relay.buildRequest(r, clientData)
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

func (relay *Relay)buildRequest(r *http.Request, clientData *ClientData) (*http.Request, error) {

    cReq, err := http.NewRequest(r.Method, fmt.Sprintf("http://%s/", clientData.psiphonServer), r.Body)
    if err != nil {
        return nil, err
    }

    serverPayload, err := relay.buildServerPayload(r, clientData.meekSessionID)
    if err != nil {
        return nil, err
    }

    cookie := &http.Cookie{Name: "key", Value: string(serverPayload)}
    cReq.AddCookie(cookie)

    for _, key := range reflectedHeaderFields {
        value := r.Header.Get(key)
        if value != "" {
            cReq.Header.Add(key, value)
        }
    }

    return cReq, nil
}

func (relay *Relay)buildServerPayload(r *http.Request, meekSessionID string) (string, error) {
    payload := make(map[string]string)
    payload["s"] = meekSessionID

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
    data, err := relay.crypto.encryptDataWithNaCl(j)
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

func (relay *Relay) GetSession(payload string)(*Session, error) {
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

        cldata, err := decodeClientJSON(jsondata)
        if err != nil {
            return nil, err
        }
        session = &Session{clientData: cldata}
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

type Crypto struct {
    obfuscationKeyword string
    clientPrivateKey [32]byte
    serverPublicKey [32]byte
    nonce [24]byte
}

func (cr *Crypto)Decrypt(data []byte)([]byte, error) {
    cipherdata, err := cr.deobfuscateData(data)
    if err != nil {
        return nil, err
    }

    jsondata, err := cr.decryptNaClData(cipherdata)
    if err != nil {
        return nil, err
    }
    return jsondata, nil
}

func (cr *Crypto)deobfuscateData(data []byte) ([]byte, error) {
    if len(data) < OBFUSCATE_SEED_LENGTH {
        return nil, errors.New("deobfuscateData: payload is too short")
    }
    h := sha1.New()
    h.Write(data[0:OBFUSCATE_SEED_LENGTH])
    h.Write([]byte(cr.obfuscationKeyword))
    h.Write([]byte(CLIENT_TO_SERVER_IV))
    digest := h.Sum(nil)

    for i := 0; i < 6000; i++ {
        h.Reset()
        h.Write(digest)
        digest = h.Sum(nil)
    }

    if len(digest) < OBFUSCATE_KEY_LENGTH {
        return nil, errors.New("deobfuscateData: SHA1 digest is too short")
    }

    digest = digest[0:OBFUSCATE_KEY_LENGTH]

    cipher, err := rc4.NewCipher(digest)
    if err != nil {
        return nil, errors.New("deobfuscateData: couldn'y init SHA1")
    }

    data = data[OBFUSCATE_SEED_LENGTH:]

    cipher.XORKeyStream(data, data)

    if len(data) < 4 {
        return nil, errors.New("deobfuscateData: magic value is less than 4 bytes")
    }

    if binary.BigEndian.Uint32(data[0:4]) != OBFUSCATE_MAGIC_VALUE {
        return nil, errors.New("deobfuscateData: NewCipher error")
    }

    data = data[4:]
    if len(data) < 4 {
        return nil, errors.New("deobfuscateData: padding length value is less than 4 bytes")
    }

    plength := int(binary.BigEndian.Uint32(data[0:4]))

    data = data[4:]
    if len(data) < plength {
        return nil, errors.New("deobfuscateData: data length is less than padding length")
    }

    data = data[plength:]

    return data, nil
}

func (cr *Crypto)encryptDataWithNaCl(data []byte) ([]byte, error) {
    ephemeralPrivateKey, _ , err := box.GenerateKey(rand.Reader)
    if err != nil {
        return nil, err
    }
    ciphertext := box.Seal(nil, data, &cr.nonce, &cr.serverPublicKey, ephemeralPrivateKey)
    return ciphertext, nil
}

func (cr *Crypto)decryptNaClData(data []byte) ([]byte, error) {
    ephemeralPublicKey, _ , err := box.GenerateKey(rand.Reader)

    if err != nil {
        return nil, err
    }

    open, ok := box.Open(nil, data, &cr.nonce, ephemeralPublicKey, &cr.clientPrivateKey)
    if !ok {
        return nil, errors.New("NaCl couldn't decrypt client's payload")
    }
    return open, nil
}

func NewCrypto (oK string, cPrivKey [32]byte,  sPubKey [32]byte) (cr *Crypto){
    //nonce is filled with 0s: http://golang.org/ref/spec#The_zero_value
    //we do not need to generate a new nonce b/c a new ephemeral key is 
    //generated for every message
    var n [24]byte

    return &Crypto{
        obfuscationKeyword: oK,
        clientPrivateKey: cPrivKey,
        serverPublicKey: sPubKey,
        nonce: n,
    }
}

func decodeClientJSON(j []byte) (*ClientData, error) {
    var f interface{}
    var s, p string

    err := json.Unmarshal(j, &f)
    if err != nil {
        return nil, err
    }

    m := f.(map[string]interface{})

    for k, v := range m {
        if k == "s" {s = v.(string)}
        if k == "p" {p = v.(string)}
    }

    if len(s) == 0 || len(p) == 0 {
        err := fmt.Errorf("decodeClientJSON error decoding '%s'", string(j))
        return nil, err
    }

    return &ClientData{s,p}, nil
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
    crypto := NewCrypto(obfuscationKeyword, clientPrivateKey, serverPublicKey)
    relay.crypto  = crypto

    s := &http.Server{
        Addr:           fmt.Sprintf(":%d", port),
        Handler:        relay,
        ReadTimeout:    10 * time.Second,
        WriteTimeout:   10 * time.Second,
    }

    go relay.ExpireSessions()
    relay.Start(s, listenTLS, certFilename, keyFilename)
}

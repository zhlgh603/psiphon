package main

import (
	"bitbucket.org/psiphon/psiphon-circumvention-system/go/utils/crypto"
	"crypto/tls"
	"encoding/base64"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io"
	"io/ioutil"
	"log"
	"net"
	"net/http"
	"os"
	"sync"
	"time"
)

const maxSessionStaleness = 120 * time.Second

var reflectedHeaderFields = []string{
	"Content-Type",
}

type Config struct {
	Port                   int
	TlsCertificatePEM      string
	TlsPrivateKeyPEM       string
	LogFilename            string
	ClientPrivateKeyBase64 string
	ServerPublicKeyBase64  string
	ListenTLS              bool
	ObfuscationKeyword     string
}

type Session struct {
	meekServer    string
	LastSeen      time.Time
	serverPayload string
}

func (session *Session) Touch() {
	session.LastSeen = time.Now()
}

func (session *Session) Expired() bool {
	return time.Since(session.LastSeen) > maxSessionStaleness
}

type Relay struct {
	sessionMap         map[string]*Session
	lock               sync.RWMutex
	crypto             *crypto.Crypto
	config             *Config
}

func (relay *Relay) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	clientPayload := ""
	for _, c := range r.Cookies() {
		clientPayload = c.Value
		break
	}

	if r.Method != "POST" {
		log.Printf("Bad HTTP request: %+v", r)
		http.NotFound(w, r)
		return
	}

	session, err := relay.GetSession(r, clientPayload)
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

func (relay *Relay) buildRequest(r *http.Request, session *Session) (*http.Request, error) {

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

func (relay *Relay) buildServerPayload(r *http.Request, psiphonServer, sshSessionId string) (string, error) {
	payload := make(map[string]string)
	payload["p"] = psiphonServer
	payload["s"] = sshSessionId

	//we do not trust any injected headers in plain HTTP
	if !relay.config.ListenTLS {
		payload["ip"] = r.RemoteAddr
	} else {
		//we are most likely in "fronting" mode, relying on
		//headers provided by the fronting service

		//Cloudflare
		IP := r.Header.Get("Cf-Connecting-Ip")
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

func (relay *Relay) Start() {
	s := &HTTPServer{
		server: &http.Server{
			Addr:         fmt.Sprintf(":%d", relay.config.Port),
			Handler:      relay,
			ReadTimeout:  10 * time.Second,
			WriteTimeout: 10 * time.Second,
		},
	}

	go relay.ExpireSessions()

	if relay.config.ListenTLS {
		log.Fatal(s.ListenAndServeTLS([]byte(relay.config.TlsCertificatePEM), []byte(relay.config.TlsPrivateKeyPEM)))
	} else {
		log.Fatal(s.ListenAndServe())
	}
}

func (relay *Relay) GetSession(r *http.Request, payload string) (*Session, error) {
	if len(payload) == 0 {
		return nil, errors.New("GetSession: payload is empty")
	}
	relay.lock.Lock()
	defer relay.lock.Unlock()

	session, ok := relay.sessionMap[payload]

	if !ok {
		obfuscated, err := base64.StdEncoding.DecodeString(payload)
		if err != nil {
			return nil, err
		}

		encrypted, err := relay.crypto.Deobfuscate(obfuscated, relay.config.ObfuscationKeyword)
		if err != nil {
			return nil, err
		}

		jsondata, err := relay.crypto.Decrypt(encrypted)

		if err != nil {
			return nil, err
		}

		m, p, s, err := decodeClientJSON(jsondata)
		if err != nil {
			return nil, err
		}
		sPayload, err := relay.buildServerPayload(r, p, s)
		if err != nil {
			return nil, err
		}

		session = &Session{
			meekServer:    m,
			serverPayload: sPayload,
		}
		relay.sessionMap[payload] = session
	}

	session.Touch()

	return session, nil
}

func (relay *Relay) CloseSession(sessionId string) {
	relay.lock.Lock()
	defer relay.lock.Unlock()
	_, ok := relay.sessionMap[sessionId]
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

func NewRelay(c *Config) (*Relay, error) {
	var serverPublicKey, clientPrivateKey [32]byte

	relay := &Relay{config: c}
	relay.sessionMap = make(map[string]*Session)

	keydata, err := base64.StdEncoding.DecodeString(c.ServerPublicKeyBase64)
	if err != nil {
		return nil, fmt.Errorf("error decoding config.ServerPublicKeyBase64: %s", err)
	}
	copy(serverPublicKey[:], keydata)

	keydata, err = base64.StdEncoding.DecodeString(c.ClientPrivateKeyBase64)
	if err != nil {
		return nil, fmt.Errorf("error decoding config.ClientPrivateKeyBase64: %s", err)
	}
	copy(clientPrivateKey[:], keydata)

	cr := crypto.New(serverPublicKey, clientPrivateKey)
	relay.crypto = cr

	return relay, nil
}

//http.Server wrapper
type HTTPServer struct {
	server *http.Server
}

func (s *HTTPServer) ListenAndServe() error {
	return s.server.ListenAndServe()
}

func (s *HTTPServer) ListenAndServeTLS(certPEMBlock, keyPEMBlock []byte) error {

	srv := s.server
	addr := srv.Addr
	if addr == "" {
		addr = ":https"
	}
	config := &tls.Config{}
	if srv.TLSConfig != nil {
		*config = *srv.TLSConfig
	}
	if config.NextProtos == nil {
		config.NextProtos = []string{"http/1.1"}
	}

	var err error
	config.Certificates = make([]tls.Certificate, 1)
	config.Certificates[0], err = tls.X509KeyPair(certPEMBlock, keyPEMBlock)
	if err != nil {
		return err
	}

	conn, err := net.Listen("tcp", addr)
	if err != nil {
		return err
	}

	tlsListener := tls.NewListener(conn, config)
	return srv.Serve(tlsListener)
}

func decodeClientJSON(j []byte) (meekServer, psiphonServer, sshSessionId string, err error) {
	var f interface{}

	err = json.Unmarshal(j, &f)
	if err != nil {
		return
	}

	mm := f.(map[string]interface{})

	for k, v := range mm {
		if k == "m" {
			meekServer = v.(string)
		}
		if k == "p" {
			psiphonServer = v.(string)
		}
		if k == "s" {
			sshSessionId = v.(string)
		}
	}

	if len(meekServer) == 0 || len(psiphonServer) == 0 || len(sshSessionId) == 0 {
		err = fmt.Errorf("decodeClientJSON: error decoding '%s'", string(j))
		return
	}

	return
}

func parseConfigJSON(data []byte) (config *Config, err error) {
	//populate with default values
	config = &Config{
		ListenTLS: false,
	}
	err = json.Unmarshal(data, &config)
	if err != nil {
		return
	}
	log.Printf("Parsed config: (%+v)", config)
	return
}

func main() {
	var configJSONFilename string
	var config *Config

	flag.StringVar(&configJSONFilename, "config", "", "relay JSON config file")
	flag.Parse()

	if configJSONFilename == "" {
		log.Fatalf("config file is required, exiting now")
	} else {
		var err error
		var read []byte
		read, err = ioutil.ReadFile(configJSONFilename)
		if err != nil {
			log.Fatalf("error reading configJSONFilename: %s", err)
		}

		config, err = parseConfigJSON(read)
		if err != nil {
			log.Fatalf("error parsing config: %s", err)
		}
		if config.Port == 0 {
			log.Fatalf("server port is missing from the config file, exiting now")
		}

	}

	if config.Port == 0 {
		log.Fatalf("server port is missing from the config file, exiting now")
	}

	if config.ClientPrivateKeyBase64 == "" {
		log.Fatalf("client private key is missing from the config file, exiting now")
	}

	if config.ServerPublicKeyBase64 == "" {
		log.Fatalf("server public key is missing from the config file, exiting now")
	}

	if config.LogFilename != "" {
		f, err := os.OpenFile(config.LogFilename, os.O_CREATE|os.O_APPEND|os.O_WRONLY, 0600)
		if err != nil {
			log.Fatalf("error opening log file: %s", err)
		}
		defer f.Close()
		log.SetOutput(f)
	}

	if config.ListenTLS {
		if config.TlsPrivateKeyPEM == "" {
			log.Fatalf("TLS mode: TlsPrivateKeyPEM is missing from the config file, exiting now")
		}
		if config.TlsCertificatePEM == "" {
			log.Fatalf("TLS mode: TlsCertificatePEM is missing from the config file, exiting now")
		}
	}

	// Allow up to 100 persistent connections to the same meek server (the relay will reuse
	// the same HTTP client connection [pool] for any client -- which is beneficial -- and we
	// don't want to create a long, serialized pipeline of requests)
	http.DefaultTransport.(*http.Transport).MaxIdleConnsPerHost = 100

	relay, err := NewRelay(config)
	if err != nil {
		log.Fatalf("Could not init a new relay: %s", err)
	}

	relay.Start()
}

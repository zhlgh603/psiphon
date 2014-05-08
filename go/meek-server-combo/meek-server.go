package main

import (
	"bitbucket.org/psiphon/psiphon-circumvention-system/go/utils/crypto"
	"crypto/rand"
	"crypto/rsa"
	"crypto/tls"
	"crypto/x509"
	"crypto/x509/pkix"
	"encoding/base64"
	"encoding/json"
	"encoding/pem"
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
	"math/big"
	//"github.com/garyburd/redigo/redis"
)

const maxPayloadLength = 0x10000
const turnaroundDeadline = 10 * time.Millisecond
const maxSessionStaleness = 120 * time.Second

// Default Redis config values from Server/psi_config.py

const DEFAULT_REDIS_SESSION_DB_HOST = "localhost"
const DEFAULT_REDIS_SESSION_DB_PORT = 6379
const DEFAULT_REDIS_SESSION_DB_INDEX = 2
const DEFAULT_REDIS_SESSION_EXPIRE_SECONDS = 2592000

const DEFAULT_REDIS_DISCOVERY_DB_HOST = DEFAULT_REDIS_SESSION_DB_HOST
const DEFAULT_REDIS_DISCOVERY_DB_PORT = DEFAULT_REDIS_SESSION_DB_PORT
const DEFAULT_REDIS_DISCOVERY_DB_INDEX = 3
const DEFAULT_REDIS_DISCOVERY_EXPIRE_SECONDS = 60 * 5

type Config struct {
	Port                        int
	ListenTLS                   bool
	CookiePrivateKeyBase64      string
	ObfuscationKeyword          string
	LogFilename                 string
	RedisSessionDbHost          string
	RedisSessionDbPort          int
	RedisSessionDbIndex         int
	RedisSessionExpireSeconds   int
	RedisDiscoveryDbHost        string
	RedisDiscoveryDbPort        int
	RedisDiscoveryDbIndex       int
	RedisDiscoveryExpireSeconds int
}

type ClientSessionData struct {
	PsiphonClientSessionId string `json:"psiphonClientSessionId"`
	PsiphonServerAddress   string `json:"psiphonServerAddress"`
}

type Session struct {
	psiConn  *net.TCPConn
	LastSeen time.Time
}

func (session *Session) Touch() {
	session.LastSeen = time.Now()
}

func (session *Session) Expired() bool {
	return time.Since(session.LastSeen) > maxSessionStaleness
}

type Dispatcher struct {
	sessionMap map[string]*Session
	lock       sync.RWMutex
	crypto     *crypto.Crypto
	config     *Config
}

func (dispatcher *Dispatcher) ServeHTTP(responseWriter http.ResponseWriter, request *http.Request) {
	cookie := ""
	for _, c := range request.Cookies() {
		cookie = c.Value
		break
	}

	if request.Method != "POST" {
		log.Printf("Bad HTTP request: %+v", request)
		http.NotFound(responseWriter, request)
		return
	}

	session, err := dispatcher.GetSession(request, cookie)
	if err != nil {
		log.Printf("GetSession: %s", err)
		http.NotFound(responseWriter, request)
		return
	}

	err = dispatcher.dispatch(session, responseWriter, request)
	if err != nil {
		log.Printf("dispatch: %s", err)
		http.NotFound(responseWriter, request)
		dispatcher.CloseSession(cookie)
		return
	}
}

func (dispatcher *Dispatcher) GetSession(request *http.Request, cookie string) (*Session, error) {
	if len(cookie) == 0 {
		return nil, errors.New("cookie is empty")
	}

	dispatcher.lock.Lock()
	session, ok := dispatcher.sessionMap[cookie]
	dispatcher.lock.Unlock()
	if ok {
		session.Touch()
		return session, nil
	}

	obfuscated, err := base64.StdEncoding.DecodeString(cookie)
	if err != nil {
		return nil, err
	}

	encrypted, err := dispatcher.crypto.Deobfuscate(obfuscated, dispatcher.config.ObfuscationKeyword)
	if err != nil {
		return nil, err
	}

	cookieJson, err := dispatcher.crypto.Decrypt(encrypted)
	if err != nil {
		return nil, err
	}

	clientSessionData, err := parseCookieJSON(cookieJson)
	if err != nil {
		return nil, err
	}

	psiphonServer, err := net.ResolveTCPAddr("tcp", clientSessionData.PsiphonServerAddress)
	if err != nil {
		return nil, err
	}

	conn, err := net.DialTCP("tcp", nil, psiphonServer)
	if err != nil {
		return nil, err
	}

	session = &Session{psiConn: conn}
	session.Touch()

	dispatcher.doGeoStats(request, clientSessionData.PsiphonClientSessionId)

	dispatcher.lock.Lock()
	dispatcher.sessionMap[cookie] = session
	dispatcher.lock.Unlock()

	return session, nil
}

func parseCookieJSON(cookieJson []byte) (clientSessionData *ClientSessionData, err error) {
	err = json.Unmarshal(cookieJson, &clientSessionData)
	if err != nil {
		err = fmt.Errorf("parseCookieJSON error decoding '%s'", string(cookieJson))
	}
	return
}

func (dispatcher *Dispatcher) doGeoStats(request *http.Request, psiphonClientSessionId string) {
	// Use Geo info in headers sent by fronts; otherwise use peer IP
	gotGeoHeaders := false

	// Only use headers when sent through TLS (although we're using
	// self signed keys in TLS mode, so man-in-the-middle is technically
	// still possible so "faked stats" is still a risk...?)
	if dispatcher.config.ListenTLS {
		if (!gotGeoHeaders) {
			// Cloudflare
			ip := request.Header.Get("Cf-Connecting-Ip")
			if len(ip) > 0 {
				// TODO: redis operation
				log.Printf("Cf-Connecting-Ip: %s", ip)
				gotGeoHeaders = true
			}
		}

		if (!gotGeoHeaders) {
			// Google App Engine
			country := request.Header.Get("X-Appengine-Country")
			city := request.Header.Get("X-Appengine-City")
			if len(country) > 0 || len(city) > 0 {
				// TODO: redis operation
				log.Printf("X-Appengine-Country:%s , X-Appengine-City: %s", country, city)
				gotGeoHeaders = true
			}
		}
	}

	if (!gotGeoHeaders) {
		ip := request.RemoteAddr
		if len(ip) > 0 {
			// TODO: redis operation
			log.Printf("RemoteAddr: %s", ip)
		}
	}
}

func (dispatcher *Dispatcher) CloseSession(sessionId string) {
	dispatcher.lock.Lock()
	defer dispatcher.lock.Unlock()
	_, ok := dispatcher.sessionMap[sessionId]
	if ok {
		delete(dispatcher.sessionMap, sessionId)
	}
}

func (dispatcher *Dispatcher) ExpireSessions() {
	for {
		time.Sleep(maxSessionStaleness / 2)
		dispatcher.lock.Lock()
		for sessionId, session := range dispatcher.sessionMap {
			if session.Expired() {
				session.psiConn.Close()
				delete(dispatcher.sessionMap, sessionId)
			}
		}
		dispatcher.lock.Unlock()
	}
}

func (dispatcher *Dispatcher) dispatch(session *Session, responseWriter http.ResponseWriter, request *http.Request) error {
	body := http.MaxBytesReader(responseWriter, request.Body, maxPayloadLength+1)
	_, err := io.Copy(session.psiConn, body)
	if err != nil {
		return errors.New(fmt.Sprintf("copying body to psiConn: %s", err))
	}

	buf := make([]byte, maxPayloadLength)
	session.psiConn.SetReadDeadline(time.Now().Add(turnaroundDeadline))
	n, err := session.psiConn.Read(buf)
	if err != nil {
		if e, ok := err.(net.Error); !ok || !e.Timeout() {
			return errors.New(fmt.Sprintf("reading from psiConn: %s", err))
		}
	}

	n, err = responseWriter.Write(buf[:n])
	if err != nil {
		return errors.New(fmt.Sprintf("writing to response: %s", err))
	}

	return nil
}

type MeekHTTPServer struct {
	server *http.Server
}

func (httpServer *MeekHTTPServer) ListenAndServe() error {
	return httpServer.server.ListenAndServe()
}

func (httpServer *MeekHTTPServer) ListenAndServeTLS(certPEMBlock, keyPEMBlock []byte) error {

	addr := httpServer.server.Addr
	if addr == "" {
		addr = ":https"
	}
	tlsConfig := &tls.Config{}
	if httpServer.server.TLSConfig != nil {
		*tlsConfig = *httpServer.server.TLSConfig
	}
	if tlsConfig.NextProtos == nil {
		tlsConfig.NextProtos = []string{"http/1.1"}
	}

	var err error
	tlsConfig.Certificates = make([]tls.Certificate, 1)
	tlsConfig.Certificates[0], err = tls.X509KeyPair(certPEMBlock, keyPEMBlock)
	if err != nil {
		return err
	}

	conn, err := net.Listen("tcp", addr)
	if err != nil {
		return err
	}

	tlsListener := tls.NewListener(conn, tlsConfig)
	return httpServer.server.Serve(tlsListener)
}

func createTLSConfig(host string) (certPEMBlock, keyPEMBlock []byte, err error) {
	now := time.Now()
	tpl := x509.Certificate{
		SerialNumber:          new(big.Int).SetInt64(0),
		Subject:               pkix.Name{CommonName: host},
		NotBefore:             now.Add(-24 * time.Hour).UTC(),
		NotAfter:              now.AddDate(1, 0, 0).UTC(),
		KeyUsage:              x509.KeyUsageKeyEncipherment | x509.KeyUsageDigitalSignature | x509.KeyUsageCertSign,
		BasicConstraintsValid: true,
		MaxPathLen:            1,
		IsCA:                  true,
		SubjectKeyId:          []byte{1, 2, 3, 4},
		Version:               2,
	}
    key, err := rsa.GenerateKey(rand.Reader, 2048)
	if err != nil {
		return
	}
    der, err := x509.CreateCertificate(rand.Reader, &tpl, &tpl, &key.PublicKey, key)
	if err != nil {
		return
	}
	certPEMBlock = pem.EncodeToMemory(&pem.Block{Type: "CERTIFICATE", Bytes: der})
	keyPEMBlock = pem.EncodeToMemory(&pem.Block{Type: "RSA PRIVATE KEY", Bytes: x509.MarshalPKCS1PrivateKey(key)})
    return
}

func (dispatcher *Dispatcher) Start() {
	server := &MeekHTTPServer {
		server: &http.Server {
			Addr:         fmt.Sprintf(":%d", dispatcher.config.Port),
			Handler:      dispatcher,
			ReadTimeout:  10 * time.Second,
			WriteTimeout: 10 * time.Second,
		},
	}

	go dispatcher.ExpireSessions()

	if dispatcher.config.ListenTLS {
		cert, privkey, err := createTLSConfig("www.example.org")
		if err != nil {
			log.Fatalf("createTLSConfig failed to create private key and certificate")
		}
		log.Fatal(server.ListenAndServeTLS(cert, privkey))
	} else {
		log.Fatal(server.ListenAndServe())
	}
}

func NewDispatcher(config *Config) (*Dispatcher, error) {
	var cookiePrivateKey, dummyKey [32]byte
	keydata, err := base64.StdEncoding.DecodeString(config.CookiePrivateKeyBase64)
	if err != nil {
		return nil, fmt.Errorf("error decoding config.CookiePrivateKeyBase64: %s", err)
	}

	copy(cookiePrivateKey[:], keydata)
	crypto := crypto.New(dummyKey, cookiePrivateKey)
	dispatcher := &Dispatcher{
		config:     config,
		crypto:     crypto,
		sessionMap: make(map[string]*Session),
	}
	return dispatcher, nil
}

func parseConfigJSON(data []byte) (config *Config, err error) {
	config = &Config{
		RedisSessionDbHost:          DEFAULT_REDIS_SESSION_DB_HOST,
		RedisSessionDbPort:          DEFAULT_REDIS_SESSION_DB_PORT,
		RedisSessionDbIndex:         DEFAULT_REDIS_SESSION_DB_INDEX,
		RedisSessionExpireSeconds:   DEFAULT_REDIS_SESSION_EXPIRE_SECONDS,
		RedisDiscoveryDbHost:        DEFAULT_REDIS_DISCOVERY_DB_HOST,
		RedisDiscoveryDbPort:        DEFAULT_REDIS_DISCOVERY_DB_PORT,
		RedisDiscoveryDbIndex:       DEFAULT_REDIS_DISCOVERY_DB_INDEX,
		RedisDiscoveryExpireSeconds: DEFAULT_REDIS_DISCOVERY_EXPIRE_SECONDS,
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
	flag.StringVar(&configJSONFilename, "config", "", "JSON config file")
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
	}

	if config.Port == 0 {
		log.Fatalf("port is missing from the config file, exiting now")
	}

	if config.CookiePrivateKeyBase64 == "" {
		log.Fatalf("cookie private key is missing from the config file, exiting now")
	}

	if config.ObfuscationKeyword == "" {
		log.Fatalf("obfuscation keyword is missing from the config file, exiting now")
	}

	if config.LogFilename != "" {
		f, err := os.OpenFile(config.LogFilename, os.O_CREATE|os.O_APPEND|os.O_WRONLY, 0600)
		if err != nil {
			log.Fatalf("error opening log file: %s", err)
		}
		defer f.Close()
		log.SetOutput(f)
	}

	dispatcher, err := NewDispatcher(config)
	if err != nil {
		log.Fatalf("Could not init a new dispatcher: %s", err)
	}

	dispatcher.Start()
}

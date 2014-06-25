package main

import (
	"bitbucket.org/psiphon/psiphon-circumvention-system/go/utils/crypto"
	"crypto/hmac"
	"crypto/rand"
	"crypto/rsa"
	"crypto/sha256"
	"crypto/tls"
	"crypto/x509"
	"crypto/x509/pkix"
	"encoding/base64"
	"encoding/json"
	"encoding/pem"
	"errors"
	"flag"
	"fmt"
	"github.com/fzzy/radix/redis"
	"io"
	"io/ioutil"
	"log"
	"math/big"
	"net"
	"net/http"
	"os"
	"strings"
	"sync"
	"time"
)

const maxPayloadLength = 0x10000
const turnAroundTimeout = 50 * time.Millisecond
const extendedTurnAroundTimeout = 200 * time.Millisecond
const maxSessionStaleness = 120 * time.Second

const MEEK_PROTOCOL_VERSION = 1

// Default config values from Server/psi_config.py

const DEFAULT_GEOIP_SERVICE_PORT = 6000
const DEFAULT_REDIS_DB_HOST = "127.0.0.1"
const DEFAULT_REDIS_DB_PORT = 6379
const DEFAULT_REDIS_SESSION_DB_INDEX = 0
const DEFAULT_REDIS_SESSION_EXPIRE_SECONDS = 2592000
const DEFAULT_REDIS_DISCOVERY_DB_INDEX = 1
const DEFAULT_REDIS_DISCOVERY_EXPIRE_SECONDS = 60 * 5

type Config struct {
	Port                                int
	ListenTLS                           bool
	CookiePrivateKeyBase64              string
	ObfuscatedKeyword                   string
	LogFilename                         string
	GeoIpServicePort                    int
	RedisDbHost                         string
	RedisDbPort                         int
	RedisSessionDbIndex                 int
	RedisSessionExpireSeconds           int
	RedisDiscoveryDbIndex               int
	RedisDiscoveryExpireSeconds         int
	ClientIpAddressStrategyValueHmacKey string
}

type ClientSessionData struct {
	MeekProtocolVersion    int    `json:"v"`
	PsiphonClientSessionId string `json:"s"`
	PsiphonServerAddress   string `json:"p"`
}

type GeoIpData struct {
	Region string `json:"region"`
	City   string `json:"city"`
	Isp    string `json:"isp"`
}

type Session struct {
	psiConn             *net.TCPConn
	meekProtocolVersion int
	LastSeen            time.Time
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
		log.Printf("unexpected request type: %s", request.Method)
		dispatcher.terminateConnection(responseWriter, request)
		return
	}

	session, err := dispatcher.GetSession(request, cookie)
	if err != nil {
		log.Printf("GetSession: %s", err)
		dispatcher.terminateConnection(responseWriter, request)
		return
	}

	err = dispatcher.relayPayload(session, responseWriter, request)
	if err != nil {
		log.Printf("dispatch: %s", err)
		dispatcher.terminateConnection(responseWriter, request)
		dispatcher.CloseSession(cookie)
		return
	}

	/*
		NOTE: this code cleans up session resources quickly (when the
		      peer closes its persistent connection) but isn't
		      appropriate for the fronted case since the front doesn't
		      necessarily keep a persistent connection open.

		notify := responseWriter.(http.CloseNotifier).CloseNotify()

		go func() {
			<-notify
			dispatcher.CloseSession(cookie)
		}()
	*/
}

func (dispatcher *Dispatcher) relayPayload(session *Session, responseWriter http.ResponseWriter, request *http.Request) error {
	body := http.MaxBytesReader(responseWriter, request.Body, maxPayloadLength+1)
	_, err := io.Copy(session.psiConn, body)
	if err != nil {
		return errors.New(fmt.Sprintf("writing payload to psiConn: %s", err))
	}

	if session.meekProtocolVersion >= MEEK_PROTOCOL_VERSION {
		_, err := copyWithTimeout(responseWriter, session.psiConn)
		if err != nil {
			return errors.New(fmt.Sprintf("reading payload from psiConn: %s", err))
		}
	} else {
		buf := make([]byte, maxPayloadLength)
		session.psiConn.SetReadDeadline(time.Now().Add(turnAroundTimeout))
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
	}

	return nil
}

/*
Relays bytes (e.g., from the remote socket (sshd) to the HTTP response payload)
Uses chunked transfer encoding. The relay is run for a max time period, so as
to not block subsequent requests from being sent (assuming non-HTTP-pipelining).
Also, each read from the source uses the standard turnaround timeout, so that if
no data is available we return no slower than the non-chunked mode.

Adapted from Copy: http://golang.org/src/pkg/io/io.go
*/
func copyWithTimeout(dst io.Writer, src *net.TCPConn) (written int64, err error) {
	startTime := time.Now()
	buffer := make([]byte, 64*1024)
	for {
		src.SetReadDeadline(time.Now().Add(turnAroundTimeout))
		bytesRead, errRead := src.Read(buffer)
		if bytesRead > 0 {
			bytesWritten, errWrite := dst.Write(buffer[0:bytesRead])
			if bytesWritten > 0 {
				written += int64(bytesWritten)
			}
			if errWrite != nil {
				err = errWrite
				break
			}
			if bytesRead != bytesWritten {
				err = io.ErrShortWrite
				break
			}
		}
		if errRead == io.EOF {
			break
		}
		if e, ok := errRead.(net.Error); ok && e.Timeout() {
			break
		}
		if errRead != nil {
			err = errRead
			break
		}
		totalElapsedTime := time.Now().Sub(startTime) / time.Millisecond
		if totalElapsedTime >= extendedTurnAroundTimeout {
			break
		}
	}
	return written, err
}

func (dispatcher *Dispatcher) terminateConnection(responseWriter http.ResponseWriter, request *http.Request) {
	http.NotFound(responseWriter, request)

	// Hijack to close socket (after flushing response).
	hijack, ok := responseWriter.(http.Hijacker)
	if !ok {
		log.Printf("webserver doesn't support hijacking")
		return
	}
	conn, buffer, err := hijack.Hijack()
	if err != nil {
		log.Printf("hijack error: %s", err.Error())
		return
	}
	buffer.Flush()
	conn.Close()
}

func (dispatcher *Dispatcher) GetSession(request *http.Request, cookie string) (*Session, error) {
	if len(cookie) == 0 {
		return nil, errors.New("cookie is empty")
	}

	dispatcher.lock.RLock()
	session, ok := dispatcher.sessionMap[cookie]
	dispatcher.lock.RUnlock()
	if ok {
		session.Touch()
		return session, nil
	}

	obfuscated, err := base64.StdEncoding.DecodeString(cookie)
	if err != nil {
		return nil, err
	}

	encrypted, err := dispatcher.crypto.Deobfuscate(obfuscated, dispatcher.config.ObfuscatedKeyword)
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

	session = &Session{psiConn: conn, meekProtocolVersion: clientSessionData.MeekProtocolVersion}
	session.Touch()

	dispatcher.doStats(request, clientSessionData.PsiphonClientSessionId)

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

func (dispatcher *Dispatcher) doStats(request *http.Request, psiphonClientSessionId string) {
	// Use Geo info in headers sent by fronts; otherwise use peer IP
	ipAddress := ""
	var geoIpData *GeoIpData

	// Only use headers when sent through TLS (although we're using
	// self signed keys in TLS mode, so man-in-the-middle is technically
	// still possible so "faked stats" is still a risk...?)
	if dispatcher.config.ListenTLS {
		if geoIpData == nil {
			ipAddress = request.Header.Get("X-Forwarded-For")
			if len(ipAddress) > 0 {
				geoIpData = dispatcher.geoIpRequest(ipAddress)
			}
		}

		if geoIpData == nil {
			// Cloudflare
			ipAddress = request.Header.Get("Cf-Connecting-Ip")
			if len(ipAddress) > 0 {
				geoIpData = dispatcher.geoIpRequest(ipAddress)
			}
		}

		if geoIpData == nil {
			// Google App Engine
			country := request.Header.Get("X-Appengine-Country")
			city := request.Header.Get("X-Appengine-City")
			if len(country) > 0 || len(city) > 0 {
				// TODO: redis operation
				log.Printf("X-Appengine-Country:%s , X-Appengine-City: %s", country, city)
				geoIpData = &GeoIpData{
					Region: country,
					City:   city,
					Isp:    "None",
				}
			}
		}
	}

	if geoIpData == nil {
		ipAddress = strings.Split(request.RemoteAddr, ":")[0]
		geoIpData = dispatcher.geoIpRequest(ipAddress)
	}

	if geoIpData != nil {
		dispatcher.updateRedis(psiphonClientSessionId, ipAddress, geoIpData)
	}
}

func (dispatcher *Dispatcher) geoIpRequest(ipAddress string) (geoIpData *GeoIpData) {
	// Default value is used when request fails
	geoIpData = &GeoIpData{
		Region: "None",
		City:   "None",
		Isp:    "None",
	}
	response, err := http.Get(fmt.Sprintf("http://127.0.0.1:%d/geoip?ip=%s", dispatcher.config.GeoIpServicePort, ipAddress))
	if err != nil {
		log.Printf("geoIP request failed: %s", err)
		return
	}
	defer response.Body.Close()
	if response.StatusCode == 200 {
		responseBody, err := ioutil.ReadAll(response.Body)
		if err != nil {
			log.Printf("geoIP response read failed: %s", err)
			return
		}
		err = json.Unmarshal(responseBody, &geoIpData)
		if err != nil {
			log.Printf("geoIP response decode failed: %s", err)
			return
		}
	}
	return
}

func (dispatcher *Dispatcher) updateRedis(psiphonClientSessionId string, ipAddress string, geoIpData *GeoIpData) {
	redisClient, err := redis.DialTimeout(
		"tcp",
		fmt.Sprintf("%s:%d", dispatcher.config.RedisDbHost, dispatcher.config.RedisDbPort),
		time.Duration(1)*time.Second)
	if err != nil {
		log.Printf("connect to redis failed: %s", err)
		return
	}
	defer redisClient.Close()

	geoIpDataJson, err := json.Marshal(geoIpData)
	if err != nil {
		log.Printf("redis json encode failed: %s", err)
		return
	}

	dispatcher.redisSetExpiringValue(
		redisClient,
		dispatcher.config.RedisSessionDbIndex,
		psiphonClientSessionId,
		string(geoIpDataJson),
		dispatcher.config.RedisSessionExpireSeconds)

	if len(ipAddress) > 0 {
		clientIpAddressStrategyValue := dispatcher.calculateClientIpAddressStrategyValue(ipAddress)
		clientIpAddressStrategyValueMap := map[string]int{"client_ip_address_strategy_value": clientIpAddressStrategyValue}

		clientIpAddressStrategyValueJson, err := json.Marshal(clientIpAddressStrategyValueMap)
		if err != nil {
			log.Printf("redis json encode failed: %s", err)
			return
		}

		dispatcher.redisSetExpiringValue(
			redisClient,
			dispatcher.config.RedisDiscoveryDbIndex,
			psiphonClientSessionId,
			string(clientIpAddressStrategyValueJson),
			dispatcher.config.RedisDiscoveryExpireSeconds)
	}
}

func (dispatcher *Dispatcher) redisSetExpiringValue(redisClient *redis.Client, dbIndex int, key string, value string, expirySeconds int) {
	response := redisClient.Cmd("select", dbIndex)
	if response.Err != nil {
		log.Printf("redis select command failed: %s", response.Err)
		return
	}
	response = redisClient.Cmd("set", key, value)
	if response.Err != nil {
		log.Printf("redis set command failed: %s", response.Err)
		return
	}
	response = redisClient.Cmd("expire", key, expirySeconds)
	if response.Err != nil {
		log.Printf("redis expire command failed: %s", response.Err)
		return
	}
}

func (dispatcher *Dispatcher) calculateClientIpAddressStrategyValue(ipAddress string) int {
	// From: psi_ops_discovery.calculate_ip_address_strategy_value:
	//     # Mix bits from all octets of the client IP address to determine the
	//     # bucket. An HMAC is used to prevent pre-calculation of buckets for IPs.
	//     return ord(hmac.new(HMAC_KEY, ip_address, hashlib.sha256).digest()[0])
	mac := hmac.New(sha256.New, []byte(dispatcher.config.ClientIpAddressStrategyValueHmacKey))
	mac.Write([]byte(ipAddress))
	return int(mac.Sum(nil)[0])
}

func (dispatcher *Dispatcher) CloseSession(sessionId string) {
	dispatcher.lock.Lock()
	session, ok := dispatcher.sessionMap[sessionId]
	if ok {
		dispatcher.closeSessionHelper(sessionId, session)
	}
	dispatcher.lock.Unlock()
}

func (dispatcher *Dispatcher) closeSessionHelper(sessionId string, session *Session) {
	// TODO: close the persistent HTTP client connection, if one exists
	session.psiConn.Close()
	delete(dispatcher.sessionMap, sessionId)
}

func (dispatcher *Dispatcher) ExpireSessions() {
	for {
		time.Sleep(maxSessionStaleness / 2)
		dispatcher.lock.Lock()
		for sessionId, session := range dispatcher.sessionMap {
			if session.Expired() {
				dispatcher.closeSessionHelper(sessionId, session)
			}
		}
		dispatcher.lock.Unlock()
	}
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
	server := &MeekHTTPServer{
		server: &http.Server{
			Addr:    fmt.Sprintf(":%d", dispatcher.config.Port),
			Handler: dispatcher,

			// TODO: This timeout is actually more like a socket lifetime which closes active persistent connections.
			// Implement a custom timeout. See link: https://groups.google.com/forum/#!topic/golang-nuts/NX6YzGInRgE
			ReadTimeout:  600 * time.Second,
			WriteTimeout: 600 * time.Second,
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
		GeoIpServicePort:            DEFAULT_GEOIP_SERVICE_PORT,
		RedisDbHost:                 DEFAULT_REDIS_DB_HOST,
		RedisDbPort:                 DEFAULT_REDIS_DB_PORT,
		RedisSessionDbIndex:         DEFAULT_REDIS_SESSION_DB_INDEX,
		RedisSessionExpireSeconds:   DEFAULT_REDIS_SESSION_EXPIRE_SECONDS,
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

	if config.ObfuscatedKeyword == "" {
		log.Fatalf("obfuscation keyword is missing from the config file, exiting now")
	}

	if config.ClientIpAddressStrategyValueHmacKey == "" {
		log.Fatalf("client ip address strategy value hmac key is missing from the config file, exiting now")
	}

	if config.LogFilename != "" {
		f, err := os.OpenFile(config.LogFilename, os.O_CREATE|os.O_APPEND|os.O_WRONLY, 0600)
		if err != nil {
			log.Fatalf("error opening log file: %s", err)
		}
		defer f.Close()
		log.SetOutput(f)
	} else {
		log.SetOutput(ioutil.Discard)
	}

	dispatcher, err := NewDispatcher(config)
	if err != nil {
		log.Fatalf("Could not init a new dispatcher: %s", err)
	}

	dispatcher.Start()
}

package main

import (
	"bitbucket.org/psiphon/psiphon-circumvention-system/go/utils/crypto"
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
	//"github.com/garyburd/redigo/redis"
)

const maxPayloadLength = 0x10000
const turnaroundDeadline = 10 * time.Millisecond
const maxSessionStaleness = 120 * time.Second

//default values from Server/psi_config.py
const DEFAULT_SESSION_DB_HOST = "localhost"
const DEFAULT_SESSION_DB_PORT = 6379
const DEFAULT_SESSION_DB_INDEX = 0
const DEFAULT_SESSION_EXPIRE_SECONDS = 2592000

const DEFAULT_DISCOVERY_DB_HOST = DEFAULT_SESSION_DB_HOST
const DEFAULT_DISCOVERY_DB_PORT = DEFAULT_SESSION_DB_PORT
const DEFAULT_DISCOVERY_DB_INDEX = 1
const DEFAULT_DISCOVERY_EXPIRE_SECONDS = 60 * 5

type Config struct {
	Port                   int
	ServerPrivateKeyBase64 string
	LogFilename            string
	SessionDbHost          string
	SessionDbPort          int
	SessionDbIndex         int
	SessionExpireSeconds   int
	DiscoveryDbHost        string
	DiscoveryDbPort        int
	DiscoveryDbIndex       int
	DiscoveryExpireSeconds int
}

type GeoData struct {
	region string
	city   string
	isp    string
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

func (d *Dispatcher) ServeHTTP(w http.ResponseWriter, r *http.Request) {
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

	session, err := d.GetSession(r, clientPayload)
	if err != nil {
		log.Printf("GetSession: %s", err)
		http.NotFound(w, r)
		return
	}

	err = d.dispatch(session, w, r)
	if err != nil {
		log.Printf("dispatch: %s", err)
		http.NotFound(w, r)
		d.CloseSession(clientPayload)
		return
	}
}

func (d *Dispatcher) GetSession(r *http.Request, payload string) (*Session, error) {
	if len(payload) == 0 {
		return nil, errors.New("payload is empty")
	}

	d.lock.Lock()
	session, ok := d.sessionMap[payload]
	d.lock.Unlock()

	if ok {
		return session, nil
	}

	encrypted, err := base64.StdEncoding.DecodeString(payload)
	if err != nil {
		return nil, err
	}
	jsondata, err := d.crypto.Decrypt(encrypted)
	if err != nil {
		return nil, err
	}

	psiphonServer, userIP, geoData, err := decodePayloadJSON(jsondata)
	if err != nil {
		return nil, err
	}

	psiphonServerAddr, err := net.ResolveTCPAddr("tcp", psiphonServer)
	if err != nil {
		return nil, err
	}

	conn, err := net.DialTCP("tcp", nil, psiphonServerAddr)
	if err != nil {
		return nil, err
	}

	session = &Session{psiConn: conn}
	session.Touch()

	d.doStats(userIP, geoData)

	d.lock.Lock()
	d.sessionMap[payload] = session
	d.lock.Unlock()

	return session, nil
}

func (d *Dispatcher) doStats(IP string, geoDada *GeoData) {
	log.Printf("IP: %s, geoData: (%+v)", IP, geoDada)
}

func (d *Dispatcher) CloseSession(sessionId string) {
	d.lock.Lock()
	defer d.lock.Unlock()
	_, ok := d.sessionMap[sessionId]
	if ok {
		delete(d.sessionMap, sessionId)
	}
}

func (d *Dispatcher) ExpireSessions() {
	for {
		time.Sleep(maxSessionStaleness / 2)
		d.lock.Lock()
		for sessionId, session := range d.sessionMap {
			if session.Expired() {
				delete(d.sessionMap, sessionId)
			}
		}
		d.lock.Unlock()
	}
}

func (d *Dispatcher) dispatch(session *Session, w http.ResponseWriter, req *http.Request) error {
	body := http.MaxBytesReader(w, req.Body, maxPayloadLength+1)
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
	n, err = w.Write(buf[:n])
	if err != nil {
		return errors.New(fmt.Sprintf("writing to response: %s", err))
	}
	return nil
}

func (d *Dispatcher) Start() {

	s := &http.Server{
		Addr:         fmt.Sprintf(":%d", d.config.Port),
		Handler:      d,
		ReadTimeout:  10 * time.Second,
		WriteTimeout: 10 * time.Second,
	}

	go d.ExpireSessions()
	log.Fatal(s.ListenAndServe())
}

func NewDispatcher(c *Config) (*Dispatcher, error) {

	var serverPrivateKey, dummyKey [32]byte

	keydata, err := base64.StdEncoding.DecodeString(c.ServerPrivateKeyBase64)
	if err != nil {
		return nil, fmt.Errorf("error decoding config.ServerPrivateKeyBase64: %s", err)
	}

	copy(serverPrivateKey[:], keydata)

	cr := crypto.New(dummyKey, serverPrivateKey)

	d := &Dispatcher{
		config:     c,
		crypto:     cr,
		sessionMap: make(map[string]*Session),
	}
	return d, nil
}

func decodePayloadJSON(j []byte) (psiphonServer, userIP string, geoData *GeoData, err error) {
	var f interface{}

	err = json.Unmarshal(j, &f)
	if err != nil {
		return
	}

	mm := f.(map[string]interface{})

	v, ok := mm["p"]
	if !ok {
		err = fmt.Errorf("decodePayloadJSON error decoding '%s'", string(j))
		return
	}
	psiphonServer = v.(string)

	v, ok = mm["ip"]
	if ok {
		userIP = v.(string)
		return //we do no need geoData if we got the IP
	}
	v, ok = mm["g"]
	if ok {
		gg := v.(map[string]interface{})
		geoData = &GeoData{
			region: gg["r"].(string),
			city:   gg["c"].(string),
			isp:    gg["i"].(string),
		}
		return
	}
	err = fmt.Errorf("decodePayloadJSON: no geo fields in '%s'", string(j))
	return
}

func parseConfigJSON(data []byte) (config *Config, err error) {

	//populate with default values
	config = &Config{
		SessionDbHost:          DEFAULT_SESSION_DB_HOST,
		SessionDbPort:          DEFAULT_SESSION_DB_PORT,
		SessionDbIndex:         DEFAULT_SESSION_DB_INDEX,
		SessionExpireSeconds:   DEFAULT_SESSION_EXPIRE_SECONDS,
		DiscoveryDbHost:        DEFAULT_DISCOVERY_DB_HOST,
		DiscoveryDbPort:        DEFAULT_DISCOVERY_DB_PORT,
		DiscoveryDbIndex:       DEFAULT_DISCOVERY_DB_INDEX,
		DiscoveryExpireSeconds: DEFAULT_DISCOVERY_EXPIRE_SECONDS,
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
		log.Fatalf("server port is missing from the config file, exiting now")
	}
	if config.ServerPrivateKeyBase64 == "" {
		log.Fatalf("server private key is missing from the config file, exiting now")
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

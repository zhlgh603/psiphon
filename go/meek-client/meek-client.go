package main

import (
	"bitbucket.org/psiphon/psiphon-circumvention-system/go/utils/crypto"
	"bytes"
	"encoding/base64"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io"
	"log"
	"net"
	"net/http"
	"os"
	"time"
)

const sessionIdLength = 32
const maxPayloadLength = 0x10000
const initPollInterval = 100 * time.Millisecond
const maxPollInterval = 5 * time.Second
const pollIntervalMultiplier = 1.5

var gCookiePayload string //base64 encoded encrypted cookie payload

var gConfig struct {
	ClientPublicKeyBase64 string
	ObfuscationKeyword    string
	MeekServerAddr        string
	PsiphonServerAddr     string
	MeekRelayAddr         string
	FrontingDomain        string
	FrontingHostname      string
	SshSessionID          string
}

func makeCookie() (string, error) {
	var clientPublicKey, dummyKey [32]byte

	keydata, err := base64.StdEncoding.DecodeString(gConfig.ClientPublicKeyBase64)
	if err != nil {
		return "", fmt.Errorf("error decoding gConfig.ClientPublicKeyBase64: %s", err)
	}

	copy(clientPublicKey[:], keydata)
	cr := crypto.New(clientPublicKey, dummyKey)

	cookie := make(map[string]string)
	cookie["m"] = gConfig.MeekServerAddr
	cookie["p"] = gConfig.PsiphonServerAddr
	cookie["s"] = gConfig.SshSessionID

	j, err := json.Marshal(cookie)
	if err != nil {
		return "", err
	}
	encrypted, err := cr.Encrypt(j)
	if err != nil {
		return "", err
	}
	obfuscated, err := cr.Obfuscate(encrypted, gConfig.ObfuscationKeyword)
	if err != nil {
		return "", err
	}
	return base64.StdEncoding.EncodeToString(obfuscated), nil
}

func roundTrip(buf []byte) (*http.Response, error) {
	tr := http.DefaultTransport

	var URL string
	bFronting := false

	if gConfig.FrontingDomain != "" {
		bFronting = true
		URL = fmt.Sprintf("https://%s/", gConfig.FrontingDomain)
	} else {
		URL = fmt.Sprintf("https://%s/", gConfig.MeekRelayAddr)
	}

	req, err := http.NewRequest("POST", URL, bytes.NewReader(buf))
	if err != nil {
		return nil, err
	}
	if bFronting {
		req.Host = gConfig.FrontingHostname
	}
	req.Header.Set("Content-Type", "application/octet-stream")

	cookie := &http.Cookie{Name: "key", Value: gCookiePayload}
	req.AddCookie(cookie)
	return tr.RoundTrip(req)
}

func sendRecv(buf []byte, conn net.Conn) (int64, error) {
	resp, err := roundTrip(buf)
	if err != nil {
		return 0, err
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return 0, errors.New(fmt.Sprintf("status code was %d, not %d", resp.StatusCode, http.StatusOK))
	}

	return io.Copy(conn, io.LimitReader(resp.Body, maxPayloadLength))
}

func pollingLoop(conn net.Conn) error {
	buf := make([]byte, maxPayloadLength)
	var interval time.Duration

	interval = initPollInterval
	for {
		conn.SetReadDeadline(time.Now().Add(interval))
		nr, readErr := conn.Read(buf)

		nw, err := sendRecv(buf[:nr], conn)
		if err != nil {
			return err
		}

		if readErr != nil {
			if e, ok := readErr.(net.Error); !ok || !e.Timeout() {
				return readErr
			}
		}

		if nw > 0 {
			interval = initPollInterval
		} else {
			interval = time.Duration(float64(interval) * pollIntervalMultiplier)
		}
		if interval > maxPollInterval {
			interval = maxPollInterval
		}
	}

	return nil
}

func acceptLoop(ln net.Listener) error {
	defer ln.Close()
	for {
		conn, err := ln.Accept()
		if err != nil {
			log.Printf("error in Accept: %s", err)
			if e, ok := err.(net.Error); ok && !e.Temporary() {
				return err
			}
			continue
		}
		go func() {
			err := pollingLoop(conn)
			if err != nil {
				log.Printf("error in handling request: %s", err)
			}
		}()
	}
	return nil
}

func main() {
	var err error
	flag.StringVar(&gConfig.ClientPublicKeyBase64, "cpubkey", "", "Client public key required to encrypt cookie payload")
	flag.StringVar(&gConfig.ObfuscationKeyword, "obfskeyword", "", "Obfuscation keyword")
	flag.StringVar(&gConfig.MeekServerAddr, "meekserver", "", "Meek server address(x.x.x.x:n")
	flag.StringVar(&gConfig.PsiphonServerAddr, "psiphonserver", "", "Psiphon server address(x.x.x.x:n")
	flag.StringVar(&gConfig.MeekRelayAddr, "meekrelay", "", "Meek relay address(x.x.x.x:n")
	flag.StringVar(&gConfig.FrontingDomain, "frontdomain", "", "Domain to use for fronting")
	flag.StringVar(&gConfig.FrontingHostname, "fronthost", "", "Host header to use for fronting")
	flag.StringVar(&gConfig.SshSessionID, "sessid", "", "Client SSH session id")
	flag.Parse()

	if gConfig.ClientPublicKeyBase64 == "" {
		log.Fatalf("-cpubkey is a required argument, exiting now")
	}
	if gConfig.MeekServerAddr == "" {
		log.Fatalf("-meekserver is a required argument, exiting now")
	}
	if gConfig.PsiphonServerAddr == "" {
		log.Fatalf("-psiphonserver is a required argument, exiting now")
	}
	if gConfig.SshSessionID == "" {
		log.Fatalf("-sessid is a required argument, exiting now")
	}
	if gConfig.MeekRelayAddr == "" && gConfig.FrontingDomain == "" {
		log.Fatalf("Either -meekrelay or -frontdomain has to be specified, exiting now")
	}
	if gConfig.FrontingDomain != "" && gConfig.FrontingHostname == "" {
		log.Fatalf("-fronthost is required with -frontdomain, exiting now")
	}

	gCookiePayload, err = makeCookie()
	if err != nil {
		log.Fatalf("Couldn't create encrypted payload: %s", err.Error())
	}

	ln, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		log.Fatalf("Couldn't start a server: %s", err.Error())
	}
	fmt.Fprintln(os.Stdout, "CMETHOD meek", ln.Addr())
	acceptLoop(ln)
}

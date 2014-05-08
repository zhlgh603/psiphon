package main

import (
	"bitbucket.org/psiphon/psiphon-circumvention-system/go/utils/crypto"
	"bufio"
	"bytes"
	"encoding/base64"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"log"
	"net"
	"net/http"
	"time"
)

import "git.torproject.org/pluggable-transports/goptlib.git"

const (
	// The size of the largest chunk of data we will read from the SOCKS
	// port before forwarding it in a request, and the maximum size of a
	// body we are willing to handle in a reply.
	maxPayloadLength = 0x10000
	// We must poll the server to see if it has anything to send; there is
	// no way for the server to push data back to us until we send an HTTP
	// request. When a timer expires, we send a request even if it has an
	// empty body. The interval starts at this value and then grows.
	initPollInterval = 100 * time.Millisecond
	// Maximum polling interval.
	maxPollInterval = 5 * time.Second
	// Geometric increase in the polling interval each time we fail to read
	// data.
	pollIntervalMultiplier = 1.5

	methodName = "meek"
)

// RequestInfo encapsulates all the configuration used for a request–response
// roundtrip, including variables that may come from SOCKS args or from the
// command line.
type RequestInfo struct {
	ClientPublicKeyBase64 string
	ObfuscationKeyword    string
	MeekServerAddr        string
	PsiphonServerAddr     string
	FrontingDomain        string
	FrontingHostname      string
	SshSessionID          string
	CookiePayload         string
}

// Do an HTTP roundtrip using the payload data in buf and the request metadata
// in info.
func roundTrip(buf []byte, info *RequestInfo) (*http.Response, error) {
	tr := http.DefaultTransport

	var URL string
	bFronting := false

	if info.FrontingDomain != "" {
		bFronting = true
		URL = fmt.Sprintf("https://%s/", info.FrontingDomain)
	} else {
		URL = fmt.Sprintf("http://%s/", info.MeekServerAddr)
	}

	req, err := http.NewRequest("POST", URL, bytes.NewReader(buf))
	if err != nil {
		return nil, err
	}
	if bFronting {
		req.Host = info.FrontingHostname
	}
	req.Header.Set("Content-Type", "application/octet-stream")

	cookie := &http.Cookie{Name: "key", Value: info.CookiePayload}
	req.AddCookie(cookie)
	return tr.RoundTrip(req)
}

// Send the data in buf to the remote URL, wait for a reply, and feed the reply
// body back into conn.
func sendRecv(buf []byte, conn net.Conn, info *RequestInfo) (int64, error) {
	resp, err := roundTrip(buf, info)
	if err != nil {
		return 0, err
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return 0, errors.New(fmt.Sprintf("status code was %d, not %d", resp.StatusCode, http.StatusOK))
	}

	return io.Copy(conn, io.LimitReader(resp.Body, maxPayloadLength))
}

// Repeatedly read from conn, issue HTTP requests, and write the responses back
// to conn.
func copyLoop(conn net.Conn, info *RequestInfo) error {
	var interval time.Duration

	ch := make(chan []byte)

	// Read from the Conn and send byte slices on the channel.
	go func() {
		var buf [maxPayloadLength]byte
		r := bufio.NewReader(conn)
		for {
			n, err := r.Read(buf[:])
			b := make([]byte, n)
			copy(b, buf[:n])
			// log.Printf("read from local: %q", b)
			ch <- b
			if err != nil {
				log.Printf("error reading from local: %s", err)
				break
			}
		}
		close(ch)
	}()

	interval = initPollInterval
loop:
	for {
		var buf []byte
		var ok bool

		// log.Printf("waiting up to %.2f s", interval.Seconds())
		// start := time.Now()
		select {
		case buf, ok = <-ch:
			if !ok {
				break loop
			}
			// log.Printf("read %d bytes from local after %.2f s", len(buf), time.Since(start).Seconds())
		case <-time.After(interval):
			// log.Printf("read nothing from local after %.2f s", time.Since(start).Seconds())
			buf = nil
		}

		nw, err := sendRecv(buf, conn, info)
		if err != nil {
			return err
		}
		/*
			if nw > 0 {
				log.Printf("got %d bytes from remote", nw)
			} else {
				log.Printf("got nothing from remote")
			}
		*/

		if nw > 0 || len(buf) > 0 {
			// If we sent or received anything, poll again
			// immediately.
			interval = 0
		} else if interval == 0 {
			// The first time we don't send or receive anything,
			// wait a while.
			interval = initPollInterval
		} else {
			// After that, wait a little longer.
			interval = time.Duration(float64(interval) * pollIntervalMultiplier)
		}
		if interval > maxPollInterval {
			interval = maxPollInterval
		}
	}

	return nil
}

func makeCookie(info *RequestInfo) (string, error) {
	var clientPublicKey, dummyKey [32]byte

	keydata, err := base64.StdEncoding.DecodeString(info.ClientPublicKeyBase64)
	if err != nil {
		return "", fmt.Errorf("error decoding info.ClientPublicKeyBase64: %s", err)
	}

	copy(clientPublicKey[:], keydata)
	cr := crypto.New(clientPublicKey, dummyKey)

	cookie := make(map[string]string)
	cookie["p"] = info.PsiphonServerAddr
	cookie["s"] = info.SshSessionID

	j, err := json.Marshal(cookie)
	if err != nil {
		return "", err
	}
	encrypted, err := cr.Encrypt(j)
	if err != nil {
		return "", err
	}
	obfuscated, err := cr.Obfuscate(encrypted, info.ObfuscationKeyword)
	if err != nil {
		return "", err
	}
	return base64.StdEncoding.EncodeToString(obfuscated), nil
}

// Callback for new SOCKS requests.
func handler(conn *pt.SocksConn) error {
	defer conn.Close()
	err := conn.Grant(&net.TCPAddr{IP: net.ParseIP("0.0.0.0"), Port: 0})
	if err != nil {
		return err
	}

	var info RequestInfo

	socksJSON := []byte(conn.Req.Username)
	err = json.Unmarshal(socksJSON, &info)
	if err != nil {
		return errors.New(fmt.Sprintf("couldn't decode SOCKS JSON payload: %s", err.Error()))
	}

	if info.ClientPublicKeyBase64 == "" {
		return errors.New("ClientPublicKeyBase64 is missing from SOCKS JSON payload")
	}
	if info.PsiphonServerAddr == "" {
		return errors.New("ClientPublicKeyBase64 is missing from SOCKS JSON payload")
	}

	if info.MeekServerAddr == "" && info.FrontingDomain == "" {
		return errors.New("Both MeekServerAddr & FrontingDomain are missing from SOCKS JSON payload")
	}
	if info.SshSessionID == "" {
		return errors.New("SshSessionID is missing from SOCKS JSON payload")
	}
	info.CookiePayload, err = makeCookie(&info)
	if err != nil {
		return errors.New(fmt.Sprintf("Couldn't create encrypted payload: %s", err.Error()))
	}

	return copyLoop(conn, &info)
}

func acceptLoop(ln *pt.SocksListener) error {
	defer ln.Close()
	for {
		conn, err := ln.AcceptSocks()
		if err != nil {
			log.Printf("error in AcceptSocks: %s", err)
			if e, ok := err.(net.Error); ok && !e.Temporary() {
				return err
			}
			continue
		}
		go func() {
			err := handler(conn)
			if err != nil {
				log.Printf("error in handling request: %s", err)
			}
		}()
	}
	return nil
}

func main() {
	var err error

	ln, err := pt.ListenSocks("tcp", "127.0.0.1:0")
	if err != nil {
		pt.CmethodError(methodName, err.Error())
		return
	}
	pt.Cmethod(methodName, ln.Version(), ln.Addr())
	acceptLoop(ln)
}

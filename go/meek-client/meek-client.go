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
	"math/rand"
	"net"
	"net/http"
	"net/url"
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
	ObfuscatedKeyword     string
	PsiphonServerAddr     string
	TargetAddr            string
	FrontingHostname      string
	SshSessionID          string
	PayloadCookie         *http.Cookie
	URL                   string
}

func randInt(min int, max int) int {
	rand.Seed(time.Now().UTC().UnixNano())
	return min + rand.Intn(max-min)
}

// Do an HTTP roundtrip using the payload data in buf and the request metadata
// in info.
func roundTrip(buf []byte, info *RequestInfo) (response *http.Response, err error) {
	tr := http.DefaultTransport
	req, err := http.NewRequest("POST", info.URL, bytes.NewReader(buf))
	if err != nil {
		return nil, err
	}
	if info.FrontingHostname != "" {
		req.Host = info.FrontingHostname
	}
	req.Header.Set("Content-Type", "application/octet-stream")

	req.AddCookie(info.PayloadCookie)

	// Retry loop, which assumes entire request failed (underlying
	// transport protocol such as SSH will fail if extra bytes are
	// replayed in either direction due to partial request success
	// followed by retry).
	// This retry mitigates intermittent failures between the client
	// and front/server.
	for i := 0; i <= 1; i++ {
		response, err = tr.RoundTrip(req)
		if err == nil {
			return
		}
		fmt.Printf("RoundTrip error: %s", err);
	}
	return
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

func makeCookie(info *RequestInfo) (*http.Cookie, error) {
	var clientPublicKey, dummyKey [32]byte

	keydata, err := base64.StdEncoding.DecodeString(info.ClientPublicKeyBase64)
	if err != nil {
		return nil, fmt.Errorf("error decoding info.ClientPublicKeyBase64: %s", err)
	}

	copy(clientPublicKey[:], keydata)
	cr := crypto.New(clientPublicKey, dummyKey)

	cookie := make(map[string]string)
	cookie["p"] = info.PsiphonServerAddr
	cookie["s"] = info.SshSessionID

	j, err := json.Marshal(cookie)
	if err != nil {
		return nil, err
	}
	encrypted, err := cr.Encrypt(j)
	if err != nil {
		return nil, err
	}
	obfuscated, err := cr.Obfuscate(encrypted, info.ObfuscatedKeyword)
	if err != nil {
		return nil, err
	}
	cookieValue := base64.StdEncoding.EncodeToString(obfuscated)
	cookieName := string(byte(randInt(65, 90)))
	return &http.Cookie{Name: cookieName, Value: cookieValue}, nil
}

// Callback for new SOCKS requests.
func handler(conn *pt.SocksConn) error {
	defer conn.Close()
	err := conn.Grant(&net.TCPAddr{IP: net.ParseIP("0.0.0.0"), Port: 0})
	if err != nil {
		return err
	}

	var info RequestInfo

	info.TargetAddr = conn.Req.Target
	info.ClientPublicKeyBase64, _ = conn.Req.Args.Get("cpubkey")
	info.PsiphonServerAddr, _ = conn.Req.Args.Get("pserver")
	info.FrontingHostname, _ = conn.Req.Args.Get("fhostname")
	info.SshSessionID, _ = conn.Req.Args.Get("sshid")
	info.ObfuscatedKeyword, _ = conn.Req.Args.Get("obfskey")

	if info.TargetAddr == "" {
		return errors.New("TargetAddr is missing from SOCKS request")
	}

	if info.ClientPublicKeyBase64 == "" {
		return errors.New("ClientPublicKeyBase64 is missing from SOCKS payload")
	}
	if info.PsiphonServerAddr == "" {
		return errors.New("PsiphonServerAddr is missing from SOCKS payload")
	}

	if info.SshSessionID == "" {
		return errors.New("SshSessionID is missing from SOCKS payload")
	}

	info.PayloadCookie, err = makeCookie(&info)
	if err != nil {
		return errors.New(fmt.Sprintf("Couldn't create encrypted payload: %s", err.Error()))
	}
	scheme := "http"

	if info.FrontingHostname != "" {
		scheme = "https"
	}

	info.URL = (&url.URL{
		Scheme: scheme,
		Host:   info.TargetAddr,
		Path:   "/",
	}).String()

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

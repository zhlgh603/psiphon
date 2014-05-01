package crypto

import "testing"
import "crypto/rand"
import mrand "math/rand"
import "code.google.com/p/go.crypto/nacl/box"
import "bytes"

func TestCrypto(t *testing.T) {
	obfuscationKeyword := "obfuscate"
	var dummyKey [32]byte
	for i := 0; i < 100; i++ {
		senderPublicKey, senderPrivateKey, _ := box.GenerateKey(rand.Reader)
		recipientPublicKey, recipientPrivateKey, _ := box.GenerateKey(rand.Reader)

		senderCrypto := New(obfuscationKeyword, *senderPublicKey, dummyKey)
		relayCrypto := New(obfuscationKeyword, *recipientPublicKey, *senderPrivateKey)
		recipientCrypto := New(obfuscationKeyword, dummyKey, *recipientPrivateKey)

		payload := make([]byte, mrand.Intn(250))
		rand.Read(payload)

		//sender
		encrypted, _ := senderCrypto.Encrypt(payload)

		//relay
		decrypted, _ := relayCrypto.Decrypt(encrypted)
		encrypted, _ = relayCrypto.Encrypt(decrypted)

		//recepient
		decrypted, _ = recipientCrypto.Decrypt(encrypted)
		if !bytes.Equal(payload, decrypted) {
			t.Fatalf("decrypted payload is not equal to the original!")
		}
	}
}

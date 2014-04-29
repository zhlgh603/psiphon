package crypto

import "testing"
import "crypto/rand"
import mrand "math/rand"
import "code.google.com/p/go.crypto/nacl/box"
import "bytes"

func TestCrypto(t *testing.T) {
    obfuscationKeyword  := "obfuscate"
    var dummyKey [32]byte
    for i := 0; i < 100; i++ {
        publicKey, privateKey, _ := box.GenerateKey(rand.Reader)

        senderCrypto := New(obfuscationKeyword, *publicKey, dummyKey)
        relayCrypto := New(obfuscationKeyword, *publicKey, *privateKey)
        recipientCrypto := New(obfuscationKeyword, dummyKey, *privateKey)
    
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

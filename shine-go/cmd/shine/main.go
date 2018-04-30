package main

import (
	"bytes"
	"net/url"

	"log"

	"flag"

	"github.com/eoscanada/eos-go"
	"github.com/eoscanada/shine/shine-go"
)

var accountNameString = flag.String("account-name", "shine", "Account name where to set shine code")
var privateKey = flag.String("private-key", "", "Private key")
var apiAddr = flag.String("api-addr", "http://localhost:8888", "RPC endpoint of the nodeos instance")
func main() {

	flag.Parse()
	apiAddrURL, err := url.Parse(*apiAddr)
	if err != nil {
		log.Fatalln("could not parse --api-addr:", err)
	}

	api := eos.New(apiAddrURL, bytes.Repeat([]byte{0}, 32))
	api.Debug = true
	accountName := eos.AccountName(*accountNameString)

	keyBag := eos.NewKeyBag()
	err = keyBag.Add(*privateKey)
	if err != nil {
		log.Fatal("key bag error: ", err)
	}

	api.SetSigner(keyBag)

	shine := shine.NewShine(api, &accountName)
	err = shine.HandleCommand("user.1", "post.1","recognize", "user.1", "msg.id.1", "bla bla bla")
	if err != nil {
		log.Fatal(err)
	}

}

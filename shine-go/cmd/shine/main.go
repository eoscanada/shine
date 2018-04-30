package main

import (
	"bytes"
	"net/url"

	"log"

	"flag"

	"github.com/eoscanada/eos-go"
	shine2 "github.com/eoscanada/shine"
)

var privateKey = flag.String("private-key", "", "Private key")

func main() {

	flag.Parse()

	api := eos.New(&url.URL{Scheme: "http", Host: "Charless-MacBook-Pro-2.local:8888"}, bytes.Repeat([]byte{0}, 32))
	api.Debug = true
	accountName := eos.AccountName("shine")

	keyBag := eos.NewKeyBag()
	err := keyBag.Add(*privateKey)
	if err != nil {
		log.Fatal("key bag error: ", err)
	}

	api.SetSigner(keyBag)

	shine := shine2.NewShine(api, &accountName)
	err = shine.HandleCommand("user.1", "recognize", "user.1", "msg.id.1", "bla bla bla")
	if err != nil {
		log.Fatal(err)
	}

}

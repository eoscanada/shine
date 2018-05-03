# shine

**Install and boot a node from** github.com/eoscanada/eos-bios

####Clone and Run Shine**
**Get Source**
* git clone git@github.com:eoscanada/shine.git
* cd $GOPATH/src/github.com/eoscanada/shine/shine-go

**Install Shine contract on node**
* go install github.com/eoscanada/shine/shine-go/cmd/setcode
* setcode --account-name shine --private-key [node private key] --shine-account-public-key [public key] --shine-code-path $GOPATH/src/github.com/eoscanada/shine/shine-go/cmd/setcode

**Run Shine**
* go install github.com/eoscanada/shine/shine-go/cmd/shine
* shine --private-key [node private key]

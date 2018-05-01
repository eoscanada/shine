package shine

import (
	"crypto/sha256"

	"io"

	"fmt"

	"github.com/eoscanada/eos-go"
	"encoding/json"
)

type Shine struct {
	accountName eos.AccountName
	api         eos.API
}

func NewShine(api *eos.API, accountName *eos.AccountName) *Shine {
	return &Shine{
		accountName: *accountName,
		api:         *api,
	}
}

func hash(s string) eos.SHA256Bytes {
	h := sha256.New()
	io.WriteString(h, s)

	return eos.SHA256Bytes(h.Sum(nil))

}

func (s *Shine) HandleCommand(fromUser, post string, command string,  params ...string) error {
	var action *eos.Action

	switch command {
	case "recognize":
		action = newAddPraise(s.accountName, hash(post), hash(fromUser), hash(params[0]), params[1])
	default:
		return fmt.Errorf("unknown command [%s]", command)
	}

	data, err := json.Marshal(action)
	fmt.Println("Data in json: ", string(data))

	actionResp, err := s.api.SignPushActions(action)
	if err != nil {
		return err
	}
	fmt.Println(actionResp)

	return nil
}

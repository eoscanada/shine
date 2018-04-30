package shine

import (
	"crypto/sha256"

	"io"

	"fmt"

	"github.com/eoscanada/eos-go"
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

func (s *Shine) HandleCommand(fromUser, command string, msgID string, params ...string) error {
	var action *eos.Action

	switch command {
	case "recognize":
		action = newAddPraise(s.accountName, hash(fromUser), hash(params[0]), params[1], hash(msgID))
	default:
		return fmt.Errorf("unknown command [%s]", command)
	}

	actionResp, err := s.api.SignPushActions(action)
	if err != nil {
		return err
	}
	fmt.Println(actionResp)

	return nil
}

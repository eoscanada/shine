package shine

import (
	"crypto/sha256"
	"strings"

	"io"

	"fmt"

	"encoding/json"

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

func (s *Shine) HandleCommand(fromUser, post string, command string) error {
	var action *eos.Action

	commandParts := strings.Split(command, " ")
	if len(commandParts) < 2 {
		return fmt.Errorf("Command no clear [%s]", command)
	}

	switch commandParts[0] {
	case "/recognize":
		praisee := commandParts[1]
		memo := strings.Join(commandParts[2:], " ")
		action = newAddPraise(s.accountName, hash(post), hash(fromUser), hash(praisee), memo)
	case "/upvote":
		post := commandParts[1]
		action = newAddVote(s.accountName, hash(post), hash(fromUser))
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

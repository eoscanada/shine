package shine

import (
	"crypto/sha256"
	"strings"

	"fmt"

	"github.com/eoscanada/eos-go"
)

type Shine struct {
	accountName eos.AccountName
	api         *eos.API
}

func NewShine(api *eos.API, accountName eos.AccountName) *Shine {
	return &Shine{
		accountName: accountName,
		api:         api,
	}
}

func hash(s string) eos.SHA256Bytes {
	h := sha256.New()
	_, _ = h.Write([]byte(s))
	return eos.SHA256Bytes(h.Sum(nil))
}

func (s *Shine) HandleCommand(fromUser, context string, command string) error {
	var action *eos.Action

	commandParts := strings.Split(command, " ")
	if len(commandParts) < 1 {
		return fmt.Errorf("Command not clear [%s]", command)
	}

	switch commandParts[0] {
	case "/recognize":
		// TODO: make sure we have enough options..
		praisee := commandParts[1]
		memo := strings.Join(commandParts[2:], " ")
		action = newAddPraise(s.accountName, hash(context), hash(fromUser), hash(praisee), memo)
	case "/upvote":
		action = newAddVote(s.accountName, hash(context), hash(fromUser))
	case "/register":
		// TODO: make sure we have commandParts[1] available
		email := context
		action = newBindMember(s.accountName, hash(email), eos.AccountName(commandParts[1]))
	default:
		return fmt.Errorf("unknown command [%s]", command)
	}

	// data, err := json.Marshal(action)
	// fmt.Println("Data in json: ", string(data))

	actionResp, err := s.api.SignPushActions(action)
	if err != nil {
		return err
	}
	fmt.Println(actionResp)

	return nil
}

package shine

import (
	"github.com/eoscanada/eos-go"
)

// addPraise matches the ABI for `addpraise`
type addPraise struct {
	Author  eos.SHA256Bytes `json:"author"`
	Post    eos.SHA256Bytes `json:"post"`
	Praisee eos.SHA256Bytes `json:"praisee"`
	Memo    string          `json:"memo"`
}

// addVote matches the ABI for `addvote`
type addVote struct {
	Voter eos.SHA256Bytes `json:"voter"`
	Post  eos.SHA256Bytes `json:"post"`
}

// bindMember matches the ABI for `bindmember`
type bindMember struct {
	Member  eos.SHA256Bytes `json:"member"`
	Account eos.AccountName `json:"account"`
}

func newAddPraise(contract eos.AccountName, post eos.SHA256Bytes, author eos.SHA256Bytes, praisee eos.SHA256Bytes, memo string) *eos.Action {

	p := addPraise{
		Author:  author,
		Post:    post,
		Praisee: praisee,
		Memo:    memo,
	}

	a := &eos.Action{
		Account: contract,
		Name:    eos.ActionName("addpraise"),
		Authorization: []eos.PermissionLevel{
			{Actor: contract, Permission: eos.PermissionName("active")},
		},
		Data: eos.NewActionData(p),
	}

	return a
}

func newAddVote(contract eos.AccountName, post eos.SHA256Bytes, voter eos.SHA256Bytes) *eos.Action {

	p := addVote{
		Voter: voter,
		Post:  post,
	}

	a := &eos.Action{
		Account: contract,
		Name:    eos.ActionName("addvote"),
		Authorization: []eos.PermissionLevel{
			{Actor: contract, Permission: eos.PermissionName("active")},
		},
		Data: eos.NewActionData(p),
	}

	return a
}

func newBindMember(contract eos.AccountName, member eos.SHA256Bytes, toAccount eos.AccountName) *eos.Action {
	return &eos.Action{
		Account: contract,
		Name:    eos.ActionName("bindmember"),
		Authorization: []eos.PermissionLevel{
			{Actor: contract, Permission: eos.PermissionName("active")},
		},
		Data: eos.NewActionData(bindMember{
			Member:  member,
			Account: toAccount,
		}),
	}
}

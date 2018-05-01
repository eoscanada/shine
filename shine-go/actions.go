package shine

import (
	"github.com/eoscanada/eos-go"
)

type addPraise struct {
	Author  eos.SHA256Bytes
	Post    eos.SHA256Bytes
	Praisee eos.SHA256Bytes
	Memo    string
}

type addVote struct {
	Voter eos.SHA256Bytes
	Post  eos.SHA256Bytes
}

func newAddPraise(accountName eos.AccountName, post eos.SHA256Bytes, author eos.SHA256Bytes, praisee eos.SHA256Bytes, memo string) *eos.Action {

	p := addPraise{
		Author:  author,
		Post:    post,
		Praisee: praisee,
		Memo:    memo,
	}

	a := &eos.Action{
		Account: accountName,
		Name:    eos.ActionName("addpraise"),
		Authorization: []eos.PermissionLevel{
			{Actor: accountName, Permission: eos.PermissionName("active")},
		},
		Data: eos.NewActionData(p),
	}

	return a
}

func newAddVote(accountName eos.AccountName, post eos.SHA256Bytes, voter eos.SHA256Bytes) *eos.Action {

	p := addVote{
		Voter: voter,
		Post:  post,
	}

	a := &eos.Action{
		Account: accountName,
		Name:    eos.ActionName("addvote"),
		Authorization: []eos.PermissionLevel{
			{Actor: accountName, Permission: eos.PermissionName("active")},
		},
		Data: eos.NewActionData(p),
	}

	return a
}

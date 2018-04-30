package shine

import "github.com/eoscanada/eos-go"

type addPraise struct {
	Author  eos.SHA256Bytes
	Praisee eos.SHA256Bytes
	msgID   eos.SHA256Bytes
	Memo    string
}

func newAddPraise(accountName eos.AccountName, author eos.SHA256Bytes, praisee eos.SHA256Bytes, memo string, msgID eos.SHA256Bytes) *eos.Action {
	a := &eos.Action{
		Account: accountName,
		Name:    eos.ActionName("addpraise"),
		Authorization: []eos.PermissionLevel{
			{Actor: accountName, Permission: eos.PermissionName("active")},
		},
		Data: eos.NewActionData(
			addPraise{
				Author:  author,
				Praisee: praisee,
				msgID:   msgID,
				Memo:    memo,
			},
		),
	}
	return a
}

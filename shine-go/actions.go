package shine

import "github.com/eoscanada/eos-go"

type addPraise struct {
	Author  eos.SHA256Bytes
	Post    eos.SHA256Bytes
	Praisee eos.SHA256Bytes
	Memo    string
}

func newAddPraise(accountName eos.AccountName, post eos.SHA256Bytes, author eos.SHA256Bytes, praisee eos.SHA256Bytes, memo string) *eos.Action {
	a := &eos.Action{
		Account: accountName,
		Name:    eos.ActionName("addpraise"),
		Authorization: []eos.PermissionLevel{
			{Actor: accountName, Permission: eos.PermissionName("active")},
		},
		Data: eos.NewActionData(
			addPraise{
				Author:  author,
				Post:    post,
				Praisee: praisee,
				Memo:    memo,
			},
		),
	}
	return a
}

package cmd

import (
	"fmt"

	"github.com/eoscanada/eos-go"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
)

func init() {
	postCmd.Flags().StringP("to","t","", "Shine to")
	postCmd.Flags().StringP("memo","m","", "Shine memo")
}

func postRunE(cmd *cobra.Command, args []string) (err error) {
	to := viper.GetString("to")
	memo := viper.GetString("memo")
	fmt.Println("to: ", to)
	fmt.Println("memo: ", memo)
	//chainID := make([]byte, 32, 32)
	//api := eos.New("http://localhost:8888", chainID)
	//
	//api.SetSigner(eos.NewWalletSigner(eos.New("http://localhost:6667", chainID), "default"))
	//
	//_, err := api.SignPushActions(
	//	NewPost(fromAccount, args[0], args[1]),
	//)
	//if err != nil {
	//	log.Fatalln(err)
	//}
	//fmt.Println("post called")
	return nil

}

func NewPost(from, to, memo string) *eos.Action {
	return &eos.Action{
		Account: eos.AccountName("shine"),
		Name:    eos.ActionName("post"),
		Authorization: []eos.PermissionLevel{
			{Actor: eos.AccountName(from), Permission: eos.PermissionName("active")},
		},
		ActionData: eos.NewActionData(Post{
			From: eos.AccountName(from),
			To:   eos.AccountName(to),
			Memo: memo,
		}),
	}
}

type Post struct {
	From eos.AccountName `json:"from"`
	To   eos.AccountName `json:"to"`
	Memo string          `json:"memo"`
}

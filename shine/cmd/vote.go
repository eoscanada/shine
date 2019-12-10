package cmd

import (
	"fmt"

	"github.com/eoscanada/eos-go"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
)

func init() {
	voteCmd.Flags().StringP("post", "p","", "Vote for a specific post id")
}

func voteRunE(cmd *cobra.Command, args []string) (err error) {
	postId := viper.GetString("post")
	fmt.Println("post_id: ", postId)

	//chainID := make([]byte, 32, 32)
	//api := eos.New("http://localhost:8888", chainID)
	//
	//api.SetSigner(eos.NewWalletSigner(eos.New("http://localhost:6667", chainID), "default"))
	//
	//_, err := api.SignPushActions(
	//	NewVote(fromAccount, votePostID),
	//)
	//if err != nil {
	//	log.Fatalln(err)
	//}
	//fmt.Println("vote called")
	return nil

}

func NewVote(from string, postID int) *eos.Action {
	return &eos.Action{
		Account: eos.AccountName("shine"),
		Name:    eos.ActionName("vote"),
		Authorization: []eos.PermissionLevel{
			{Actor: eos.AccountName(from), Permission: eos.PermissionName("active")},
		},
		ActionData: eos.NewActionData(Vote{
			Voter: eos.AccountName(from),
			PostID: uint64(postID),
		}),
	}
}

type Vote struct {
	Voter  eos.AccountName `json:"voter"`
	PostID uint64          `json:"post_id"`
}

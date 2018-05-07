// Copyright © 2018 NAME HERE <EMAIL ADDRESS>
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package cmd

import (
	"encoding/json"
	"fmt"
	"log"
	"os"

	eos "github.com/eoscanada/eos-go"
	"github.com/spf13/cobra"
)

var postID int

// voteCmd represents the vote command
var voteCmd = &cobra.Command{
	Use:   "vote",
	Short: "Vote for a post",
	Long:  `vote --from [my account] -p [post_id]`,
	Run: func(cmd *cobra.Command, args []string) {
		chainID := make([]byte, 32, 32)

		api := eos.New(nodeURL, chainID)
		api.SetSigner(eos.NewWalletSigner(eos.New(walletURL, chainID), "default"))

		action := NewVote(fromAccount, postID)
		json.NewEncoder(os.Stdout).Encode(action)
		if _, err := api.SignPushActions(action); err != nil {
			log.Fatalln(err)
		}
		fmt.Println("Done")
	},
}

func init() {
	RootCmd.AddCommand(voteCmd)
	voteCmd.Flags().IntVarP(&postID, "post_id", "p", 0, "Specify the post-id")
}

func NewVote(from string, postID int) *eos.Action {
	return &eos.Action{
		Account: eos.AccountName("shine"),
		Name:    eos.ActionName("vote"),
		Authorization: []eos.PermissionLevel{
			{Actor: eos.AccountName(from), Permission: eos.PermissionName("active")},
		},
		Data: eos.NewActionData(Vote{
			From:   eos.AccountName(from),
			PostID: uint64(postID),
		}),
	}
}

type Vote struct {
	From   eos.AccountName `json:"from"`
	PostID uint64          `json:"post_id"`
}

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
	"fmt"
	"os"

	"github.com/spf13/cobra"
)

var fromAccount string
var nodeURL string
var walletURL string

// RootCmd represents the base command when called without any subcommands
var RootCmd = &cobra.Command{
	Use:   "shine",
	Short: "An employee rewards system",
}

func Execute() {
	if err := RootCmd.Execute(); err != nil {
		fmt.Println(err)
		os.Exit(1)
	}
}

func init() {
	RootCmd.PersistentFlags().StringVarP(&fromAccount, "from", "f", "", "From account name")
	RootCmd.PersistentFlags().StringVar(&nodeURL, "url", "http://localhost:8888", "EOSIO node url")
	RootCmd.PersistentFlags().StringVar(&walletURL, "wallet-url", "http://localhost:6667", "EOSIO keosd wallet")
}

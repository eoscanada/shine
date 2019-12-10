package cmd

import (
	"github.com/spf13/cobra"
)


var registerCmd = &cobra.Command{
	Use:                        "register -f [account] [key]",
	Args:                       nil,
	Run: func(cmd *cobra.Command, args []string) {

	},
}
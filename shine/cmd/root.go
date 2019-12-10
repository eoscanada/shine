package cmd

import (
	"fmt"
	"os"

	"github.com/mitchellh/go-homedir"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
)

var cfgFile string
var rootCmd = &cobra.Command{ Use: "shine", Short: "Runs the shine client"}
var voteCmd = &cobra.Command{ Use: "vote", Short: "Vote for a post", Long: "shine post -f joe [to] [memo]", RunE: voteRunE}
var postCmd = &cobra.Command{ Use: "post", Short: "Post for a post", Long: "shine vote -f joe -p post_id", RunE: postRunE}
//var registerCmd = &cobra.Command{ Use: "register", Short: "Register"}

func init() {
	cobra.OnInitialize(initConfig)
	rootCmd.PersistentFlags().StringVar(&cfgFile, "config", "", "config file (default is $HOME/.shine.yaml)")
	rootCmd.AddCommand(voteCmd)
	rootCmd.AddCommand(postCmd)
	rootCmd.AddCommand(registerCmd)
}

// RootCmd represents the base command when called without any subcommands


// Execute adds all child commands to the root command and sets flags appropriately.
// This is called by main.main(). It only needs to happen once to the rootCmd.
func Execute() {
	if err := rootCmd.Execute(); err != nil {
		fmt.Println(err)
		os.Exit(1)
	}
}


func initConfig() {
	if cfgFile != "" {
		// Use config file from the flag.
		viper.SetConfigFile(cfgFile)
	} else {
		// Find home directory.
		home, err := homedir.Dir()
		if err != nil {
			fmt.Println(err)
			os.Exit(1)
		}

		// Search config in home directory with name
		// ".shine" (without extension).
		viper.AddConfigPath(home)
		viper.SetConfigName(".shine")
	}

	viper.AutomaticEnv()

	if err := viper.ReadInConfig(); err == nil {
		fmt.Println("Using config file:", viper.ConfigFileUsed())
	}
}

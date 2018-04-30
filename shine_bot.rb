#!/usr/bin/env ruby

require 'digest'
require 'json'
require 'open3'
require 'tty-prompt'

EOS_NAME_REGEX = /^[\.1-5a-z]{1,12}[\.a-p]?$/

def main(arguments)
  prompt = TTY::Prompt.new
  context = {}

  context[:arguments] = arguments
  context[:quick_run] = arguments_any?(arguments, '-q', '--quick')
  context[:contract] = ask_contract(prompt)
  context[:action] = ask_action(prompt, context)

  send("perform_#{context[:action]}".to_sym, prompt, context)
end

def ask_contract(prompt)
  default = ENV['SHINE_BOT_CONTRACT']
  return default if default

  prompt.ask('Contract:') do |question|
    question.required true
    question.default default
  end
end

def ask_action(prompt, context)
  default = extract_default_action(context[:arguments])
  return default if default

  prompt.select('Action') do |menu|
    menu.choice 'Praise', :praise
    menu.choice 'Vote', :vote
    menu.choice 'Bind Member', :bind_member
    menu.choice 'Unbind Member', :unbind_member
    menu.choice 'Reset', :reset
    menu.choice 'Clear', :clear
    menu.choice 'Scenario', :scenario
  end
end

def perform_scenario(_prompt, context)
  contract = context[:contract]

  praises = [
    { post: "0", author: 'matt', praisee: 'eve' },
    { post: "1", author: 'matt', praisee: 'eve' },
    { post: "2", author: 'evan', praisee: 'eve' },
    { post: "3", author: 'eve', praisee: 'matt' },
    { post: "4", author: 'mike', praisee: 'matt' },
    { post: "5", author: 'mike', praisee: 'eve' },
    { post: "6", author: 'mike', praisee: 'evan' },
  ]

  votes = [
    { voter: 'evan', post: '0' },
    { voter: 'evan', post: '1' },
    { voter: 'mike', post: '1' },
    { voter: 'mike', post: '3' },
    { voter: 'mike', post: '3' },
    { voter: 'evan', post: '3' },
    { voter: 'eve', post: '4' },
    { voter: 'matt', post: '5' },
    { voter: 'matt', post: '6' },
    { voter: 'eve', post: '6' },
  ]

  praises.each do |praise|
    execute_praise(contract, {
      post: sha256(praise[:post]),
      author: sha256(praise[:author]),
      praisee: sha256(praise[:praisee]),
      memo: "#{praise[:author]} -> #{praise[:praisee]}",
    })
  end

  votes.each do |vote|
    execute_vote(contract, {
      post: sha256(vote[:post]),
      voter: sha256(vote[:voter]),
    })
  end
end

def perform_praise(prompt, context)
  praise = {
    post: ask_post(prompt, context, 'Post:', ENV['SHINE_BOT_POST']),
    author: ask_member_id(prompt, context, 'Author:', ENV['SHINE_BOT_AUTHOR']),
    praisee: ask_member_id(prompt, context, 'Praisee:', ENV['SHINE_BOT_PRAISEE']),
    memo: ask_memo(prompt, context),
  }

  puts 'Data:'
  puts JSON.pretty_generate(praise)
  puts ''

  puts execute_praise(context[:contract], praise)
end

def execute_praise(contract, praise)
  execute_transaction(contract, 'addpraise', JSON.generate(praise))
end

def ask_memo(prompt, context)
  default = ENV['SHINE_BOT_MEMO'] || ''
  return default if default && context[:quick_run]

  prompt.ask('Memo:') do |question|
    question.required false
    question.default default
  end
end

def perform_vote(prompt, context)
  vote = {
    post: ask_post(prompt, context, 'Post: ', ENV['SHINE_BOT_POST']),
    voter: ask_member_id(prompt, context, 'Voter: ', ENV['SHINE_BOT_VOTER']),
  }

  puts 'Data:'
  puts JSON.pretty_generate(vote)
  puts ''

  puts execute_vote(context[:contract], vote)
end

def execute_vote(contract, vote)
  execute_transaction(contract, 'addvote', JSON.generate(vote))
end

def ask_praise_id(prompt, context)
  default = ENV['SHINE_BOT_PRAISE_ID']
  return default if default && context[:quick_run]

  prompt.ask('Praise ID:') do |question|
    question.required true
    question.validate /[0-9]+/
    question.convert -> (input) { input.to_i }
  end
end

def perform_bind_member(prompt, context)
  puts execute_transaction(context[:contract], 'bindmember', JSON.generate({
    member: ask_member_id(prompt, context, 'Member:', ENV['SHINE_BOT_BIND_MEMBER']),
    account: ask_account(prompt, context, 'Account:', ENV['SHINE_BOT_BIND_ACCOUNT']),
  }))
end

def perform_unbind_member(prompt, context)
  puts execute_transaction(context[:contract], 'unbindmember', JSON.generate({
    member: ask_member_id(prompt, context, 'Member:', ENV['SHINE_BOT_UNBIND_MEMBER']),
  }))
end

def perform_reset(_prompt, context)
  puts execute_transaction(context[:contract], 'reset', JSON.generate({ any: 0 }))
end

def perform_clear(_prompt, context)
  puts execute_transaction(context[:contract], 'clear', JSON.generate({ any: 0 }))
end

def ask_account(prompt, context, text, default = nil)
  return default if default && context[:quick_run]

  prompt.ask(text) do |question|
    question.required true
    question.default default
    question.validate EOS_NAME_REGEX
  end
end

def ask_post(prompt, context, text, default)
  return sha256(default) if default && context[:quick_run]

  prompt.ask(text) do |question|
    question.required true
    question.default default
    question.convert -> (input) { sha256(input) }
  end
end

def ask_member_id(prompt, context, text, default)
  return sha256(default) if default && context[:quick_run]

  prompt.ask(text) do |question|
    question.required true
    question.default default
    question.convert -> (input) { sha256(input) }
  end
end

def arguments_any?(arguments, *flags)
  arguments.any? { |input| flags.include? input }
end

def extract_default_action(arguments)
  return :praise if arguments_any?(arguments, '--praise')
  return :vote if arguments_any?(arguments, '--vote')
  return :bind_member if arguments_any?(arguments, '--bind-member')
  return :unbind_member if arguments_any?(arguments, '--unbind-member')
  return :reset if arguments_any?(arguments, '--reset')
  return :clear if arguments_any?(arguments, '--clear')
  return :scenario if arguments_any?(arguments, '--scenario')

  ENV['SHINE_BOT_ACTION'].nil? ? nil : ENV['SHINE_BOT_ACTION'].to_sym
end

def execute_transaction(contract, action, data)
  execute_cleos([
    'push',
    'action',
    contract,
    action,
    data,
    '--force-unique',
    '--permission',
    "#{contract}@active"
  ])
end

def execute_cleos(arguments)
  wallet_host = ENV['SHINE_BOT_WALLET_HOST']
  wallet_port = ENV['SHINE_BOT_WALLET_PORT']

  options = []
  options << '--wallet-port' << wallet_port if wallet_port
  options << '--wallet-host' << wallet_host if wallet_host

  stdout, stderr, status = Open3.capture3('cleos', *options, *arguments)
  unless status.success?
    puts stderr
    exit(status.exitstatus)
  end

  stdout
end

def sha256(input)
  Digest::SHA256.hexdigest(input)
end

main(ARGV)

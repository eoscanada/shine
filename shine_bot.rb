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
  context[:contract] = ask_contract(prompt, context)
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
    menu.choice 'Rewards', :rewards
    menu.choice 'Scenario', :scenario
  end
end

def perform_scenario(_prompt, context)
  contract = context[:contract]

  praises = [
    { author: 'matt', praisee: 'eve' },
    { author: 'matt', praisee: 'eve' },
    { author: 'evan', praisee: 'eve' },
    { author: 'eve', praisee: 'matt' },
    { author: 'mike', praisee: 'matt' },
    { author: 'mike', praisee: 'eve' },
    { author: 'mike', praisee: 'evan' },
  ]

  votes = [
    { voter: 'evan', praise_id: 0 },
    { voter: 'evan', praise_id: 1 },
    { voter: 'mike', praise_id: 1 },
    { voter: 'mike', praise_id: 3 },
    { voter: 'mike', praise_id: 3 },
    { voter: 'evan', praise_id: 3 },
    { voter: 'eve', praise_id: 4 },
    { voter: 'matt', praise_id: 5 },
    { voter: 'matt', praise_id: 6 },
    { voter: 'eve', praise_id: 6 },
  ]

  praises.each do |praise|
    execute_praise(contract, {
      author: sha256(praise[:author]),
      praisee: sha256(praise[:praisee]),
      memo: "#{praise[:author]} -> #{praise[:praisee]}",
    })

    sleep(1)
  end

  votes.each do |vote|
    execute_vote(contract, {
      praise_id: vote[:praise_id],
      voter: sha256(vote[:voter]),
    })

    sleep(1)
  end
end

def perform_praise(prompt, context)
  praise = {
    author: ask_member_id(prompt, 'Author:', ENV['SHINE_BOT_AUTHOR'], context),
    praisee: ask_member_id(prompt, 'Praisee:', ENV['SHINE_BOT_PRAISEE'], context),
    memo: ask_memo(prompt, context),
  }

  puts 'Data:'
  puts JSON.pretty_generate(praise)
  puts ''

  puts execute_praise(context[:contract], praise)
end

def execute_praise(contract, praise)
  action = 'addpraise'

  execute_cleos([
    'push',
    'action',
    contract,
    action,
    JSON.generate(praise),
    '--permission',
    "#{contract}@active"
  ])
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
    praise_id: ask_praise_id(prompt),
    voter: ask_member_id(prompt, 'Voter:', ENV['SHINE_BOT_VOTER'], context),
  }

  puts 'Data:'
  puts JSON.pretty_generate(vote)
  puts ''

  puts execute_vote(context[:contract], vote)
end

def execute_vote(contract, vote)
  action = 'addvote'

  execute_cleos([
    'push',
    'action',
    contract,
    action,
    JSON.generate(vote),
    '--permission',
    "#{contract}@active"
  ])
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

def perform_rewards(prompt, context)
  action = 'calcrewards'
  contract = context[:contract]
  data = {
    pot: "500 EOS",
  }

  puts execute_cleos([
    'push',
    'action',
    contract,
    action,
    JSON.generate(data),
    '--permission',
    "#{contract}@active"
  ])
end

def ask_member_id(prompt, text, default, context)
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
  return :praise if arguments_any?(arguments, '--praise', '-p')
  return :vote if arguments_any?(arguments, '--vote', '-v')
  return :rewards if arguments_any?(arguments, '--rewards', '-r')
  return :scenario if arguments_any?(arguments, '--scenario')

  ENV['SHINE_BOT_ACTION'].nil? ? :praise : ENV['SHINE_BOT_ACTION'].to_sym
end

def execute_cleos(arguments)
  stdout, stderr, status = Open3.capture3('cleos', *arguments)
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

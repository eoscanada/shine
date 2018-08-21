## EOS Shine Smart Contract

This document describes various aspects of the EOS Shine smart contract
implementation ranging from architecture, building, troubleshooting, etc.

Moreover, this document also contains various information about EOS
smart contract in general, mainly aimed at the developer audience.

#### Actions

* `post` - Create a post in the blockchain.
* `vote` - Vote for a given praise in the blockchain.
* `reset` - Reset all collected stats to initial state.

### Development

#### Compilation

##### Docker

The easiest way to compile is by using EOS Canada `gcr.io/eoscanada-public/eosio-wasmsdk`
Docker image.

While at the root of this project:

```
cd contract
docker run --rm -it -v `pwd`:/shine -w /shine gcr.io/eoscanada-public/eosio-wasmsdk:v1.1.1 bash build.sh
```

You will find the compiled `shine.abi` and `shine.wasm` in the `build` directory.

##### Locally

Ensure that you have `https://github.com/EOSIO/eosio.wasmsdk` compiled locally.

While at the root of this project:

```
./build.sh
```

You will find the compiled `shine.abi` and `shine.wasm` in the `build` directory.

#### Installation

While at the root of this project:

```
cd contract
cleos set contract shine build build/shine.wasm build/shine.abi
```

#### Testing

First, install [eos-bios](https://github.com/eoscanada/eos-bios) then follow
the [local-development-environment](https://github.com/eoscanada/eos-bios#local-development-environment)
instructions to start a local `nodeos` for development.

Ensure that your `boot_sequence.yaml` contains the following content: (the content must comes
before the `Replacing eosio account from eosio.bios contract to eosio.system` block):

```
- op: system.newaccount
  label: Create account eosioforum
  data:
    creator: eosio
    new_account: shine
    pubkey: EOS5MHPYyhjBjnQZejzZHqHewPWhGTfQWSVTWYEhDmJu4SXkzgweP

- op: system.newaccount
  label: Create user account for shine
  data:
    creator: eosio
    new_account: matt
    pubkey: EOS5MHPYyhjBjnQZejzZHqHewPWhGTfQWSVTWYEhDmJu4SXkzgweP

- op: system.newaccount
  label: Create user account for shine
  data:
    creator: eosio
    new_account: eve
    pubkey: EOS5MHPYyhjBjnQZejzZHqHewPWhGTfQWSVTWYEhDmJu4SXkzgweP

- op: system.newaccount
  label: Create user account for shine
  data:
    creator: eosio
    new_account: evan
    pubkey: EOS5MHPYyhjBjnQZejzZHqHewPWhGTfQWSVTWYEhDmJu4SXkzgweP

- op: system.newaccount
  label: Create user account for shine
  data:
    creator: eosio
    new_account: mike
    pubkey: EOS5MHPYyhjBjnQZejzZHqHewPWhGTfQWSVTWYEhDmJu4SXkzgweP
```

Import the following private key in your wallet: `5JpjqdhVCQTegTjrLtCSXHce7c9M8w7EXYZS7xC13jVFF4Phcrx`
(this is the private key for the public one `EOS5MHPYyhjBjnQZejzZHqHewPWhGTfQWSVTWYEhDmJu4SXkzgweP`, of
course, you can use your own pair if you change the public key part in the boot sequence).

To make it easier to develop, the `shine_bot.rb` is a small command line
utility to interact more easily with the smart contract.

First, it's possible to run all command in a quick way by passing the flag
`-q` to the CLI tool. This will not ask for question for which a default value
has been provided by via environment variables. Environment variables for the
various questions:

* SHINE_BOT_CONTRACT - Default value of the contract id
* SHINE_BOT_ACTION - Default action to execute (use in preference CLI arguments instead of this one)
* SHINE_BOT_AUTHOR - Default member that posted a praise
* SHINE_BOT_PRAISEE - Default member praised in a praise
* SHINE_BOT_VOTER - Default member that voted for a praise
* SHINE_BOT_PRAISE_ID - Default praise id to use when voting
* SHINE_BOT_REWARD_POT - Default EOS reward amount (in form `#.XXXX EOS`)

Action can also be specified via the CLI, this is the preferred way of
performing an action.

Without any argument, contract and action will be asked via the terminal.
So it's recommended to export `SHINE_BOT_CONTRACT` so the question is not
ask at all (even without quick mode flag `-q`). With the right action flag
(or environment variable), the contract and action will not be asked
and default value will be used.

When entering member id in questions, you can use any string value. The input
defined will then be hashed using `sha256` and this will result in
the `member_id` being pass to the contract.

Possible usage:

* `./shine_bot.rb --post` - Perform a post (from, to, memo)
* `./shine_bot.rb --vote` - Perform a vote (praise_id, voter)
* `./shine_bot.rb --reset` - Reset the all state to initial values
* `./shine_bot.rb --scenario` - Perform a scenario adding 6 posts and 10 votes.

By using the scenario, you will automatically add a bunch of praises and
votes so it's easier to test your smart contract code and changes afterward.

### EOS smart contract

#### References

This section aims at providing links to documentation related to some concept
of building smart contract for the EOS blockchain.

* `eosio::multi_index`

  Currently, the EOS `multi_index` is poorly documented. Suggestion is to
  check out [boost::multi_index](https://www.boost.org/doc/libs/1_66_0/libs/multi_index/doc/tutorial)
  documentation since `eosio::multi_index` API is heavily based on it.

  Otherwise, looking at the [eosio::multi_index source](https://github.com/EOSIO/eos/blob/master/contracts/eosiolib/multi_index.hpp).

#### Table Update

Updating an existing table is problematic right now as it appears (unconfirmed)
that serialization of the data is performed in a sequential binary way. This
causes problem when serialization/deserialization back the new table structure.

Indeed, probably that under the cover, field names are not included in the
serialized form. Deserialization then expects a certain layout to allow
correct deserialization.

As such, when a field that is being added to an existing table will likely
causing read error since not enough bytes will be available to fill the
structure completely. This results in the following error message when
trying to post an action that interacts with this table:

```
Error 3030000: transaction validation exception
Ensure that your transaction satisfy the contract's constraint!
Error Details:
condition: assertion failed: read
```

When a field is instead removed, then there it's likely that there will
not be any runtime error as bytes will simply be re-interpreted as whatever
the target type expect. This will causes logical error since the data held
by the struct will be all scrambled (for example, a count value of `1` could
become `7956005066189463816` since 8 random bytes will be re-interpreted to
a `uint64_t` which might give anything).

#### Set Contract Code Update

There is some limitations as speaking (10th April 2018) about code update
and doing a `set contract` command on an existing contract.

Limitations:

* Adding only a field to a `struct` does not update the code version
  giving `account.code_version != code_id: contract is already running this version of code`
  error message when trying to update the contract.

#### Set Contract Common Mistakes

* Test when

* ABI JSON Wrong Floating Type

  The correct way in the ABI JSON to represents a floating point
  number is to use `float64` which maps to `double` in the C++ code.
  Using `double` or `float` will result in the following cryptic error:

  ```
  Error 3030000: transaction validation exception
  Ensure that your transaction satisfy the contract's constraint!
  Error Details:
  is_type(a.second):
  ```

* ABI JSON Table Name Mismatch

  In your C++ contract, if you make a type in the `multi_index` table
  name when defining the `multi_index` `typedef` like this:

  ```
  typedef eosio::multi_index<N(tablename), card> card_index;
  ```

  If `tablename` has a mismatch with regarding the table name ABI
  definition, then at runtime it will work but access to the
  table via the CLI will not work since it will say table does not
  exist.

* ABI JSON Table Missing Fields

  If the `ABI` table definition has mismatch between actual code
  `struct` and definition, consequences will firstly the when querying
  the table data back, JSON representation will be broken.

  For an actual C++ definition of:

  ```
  struct {
    account_name owner;
    uint64_t value1;
    uint64_t value2;
  }
  ```

  And a table ABI definition of:

  ```
  {
    "owner": "account_name",
    "value2": "uint64"
  }
  ```

  A data of containing `{owner:john,value1:1,value2:2}`, the call
  `cleos get table contract contract table` results in:

  ```
  {
    "rows": [{
      "owner": "john",
      "value2": "1"
    }],
    "more": "false"
  }
  ```

* `EOSLIB_SERIALIZE` mismatch field(s)

  Let's say you added a field to an existing `struct` in your C++ code and also
  in the ABI but you forgot to add to the serialization of the struct, this results
  in no error when setting the contract and will instead cause runtime problem.

  For example, an `account` struct with fields `owner` and `player` where the
  EOS serialization call would be `EOSLIB_SERIALIZE(account, (owner))`. This will
  lead to runtime problem where `player` won't be initialized.

#### Set Contract Common Errors

* Error generated by `cleos set contract <account> <contract_folder>`
  when an action name is not respecting the base32 naming convention of EOS:

  ```
  Reading WAST...
  Assembling WASM...
  Error 3120001: Invalid name
  Name should be less than 13 characters and only contains the following symbol .12345abcdefghijklmnopqrstuvwxyz
  Error Details:
  Name not properly normalized (name: createv6, normalized: createv)
  Fail to parse ABI JSON
  ```

* Error generated by `cleos set contract <account> <contract_folder>`
  when an action is present but the associated struct of the action
  is not present in the `.abi` file:

  ```
   Reading WAST...
   Assembling WASM...
   Publishing contract...
   Error 3030000: transaction validation exception
   Ensure that your transaction satisfy the contract's constraint!
   Error Details:
   is_type(a.second):
  ```

* Error generated by `cleos set contract <account> <contract_folder>`
  when a table definition is present (i.e. `accounts`) but the associated
  struct of the stored data is not present (i.e. `account`) in the `.abi` file:

  ```
   Reading WAST...
   Assembling WASM...
   Publishing contract...
   Error 3030000: transaction validation exception
   Ensure that your transaction satisfy the contract's constraint!
   Error Details:
   is_type(a.second):
  ```

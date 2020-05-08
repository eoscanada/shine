#pragma once

#include <algorithm>
#include <string>
#include <cmath>

#include <eosio/eosio.hpp>
#include <eosio/time.hpp>
#include <eosio/system.hpp>
#include <eosio/asset.hpp>

using eosio::asset;
using eosio::check;
using eosio::const_mem_fun;
using eosio::current_time_point;
using eosio::datastream;
using eosio::indexed_by;
using eosio::name;
using eosio::time_point_sec;
using eosio::permission_level;
using std::function;
using std::string;


#include "asset.hpp"
#include "table.hpp"

// Typedefs
typedef uint64_t post_id;

class [[eosio::contract("shine")]] shine : public eosio::contract {
 public:
  shine(name receiver, name code, datastream<const char*> ds)
        :eosio::contract(receiver, code, ds) {}

  // Management Actions
  [[eosio::action]]
    void purgeall(const name org);
  [[eosio::action]]
    void reset(const name org);
  [[eosio::action]]
    void configure(const name org, uint64_t recipient_weight, uint64_t voter_weight, uint64_t poster_weight);

  // Posting/voting actions
  [[eosio::action]]
    void post(const name org, const bool org_auth, const name from, const name to, const string& memo);
  [[eosio::action]]
    void vote(const name org, const bool org_auth, const name voter, const post_id post_id);

  // Registration actions
  [[eosio::action]]
    void regaccount(const name org, const name shine_account, const name onchain_account, const string offchain_account);
  [[eosio::action]]
    void unregaccount(const name org, const name shine_account);

  // Dispatch actions
  [[eosio::on_notify("eosio.token::transfer")]]
    void on_transfer(const name from, const name to, const asset quantity, const string& memo);

 private:

  struct [[eosio::table]] seen_row {
     // Contains a XOR of `post_id` + `shine_account`. Simple marker that you voted already, so you don't double-vote.  Not maximally collision free, but fast and efficient enough.
    uint64_t seenhash;

    uint64_t primary_key() const { return seenhash; }
    void print() const { eosio::print("Seen(", eosio::name{seenhash}, ")"); }

    EOSLIB_SERIALIZE(seen_row, (seenhash))
  };

  typedef eosio::multi_index<"seen"_n, seen_row> seen_index;


  struct [[eosio::table]] post_row {
    uint64_t id;
    name from;
    name to;
    string memo;

    uint64_t primary_key() const { return id; }

    void print() const { eosio::print("Post (", eosio::name{id}, ")"); }

    EOSLIB_SERIALIZE(post_row, (id)(from)(to)(memo))
  };

  typedef eosio::multi_index<"posts"_n, post_row> posts_index;

  //
  // Accumulated weights
  //

  struct [[eosio::table]] weights_row {
    name shine_account;
    uint64_t weight;

    uint64_t primary_key() const { return shine_account.value; }

    void print() const { eosio::print("Weights (", eosio::name{shine_account}, ", ", weight, ")"); }

    EOSLIB_SERIALIZE(weights_row, (shine_account)(weight))
  };

  typedef eosio::multi_index<"weights"_n, weights_row> weights_index;


  //
  // This is a persistent table, not cleared upon resets.
  //

  struct [[eosio::table]] member_row {
    name shine_account;  // Internal account ID
    name onchain_account;
    string offchain_account; // Some sort of hash of something off chain, like a salt + Slack e-mail or ID

    uint64_t primary_key() const { return shine_account.value; }

    void print() const { eosio::print("Member (", shine_account, ", ", eosio::name{onchain_account}, ", ", offchain_account , ")"); }

    EOSLIB_SERIALIZE(member_row, (shine_account)(onchain_account)(offchain_account))
  };

  typedef eosio::multi_index<"members"_n, member_row> members_index;

  //
  // Organization's rewards configuration
  //

  struct [[eosio::table]] config_row {
    uint64_t last_post_id;
    uint64_t recipient_weight;
    uint64_t voter_weight;
    uint64_t poster_weight;

    uint64_t primary_key() const { return "config"_n.value; }

    void print() const { eosio::print("Config (last post id: ", last_post_id, ", recipient weight: ", recipient_weight, ", voter weight: ", voter_weight, ", poster weight: ", poster_weight, ")"); }

    EOSLIB_SERIALIZE(config_row, (last_post_id)(recipient_weight)(voter_weight)(poster_weight))
  };

  typedef eosio::multi_index<"configs2"_n, config_row> configs_index;


  void update_weight(const name org, const name payer, const name account, const function<void(weights_row&)> updater);
  void require_active_auth(const name account) const { require_auth(permission_level{account, "active"_n}); };
  void register_member(const name org, const name shine_account, const name onchain_account, const string offchain_account);
  void unregister_member(const name org, const name shine_account);
  bool shine_account_exists(const name org, const name account);
};

#include <algorithm>
#include <cmath>
#include <string>

#include <eosiolib/action.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/fixed_key.hpp>

#include "asset.hpp"
#include "table.hpp"

// Configurable values
#define REWARD_PRAISE_POSTED_WEIGHT 0.07
#define REWARD_VOTE_RECEIVED_WEIGHT 0.9
#define REWARD_VOTE_GIVEN_WEIGHT 0.03

// Macro
#define TABLE(X) ::eosio::string_to_name(#X)

// Namespaces
using eosio::asset;
using eosio::const_mem_fun;
using eosio::indexed_by;
using eosio::permission_level;
using eosio::require_auth;
using eosio::unpack_action_data;
using std::for_each;
using std::function;
using std::string;

using namespace eoscanada;

// Typedefs
typedef uint64_t praise_id;

namespace eosio {
/**
 * FIXME:
 * The actual `eosio.token` transfer struct definition until its definition is accesible
 * from an actual `eosio.token.hpp` file. Until then, we define it ourself so
 * we can unpack the actual action data when a token transfer occurs inside
 * the `eosio.token` contract to this contract's account.
 */
struct token_transfer {
  account_name from;
  account_name to;
  asset quantity;
  string memo;
};
}  // namespace eosio

namespace eoscanada {

class shine : public eosio::contract {
 public:
  static bool is_token_transfer(uint64_t code, uint64_t action) {
    return code == N(eosio.token) && action == N(transfer);
  }

  shine(account_name contract_account)
      : eosio::contract(contract_account),
        praises(contract_account, contract_account),
        votes(contract_account, contract_account),
        stats(contract_account, contract_account),
        rewards(contract_account, contract_account) {}

  // Actions
  void addpraise(const account_name author, const account_name praisee, const string& memo);
  void addvote(const praise_id praise_id, const account_name voter);
  void reset(const uint64_t any);

  // Notifications
  void transfer(const asset& pot);

 private:
  //@abi table praises i64
  struct praise {
    uint64_t id;
    account_name author;
    account_name praisee;
    string memo;

    uint64_t primary_key() const { return id; }

    void print() const { eosio::print("Praise (", eosio::name{id}, ")"); }

    EOSLIB_SERIALIZE(praise, (id)(author)(praisee)(memo))
  };

  typedef eosio::multi_index<TABLE(praises), praise> praises_index;

  //@abi table votes i64
  struct vote {
    uint64_t id;
    praise_id praise_id;
    account_name voter;

    uint64_t primary_key() const { return id; }

    void print() const { eosio::print("Vote (", eosio::name{id}, ")"); }

    EOSLIB_SERIALIZE(vote, (id)(praise_id)(voter))
  };

  typedef eosio::multi_index<TABLE(votes), vote> votes_index;

  //@abi table memberstats i64
  /**
   * praise_posted - The praise count posted by the member
   * praise_vote_received - The amount of vote received for all praise posted by the member
   * vote_given_explicit - The amout of vote the member did on other member praises
   * vote_received_implicit - The amount of implicit vote (vote by the author of the praise) received
   * vote_received_explicit - The amount of explicit vote (voty by other members than author of praise) received
   */
  struct member_stat {
    account_name account;
    uint64_t praise_posted;
    uint64_t praise_vote_received;
    uint64_t vote_given_explicit;
    uint64_t vote_received_implicit;
    uint64_t vote_received_explicit;

    member_stat()
        : praise_posted(0),
          praise_vote_received(0),
          vote_given_explicit(0),
          vote_received_implicit(0),
          vote_received_explicit(0) {}

    uint64_t primary_key() const { return account; }

    void print() const { eosio::print("Member stat (", eosio::name{account}, ")"); }

    EOSLIB_SERIALIZE(member_stat, (account)(praise_posted)(praise_vote_received)(vote_given_explicit)(
                                      vote_received_implicit)(vote_received_explicit))
  };

  typedef eosio::multi_index<TABLE(memberstats), member_stat> member_stats_index;

  //@abi table rewards i64
  struct reward {
    account_name account;
    asset amount_praise_posted;
    asset amount_vote_received;
    asset amount_vote_given;
    asset amount_total;

    uint64_t primary_key() const { return account; }

    void print() const { eosio::print("Reward (", eosio::name{account}, ")"); }

    EOSLIB_SERIALIZE(reward, (account)(amount_praise_posted)(amount_vote_received)(amount_vote_given)(amount_total))
  };

  typedef eosio::multi_index<TABLE(rewards), reward> rewards_index;

  struct distribution_stat {
    uint64_t vote_explicit;
    uint64_t vote_total;
    double vote_given_weighted_total;

    asset distributed_pot;

    distribution_stat()
        : vote_explicit(0), vote_total(0), vote_given_weighted_total(0), distributed_pot(0, EOS_SYMBOL) {}
  };

  static double compute_vote_given_weight(uint64_t vote_given) {
    if (vote_given <= 0) return 0;
    if (vote_given <= 1) return 1 / 3.0;
    if (vote_given <= 2) return 2 / 3.0;

    return 1.0;
  }

  void compute_rewards(const asset& pot);
  void compute_global_stats(distribution_stat& distribution);
  void distribute_rewards(const asset& pot, distribution_stat& distribution);

  member_stats_index::const_iterator find_last_stats_member_itr() { return stats.end()--; }

  bool has_rewards() const { return rewards.begin() != rewards.end(); }
  void require_active_auth(const account_name account) const { require_auth(permission_level{account, N(active)}); }
  void update_member_stat(const account_name account, const function<void(member_stat&)> updater);
  void update_reward_for_member(const account_name account, const function<void(reward&)> updater);

  praises_index praises;
  votes_index votes;
  member_stats_index stats;
  rewards_index rewards;
};

}  // namespace eoscanada

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
#define REWARD_post_count_WEIGHT 0.07
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
typedef uint64_t post_id;

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
        posts(contract_account, contract_account),
        votes(contract_account, contract_account),
        stats(contract_account, contract_account),
        rewards(contract_account, contract_account) {}

  // Actions
  void post(const account_name from, const account_name to, const string& memo);
  void vote(const account_name voter, const post_id post_id);
  void reset(const uint64_t any);

  // Notifications
  void transfer(const asset& pot);

 private:
  //@abi table posts i64
  struct post_row {
    uint64_t id;
    account_name from;
    account_name to;
    string memo;

    uint64_t primary_key() const { return id; }

    void print() const { eosio::print("Post (", eosio::name{id}, ")"); }

    EOSLIB_SERIALIZE(post_row, (id)(from)(to)(memo))
  };

  typedef eosio::multi_index<TABLE(posts), post_row> posts_index;

  //@abi table votes i64
  struct vote_row {
    uint64_t id;
    account_name voter;
    post_id post_id;

    uint64_t primary_key() const { return id; }

    void print() const { eosio::print("Vote (", eosio::name{id}, ")"); }

    EOSLIB_SERIALIZE(vote_row, (id)(voter)(post_id))
  };

  typedef eosio::multi_index<TABLE(votes), vote_row> votes_index;

  //@abi table memberstats i64
  /**
   * post_count - The post count posted by the member
   * post_vote_received - The amount of vote received for all post posted by the member
   * vote_given_explicit - The amout of vote the member did on other member posts
   * vote_received_implicit - The amount of implicit vote (vote by the author of the post) received
   * vote_received_explicit - The amount of explicit vote (voty by other members than author of post) received
   */
  struct member_stat_row {
    account_name account;
    uint64_t post_count;
    uint64_t post_vote_received;
    uint64_t vote_given_explicit;
    uint64_t vote_received_implicit;
    uint64_t vote_received_explicit;

    member_stat_row()
        : post_count(0),
          post_vote_received(0),
          vote_given_explicit(0),
          vote_received_implicit(0),
          vote_received_explicit(0) {}

    uint64_t primary_key() const { return account; }

    void print() const { eosio::print("Member stat (", eosio::name{account}, ")"); }

    EOSLIB_SERIALIZE(member_stat_row, (account)(post_count)(post_vote_received)(vote_given_explicit)(
                                          vote_received_implicit)(vote_received_explicit))
  };

  typedef eosio::multi_index<TABLE(memberstats), member_stat_row> member_stats_index;

  //@abi table rewards i64
  struct reward_row {
    account_name account;
    asset amount_post_count;
    asset amount_vote_received;
    asset amount_vote_given;
    asset amount_total;

    uint64_t primary_key() const { return account; }

    void print() const { eosio::print("Reward (", eosio::name{account}, ")"); }

    EOSLIB_SERIALIZE(reward_row, (account)(amount_post_count)(amount_vote_received)(amount_vote_given)(amount_total))
  };

  typedef eosio::multi_index<TABLE(rewards), reward_row> rewards_index;

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
  void update_member_stat(const account_name account, const function<void(member_stat_row&)> updater);
  void update_reward_for_member(const account_name account, const function<void(reward_row&)> updater);

  posts_index posts;
  votes_index votes;
  member_stats_index stats;
  rewards_index rewards;
};

}  // namespace eoscanada

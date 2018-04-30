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
using eosio::key256;
using eosio::permission_level;
using eosio::require_auth;
using eosio::unpack_action_data;
using std::for_each;
using std::function;
using std::string;

using namespace eoscanada;

// Typedefs
typedef checksum256 member_id;
typedef checksum256 post_id;

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
        accounts(contract_account, contract_account),
        praises(contract_account, contract_account),
        votes(contract_account, contract_account),
        stats(contract_account, contract_account),
        rewards(contract_account, contract_account) {}

  // Actions
  void addpraise(const post_id& post, const member_id& author, const member_id& praisee, const string& memo);
  void addvote(const post_id& post, const member_id& voter);
  void bindmember(const member_id& member, const account_name account);
  void unbindmember(const member_id& member);
  void reset(const uint64_t any);
  void clear(const uint64_t any);

  // Notifications
  void transfer(const asset& pot);

 private:
  //@abi table accounts i64
  struct account {
    account_name id;
    member_id member;

    uint64_t primary_key() const { return id; }
    key256 by_member() const { return shine::compute_checksum256_key(member); }

    void print() const { eosio::print("Account (", eosio::name{id}, ")"); }

    EOSLIB_SERIALIZE(account, (id)(member))
  };

  typedef eosio::multi_index<TABLE(accounts), account,
                             indexed_by<N(member), const_mem_fun<account, key256, &account::by_member>>>
      accounts_index;

  //@abi table praises i64
  struct praise {
    uint64_t id;
    post_id post;
    member_id author;
    member_id praisee;
    string memo;

    uint64_t primary_key() const { return id; }
    key256 by_post() const { return shine::compute_checksum256_key(post); }

    void print() const { eosio::print("Praise (", eosio::name{id}, ")"); }

    EOSLIB_SERIALIZE(praise, (id)(post)(author)(praisee)(memo))
  };

  typedef eosio::multi_index<TABLE(praises), praise,
                             indexed_by<N(post), const_mem_fun<praise, key256, &praise::by_post>>>
      praises_index;

  //@abi table votes i64
  struct vote {
    uint64_t id;
    post_id post;
    member_id voter;

    uint64_t primary_key() const { return id; }

    void print() const { eosio::print("Vote (", eosio::name{id}, ")"); }

    EOSLIB_SERIALIZE(vote, (id)(post)(voter))
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
    uint64_t id;
    member_id member;
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

    uint64_t primary_key() const { return id; }
    key256 by_member() const { return shine::compute_checksum256_key(member); }

    void print() const { eosio::print("Member stat (", eosio::name{id}, ")"); }

    EOSLIB_SERIALIZE(member_stat, (id)(member)(praise_posted)(praise_vote_received)(vote_given_explicit)(
                                      vote_received_implicit)(vote_received_explicit))
  };

  typedef eosio::multi_index<TABLE(memberstats), member_stat,
                             indexed_by<N(member), const_mem_fun<member_stat, key256, &member_stat::by_member>>>
      member_stats_index;

  //@abi table rewards i64
  struct reward {
    uint64_t id;
    member_id member;
    asset amount_praise_posted;
    asset amount_vote_received;
    asset amount_vote_given;
    asset amount_extra;
    asset amount_total;

    uint64_t primary_key() const { return id; }
    key256 by_member() const { return shine::compute_checksum256_key(member); }

    void print() const { eosio::print("Reward (", eosio::name{id}, ")"); }

    EOSLIB_SERIALIZE(
        reward, (id)(member)(amount_praise_posted)(amount_vote_received)(amount_vote_given)(amount_extra)(amount_total))
  };

  typedef eosio::multi_index<TABLE(rewards), reward,
                             indexed_by<N(member), const_mem_fun<reward, key256, &reward::by_member>>>
      rewards_index;

  struct distribution_stat {
    uint64_t member_count;
    uint64_t distributed_count;

    uint64_t vote_explicit;
    uint64_t vote_total;
    double vote_given_weighted_total;

    asset distributed_pot;
    asset undistributed_pot;

    distribution_stat()
        : member_count(0),
          distributed_count(0),
          vote_explicit(0),
          vote_total(0),
          vote_given_weighted_total(0),
          distributed_pot(0, EOS_SYMBOL),
          undistributed_pot(0, EOS_SYMBOL) {}
  };

  static key256 compute_checksum256_key(const checksum256& checksum) {
    const uint64_t* p64 = reinterpret_cast<const uint64_t*>(&checksum);
    return key256::make_from_word_sequence<uint64_t>(p64[0], p64[1], p64[2], p64[3]);
  }

  static double compute_vote_given_weight(uint64_t vote_given) {
    if (vote_given <= 0) return 0;
    if (vote_given <= 1) return 1 / 3.0;
    if (vote_given <= 2) return 2 / 3.0;

    return 1.0;
  }

  void compute_rewards(const asset& pot);
  void compute_global_stats(distribution_stat& distribution);
  void distribute_rewards_to_bound_members(const asset& pot, distribution_stat& distribution);
  void distribute_pot_balance_to_first_member(const asset& balance, distribution_stat& distribution);
  void distribute_pot_balance_to_bound_members(asset balance, distribution_stat& distribution);

  member_stats_index::const_iterator find_last_stats_member_itr() {
    for (auto itr = stats.end(); itr != stats.begin(); itr = itr--) {
      if (has_account(itr->member)) return itr;
    }

    return stats.end();
  }

  bool has_account(const member_id& id) const {
    auto index = accounts.template get_index<N(member)>();

    return index.find(compute_checksum256_key(id)) != index.end();
  }

  bool has_rewards() const { return rewards.begin() != rewards.end(); }
  void require_shine_active_auth() const { require_auth(permission_level{_self, N(active)}); }
  void update_member_stat(const member_id& member, const function<void(member_stat&)> updater);
  void update_reward_for_member(const member_id& member, const function<void(reward&)> updater);

  accounts_index accounts;
  praises_index praises;
  votes_index votes;
  member_stats_index stats;
  rewards_index rewards;
};

}  // namespace eoscanada

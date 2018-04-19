#include <algorithm>

#include "asset.hpp"
#include "shine.hpp"
#include "table.hpp"

// Namespaces
using namespace dblk;

using eosio::asset;
using eosio::require_auth;
using eosio::permission_level;
using eosio::unpack_action_data;

/**
 * SmartContract C entrypoint using a macro based on the list of action in the.
 * For each of defined action, a switch branch is added
 * automatically unpacking the data into the action's structure and dispatching
 * a method call to `action` define in this SmartContract.
 *
 * Each time a new action is added, EOSIO_API definition should be expanded with the
 * new action handler's method name.
 */
extern "C" {
  void apply(uint64_t receiver, uint64_t code, uint64_t action) {
    auto self = receiver;
    if (code == self) {
      // Don't rename `thiscontract`, it's being use verbatim in `EOSIO_API` macro
      shine thiscontract(self);
      switch (action) {
        EOSIO_API(shine, (addpraise)(addvote)(bindmember)(unbindmember)(reset)(clear))
      }

      eosio_exit(0);
    }

    if (shine::is_token_transfer(code, action)) {
      eosio::token_transfer action = unpack_action_data<eosio::token_transfer>();
      if (action.to != self) {
        eosio_exit(0);
        return;
      }

      shine(self).transfer(action.quantity);
      eosio_exit(0);
    }
  }
}

///
//// Actions
///

void shine::addpraise(const member_id& author, const member_id& praisee, const string& memo) {
  // require_shine_active_auth();

  auto praise_itr = praises.emplace(_self, [&](auto& praise) {
    praise.id = praises.available_primary_key();
    praise.author = author;
    praise.praisee = praisee;
    praise.memo = memo;
  });

  update_member_stat(author, [](auto& stat) { stat.praise_posted += 1; });
  update_member_stat(praisee, [](auto& stat) { stat.vote_received_implicit += 1; });
  update_global_stat([](auto& global_stat) { global_stat.vote_implicit += 1; });
}

void shine::addvote(const uint64_t praise_id, const member_id& voter) {
  require_shine_active_auth();

  auto praise_itr = praises.find(praise_id);
  eosio_assert(praise_itr != praises.end(), "praise with this id does not exist");

  votes.emplace(_self, [&](auto& vote) {
    vote.id = votes.available_primary_key();
    vote.praise_id = praise_id;
    vote.voter = voter;
  });

  update_member_stat(voter, [](auto& stat) { stat.vote_given_explicit += 1; });
  update_member_stat(praise_itr->praisee, [](auto& stat) { stat.vote_received_explicit += 1; });
  update_member_stat(praise_itr->author, [](auto& stat) { stat.praise_vote_received += 1; });
  update_global_stat([](auto& global_stat) { global_stat.vote_explicit += 1; });
}

/**
 * Associate an EOS account to a shine member. Assuming we receive in parameter
 * `M` as member id and `A` as account, 5 possible cases can happen:
 *  + A -> M' exists
 *  + A' -> M exists
 *  + A' -> M & A -> exist
 *  + A -> M exists
 *  + No mapping exist
 *
 * We also must keep the integrity so that we never encounter `A -> M & A' -> M`.
 *
 * Implementation wise, we must also note that the member index cannot change
 * the `id` value of the table since it's the primary key, a removal + insertion
 * must be peformed to change the primary key.
 */
void shine::bindmember(const member_id& member, const account_name account_id) {
  require_shine_active_auth();

  auto account_itr = accounts.find(account_id);
  if (account_itr != accounts.end()) {
    if (account_itr->member == member) {
      // Nothing to do, mapping is already the correct one.
      return;
    }

    accounts.erase(account_itr);
  }

  auto member_account_index = accounts.template get_index<N(member)>();
  auto member_itr = member_account_index.find(compute_member_id_key(member));
  if (member_itr != member_account_index.end()) {
    member_account_index.erase(member_itr);
  }

  accounts.emplace(_self, [&](auto& account) {
    account.id = account_id;
    account.member = member;
  });
}

/**
 * Removes the actual association between an EOS account and
 * a member.
 */
void shine::unbindmember(const member_id& member) {
  require_shine_active_auth();

  auto member_account_index = accounts.template get_index<N(member)>();
  auto member_itr = member_account_index.find(compute_member_id_key(member));
  if (member_itr != member_account_index.end()) {
    member_account_index.erase(member_itr);
  }
}

/**
 * Reset statistics for praises, votes, stats and rewards. Mapping between
 * member and account will be kept.
 */
void shine::reset(const uint64_t any) {
  require_shine_active_auth();

  table::clear(praises);
  table::clear(votes);
  table::clear(stats);
  table::clear(global_stats);
  table::clear(rewards);
}

/**
 * Clear all tables of this contract to start from scratch.
 *
 * **Important** Only useful in development, should not be used in production.
 */
void shine::clear(const uint64_t any) {
  require_shine_active_auth();

  table::clear(accounts);
  reset(any);
}

///
//// Notifications
///

/**
 * Transfer the actual pot into rewards to all members.
 */
void shine::transfer(const asset& pot) {
  compute_rewards(pot);
  eosio_assert(has_rewards(), "cannot transfer pot without any rewards");

  std::for_each(rewards.begin(), rewards.end(), [&](auto& reward) {
    auto account_member_index = accounts.template get_index<N(member)>();
    auto account_member_itr = account_member_index.find(compute_member_id_key(reward.member));
    if (account_member_itr == account_member_index.end()) {
      eosio::print("No mapping from member to account name, no reward to transfer.");
      return;
    }

    eosio::token_transfer transfer{_self, account_member_itr->id, reward.amount_total, ""};
    eosio::action(permission_level{_self, N(active)}, N(eosio.token), N(transfer), transfer).send();
  });
}

/**
 * The calculation of rewards follow a simple algorithm using as raw input
 * for each member:
 *  + The amount of vote received
 *  + The amount of vote given
 *  + The amount of praise posted
 *
 * Base on these three values, a weight is computed for each of them:
 *  + Vote received -> Proportional to total vote count (explicit + implicit)
 *  + Vote given -> Proportional to total of vote given weight (1 vote -> 1/3, 2 votes -> 2/3, 2+ votes -> 3/3)
 *  + Praise posted -> Propotional to total explicite vote count
 */
void shine::compute_rewards(const asset& pot) {
  auto symbol = pot.symbol;
  eosio_assert(symbol.is_valid() && symbol == EOS_SYMBOL, "pot currency should be EOS with precision 4");

  auto global_stats_itr = global_stats.find(GLOBAL_STAT_ID);
  eosio_assert(global_stats_itr != global_stats.end(), "cannot compute rewards without any praise");

  auto pot_amount = asset_to_double(pot);
  auto vote_implicit = global_stats_itr->vote_implicit;
  auto vote_explicit = global_stats_itr->vote_explicit;
  auto vote_total = vote_implicit + vote_explicit;

  // This loop through all stats giving 2N iterations. Ideally, this could
  // probably eliminated somehow. At least, computation of
  // `compute_vote_given_weight` could be cached so it would not be redo in the
  // second loop that follows.
  auto vote_given_weighted_total = compute_vote_given_weighted_total();

  std::for_each(stats.begin(), stats.end(), [&](auto& stats) {
    auto vote_received = stats.vote_received_explicit + stats.vote_received_implicit;
    auto praise_vote_received = stats.praise_vote_received;
    auto vote_given = stats.vote_given_explicit;

    auto vote_given_weights = compute_vote_given_weight(vote_given);
    auto vote_given_weighted = vote_given * vote_given_weights;

    auto vote_received_weight = vote_received / (double)vote_total;
    auto praise_posted_weight = praise_vote_received / (double)vote_explicit;
    auto vote_given_weight = vote_given_weighted / vote_given_weighted_total;

    auto vote_received_amount = double_to_asset(vote_received_weight * pot_amount * REWARD_VOTE_RECEIVED_WEIGHT);
    auto praise_posted_amount = double_to_asset(praise_posted_weight * pot_amount * REWARD_PRAISE_POSTED_WEIGHT);
    auto vote_given_amount = double_to_asset(vote_given_weight * pot_amount * REWARD_VOTE_GIVEN_WEIGHT);
    auto amount_total = vote_received_amount + praise_posted_amount + vote_given_amount;

    update_reward_for_member(stats.id, stats.member, [&](auto& reward) {
      reward.amount_praise_posted = praise_posted_amount;
      reward.amount_vote_received = vote_received_amount;
      reward.amount_vote_given = vote_given_amount;
      reward.amount_total = amount_total;
    });
  });
}

///
//// Helpers
///

double shine::compute_vote_given_weighted_total() const {
  auto vote_given_weighted_total = 0.0;

  std::for_each(stats.begin(), stats.end(), [&](auto& stats) {
    auto vote_given = stats.vote_given_explicit;

    vote_given_weighted_total += vote_given * compute_vote_given_weight(vote_given);
  });

  return vote_given_weighted_total;
}

void shine::update_global_stat(const std::function<void(global_stat&)> updater) {
  auto global_stat_itr = global_stats.find(GLOBAL_STAT_ID);
  if (global_stat_itr == global_stats.end()) {
    global_stats.emplace(_self, [&](auto& global_stat) {
      global_stat.id = GLOBAL_STAT_ID;

      updater(global_stat);
    });
  } else {
    global_stats.modify(global_stat_itr, _self, [&](auto& global_stat) { updater(global_stat); });
  }
}

void shine::update_reward_for_member(const uint64_t id, const member_id& member,
                                     const std::function<void(reward&)> updater) {
  auto reward_itr = rewards.find(id);
  if (reward_itr == rewards.end()) {
    rewards.emplace(_self, [&](auto& reward) {
      reward.id = id;
      reward.member = member;
      updater(reward);
    });
  } else {
    rewards.modify(reward_itr, _self, [&](auto& reward) { updater(reward); });
  }
}

void shine::update_member_stat(const member_id& member, const std::function<void(member_stat&)> updater) {
  auto member_stats_index = stats.template get_index<N(member)>();

  auto stat_itr = member_stats_index.find(compute_member_id_key(member));
  if (stat_itr == member_stats_index.end()) {
    stats.emplace(_self, [&](auto& stat) {
      stat.id = stats.available_primary_key();
      stat.member = member;

      updater(stat);
    });
  } else {
    member_stats_index.modify(stat_itr, _self, [&](auto& stat) { updater(stat); });
  }
}

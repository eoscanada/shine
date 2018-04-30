#include "shine.hpp"

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
    switch (action) { EOSIO_API(shine, (addpraise)(addvote)(bindmember)(unbindmember)(reset)(clear)) }

    eosio_exit(0);
  }

  if (shine::is_token_transfer(code, action)) {
    eosio::token_transfer action = unpack_action_data<eosio::token_transfer>();

    // Only pass notification to shine if transfer `to` is shine contract account and `quantity` are EOS tokens
    if (action.to == self && action.quantity.symbol == EOS_SYMBOL) {
      shine(self).transfer(action.quantity);
    }

    eosio_exit(0);
  }
}
}

///
//// Actions
///

void shine::addpraise(const post_id& post, const member_id& author, const member_id& praisee, const string& memo) {
  require_shine_active_auth();

  auto praise_itr = praises.emplace(_self, [&](auto& praise) {
    praise.id = praises.available_primary_key();
    praise.post = post;
    praise.author = author;
    praise.praisee = praisee;
    praise.memo = memo;
  });

  update_member_stat(author, [](auto& stat) { stat.praise_posted += 1; });
  update_member_stat(praisee, [](auto& stat) { stat.vote_received_implicit += 1; });
}

void shine::addvote(const post_id& post, const member_id& voter) {
  require_shine_active_auth();

  auto post_index = praises.template get_index<N(post)>();
  auto praise_itr = post_index.find(compute_checksum256_key(post));
  eosio_assert(praise_itr != post_index.end(), "Praise with this post id does not exist.");

  votes.emplace(_self, [&](auto& vote) {
    vote.id = votes.available_primary_key();
    vote.post = praise_itr->post;
    vote.voter = voter;
  });

  update_member_stat(voter, [](auto& stat) { stat.vote_given_explicit += 1; });
  update_member_stat(praise_itr->praisee, [](auto& stat) { stat.vote_received_explicit += 1; });
  update_member_stat(praise_itr->author, [](auto& stat) { stat.praise_vote_received += 1; });
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
  auto member_itr = member_account_index.find(compute_checksum256_key(member));
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
  auto member_itr = member_account_index.find(compute_checksum256_key(member));
  if (member_itr != member_account_index.end()) {
    member_account_index.erase(member_itr);
  }

  auto reward_member_index = rewards.template get_index<N(member)>();
  auto member_reward_itr = reward_member_index.find(compute_checksum256_key(member));
  if (member_reward_itr != reward_member_index.end()) {
    reward_member_index.erase(member_reward_itr);
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
  eosio_assert(has_rewards(), "Cannot transfer pot without any rewards.");

  for_each(rewards.begin(), rewards.end(), [&](auto& reward) {
    auto account_member_index = accounts.template get_index<N(member)>();
    auto account_member_itr = account_member_index.find(compute_checksum256_key(reward.member));
    if (account_member_itr == account_member_index.end()) {
      // This should never append has rewards computation is save to table only on bound account
      eosio::print("No mapping from member to EOS account, this should never happen.");
      continue;
    }

    eosio::token_transfer transfer{_self, account_member_itr->id, reward.amount_total, "From Shine"};
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
 *  + Praise posted -> Propotional to total explicite vote count.
 *
 * The algorithm first loop through all member stats to compute global
 * stats (total vote, total praise, etc).
 *
 * Then, it computes the rewards for each member that has a bound EOS account.
 * The amount of distributed tokens is also being accumulated. When all member
 * has been processed, the undistributed pot is computed by doing `pot - distributed_pot`.
 *
 * If the undistributed pot is 0, the rewards computation stops here.
 *
 * If the undistributed pot is above 0, then there is two possibilites.
 *
 * First, if there is only one person to distribute the balance to if
 * it's impossible to divise further the undistributed pot to all bound
 * members, the balance is simply awarded to the first member.
 *
 * Else, the undistributed pot is spread evenly through all bound members,
 * if the undistributed pot cannot be split evenly, the last member receives
 * the remainder.
 */
void shine::compute_rewards(const asset& pot) {
  eosio_assert(pot.amount > 0, "Pot quantity must be higher than 0.");

  distribution_stat distribution;

  compute_global_stats(distribution);
  distribute_rewards_to_bound_members(pot, distribution);
  if (distribution.distributed_pot.amount == pot.amount) {
    return;
  }

  eosio_assert(distribution.distributed_count > 0, "No one eligible for rewards distribution.");
  eosio_assert(distribution.distributed_pot.amount <= pot.amount,
               "Distributed pot should never go higher than actual pot.");

  if (distribution.distributed_count == 1 ||
      (distribution.undistributed_pot / distribution.distributed_count).amount == 0) {
    distribute_pot_balance_to_first_member(distribution.undistributed_pot, distribution);
  } else {
    distribute_pot_balance_to_bound_members(distribution.undistributed_pot, distribution);
  }

  eosio_assert(distribution.distributed_pot.amount == pot.amount,
               "Distributed pot should be equivalent to actual pot.");
}

void shine::compute_global_stats(distribution_stat& distribution) {
  for_each(stats.begin(), stats.end(), [&](auto& stat) {
    auto praised_posted = stat.praise_posted;
    auto vote_given = stat.vote_given_explicit;

    distribution.member_count += 1;
    distribution.vote_explicit += vote_given;
    distribution.vote_total += praised_posted + vote_given;
    distribution.vote_given_weighted_total += vote_given * compute_vote_given_weight(vote_given);
  });
}

void shine::distribute_rewards_to_bound_members(const asset& pot, distribution_stat& distribution) {
  auto pot_amount = asset_to_double(pot);

  for_each(stats.begin(), stats.end(), [&](auto& stat) {
    auto vote_received = stat.vote_received_explicit + stat.vote_received_implicit;
    auto praise_vote_received = stat.praise_vote_received;
    auto vote_given = stat.vote_given_explicit;

    auto vote_given_weights = compute_vote_given_weight(vote_given);
    auto vote_given_weighted = vote_given * vote_given_weights;

    auto vote_received_weight = vote_received / (double)distribution.vote_total;
    auto praise_posted_weight = praise_vote_received / (double)distribution.vote_explicit;
    auto vote_given_weight = vote_given_weighted / distribution.vote_given_weighted_total;

    auto vote_received_amount = double_to_asset(vote_received_weight * pot_amount * REWARD_VOTE_RECEIVED_WEIGHT);
    auto praise_posted_amount = double_to_asset(praise_posted_weight * pot_amount * REWARD_PRAISE_POSTED_WEIGHT);
    auto vote_given_amount = double_to_asset(vote_given_weight * pot_amount * REWARD_VOTE_GIVEN_WEIGHT);
    auto amount_total = vote_received_amount + praise_posted_amount + vote_given_amount;

    if (has_account(stat.member)) {
      distribution.distributed_count += 1;
      distribution.distributed_pot += amount_total;

      update_reward_for_member(stat.member, [&](auto& reward) {
        reward.amount_praise_posted = praise_posted_amount;
        reward.amount_vote_received = vote_received_amount;
        reward.amount_vote_given = vote_given_amount;
        reward.amount_extra = double_to_asset(0.0);
        reward.amount_total = amount_total;
      });
    }
  });

  distribution.undistributed_pot = (pot - distribution.distributed_pot);
}

void shine::distribute_pot_balance_to_first_member(const asset& balance, distribution_stat& distribution) {
  auto& stat = *(stats.begin());

  distribution.distributed_pot += balance;

  update_reward_for_member(stat.member, [&](auto& reward) {
    reward.amount_extra = balance;
    reward.amount_total += balance;
  });
}

void shine::distribute_pot_balance_to_bound_members(asset balance, distribution_stat& distribution) {
  auto redistribution_weight = 1 / (double)distribution.distributed_count;
  auto undistributed_amount = asset_to_double(distribution.undistributed_pot);

  auto last_member_itr = find_last_stats_member_itr();
  for (auto itr = stats.begin(); itr != stats.end(); itr = itr++) {
    auto& stat = *itr;
    if (!has_account(stat.member)) continue;

    auto amount_extra = double_to_asset(undistributed_amount * redistribution_weight);

    // Re-compute balance and dispatch it to amount total if last member
    balance -= amount_extra;
    if (itr == last_member_itr) {
      amount_extra += balance;
    }

    distribution.distributed_pot += amount_extra;

    update_reward_for_member(stat.member, [&](auto& reward) {
      reward.amount_extra = amount_extra;
      reward.amount_total += amount_extra;
    });
  }
}

///
//// Helpers
///

void shine::update_reward_for_member(const member_id& member, const function<void(reward&)> updater) {
  auto reward_member_index = rewards.template get_index<N(member)>();

  auto reward_itr = reward_member_index.find(compute_checksum256_key(member));
  if (reward_itr == reward_member_index.end()) {
    rewards.emplace(_self, [&](auto& reward) {
      reward.id = rewards.available_primary_key();
      reward.member = member;
      updater(reward);
    });
  } else {
    reward_member_index.modify(reward_itr, _self, [&](auto& reward) { updater(reward); });
  }
}

void shine::update_member_stat(const member_id& member, const function<void(member_stat&)> updater) {
  auto member_stats_index = stats.template get_index<N(member)>();

  auto stat_itr = member_stats_index.find(compute_checksum256_key(member));
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

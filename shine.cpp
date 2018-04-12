#include "shine.hpp";

const asset_symbol shine::EOS_SYMBOL = S(CURRENCY_PRECISION, EOS);

/**
 * Macro that auto-create the `apply` SmartContract C entrypoint
 * by matching the action received based on the list of action in the
 * macro definition. For each of defined action, a switch branch is added
 * automatically unpacking the data into the action's structure and dispatching
 * a method call to `action` define in this SmartContract.
 *
 * Each time a new action is added, this definition should be expanded with the
 * new action handler's method name.
 */
EOSIO_ABI(shine, (addpraise)(addvote)(calcrewards))

//@abi action
void shine::addpraise(const member_id& author, const member_id& praisee, const string& memo) {
    require_auth(_self);

    auto praise_itr = praises.emplace(_self, [&](auto& praise) {
        praise.id = praises.available_primary_key();
        praise.author = author;
        praise.praisee = praisee;
        praise.memo = memo;
    });

    // Author stats update (praise_posted)
    update_member_stat(author, [](auto& stat) {
        stat.praise_posted += 1;
    });

    // Praisee stats update (vote_received_implicit)
    update_member_stat(praisee, [](auto& stat) {
        stat.vote_received_implicit += 1;
    });

    // Global stats update (vote_implicit)
    update_global_stat([](auto& global_stat) {
        global_stat.vote_implicit += 1;
    });
}

//@abi action
void shine::addvote(const uint64_t praise_id, const member_id& voter) {
    require_auth(_self);

    auto praise_itr = praises.find(praise_id);
    eosio_assert(praise_itr != praises.end(), "praise with this id does not exist");

    votes.emplace(_self, [&](auto& vote) {
        vote.id = votes.available_primary_key();
        vote.praise_id = praise_id;
        vote.voter = voter;
    });

    // Voter stats update (vote_given_explicit)
    update_member_stat(voter, [](auto& stat) {
        stat.vote_given_explicit += 1;
    });

    // Praisee update (vote_received_explicit)
    update_member_stat(praise_itr->praisee, [](auto& stat) {
        stat.vote_received_explicit += 1;
    });

    // Praise author update (praise_vote_received)
    update_member_stat(praise_itr->author, [](auto& stat) {
        stat.praise_vote_received += 1;
    });

    // Global stats update
    update_global_stat([](auto& global_stat) {
        global_stat.vote_explicit += 1;
    });
}

void shine::calcrewards(const asset& pot) {
    auto symbol = pot.symbol;
    eosio_assert(symbol.is_valid() && symbol == EOS_SYMBOL, "pot currency should be EOS with precision 4");

    auto global_stats_itr = global_stats.find(GLOBAL_STAT_ID);
    eosio_assert(global_stats_itr != global_stats.end(), "cannot compute rewards without any praise");

    auto pot_amount = asset_to_double(pot);
    auto vote_implicit = global_stats_itr->vote_implicit;
    auto vote_explicit = global_stats_itr->vote_explicit;
    auto vote_total = vote_implicit + vote_explicit;


    // This loop through all stats giving 2N iterations. Ideally, this could
    // probably eliminated somehow. At least, computation of `compute_vote_given_weight`
    // could be cached so it would not be redo in the second loop that follows.
    auto vote_given_weighted_total = compute_vote_given_weighted_total();

    std::for_each(stats.begin(), stats.end(), [&](auto& stats) {
        auto vote_received = stats.vote_received_explicit + stats.vote_received_implicit;
        auto praise_vote_received = stats.praise_vote_received;
        auto vote_given = stats.vote_given_explicit;

        auto vote_given_weights = compute_vote_given_weight(vote_given);
        auto vote_given_weighted = vote_given * vote_given_weights;

        auto vote_received_weight = vote_received / (double) vote_total;
        auto praise_posted_weight = praise_vote_received / (double) vote_explicit;
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
        global_stats.modify(global_stat_itr, _self, [&](auto& global_stat) {
            updater(global_stat);
        });
    }
}

void shine::update_reward_for_member(
    const uint64_t id,
    const member_id& member,
    const std::function<void(reward&)> updater
) {
    auto reward_itr = rewards.find(id);
    if (reward_itr == rewards.end()) {
        rewards.emplace(_self, [&](auto& reward) {
            reward.id = id;
            reward.member = member;
            updater(reward);
        });
    } else {
        rewards.modify(reward_itr, _self, [&](auto& reward) {
            updater(reward);
        });
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
        member_stats_index.modify(stat_itr, _self, [&](auto& stat) {
            updater(stat);
        });
    }
}

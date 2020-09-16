#include "shine.hpp"

///
/// Actions
///

void shine::post(const name org, const bool org_auth, const name from, const name to, const string& memo) {
  name payer;
  if (org_auth) {
    require_auth(org);
    payer = org;

    // Register accounts only if it's the organization signing.
    if (!shine_account_exists(org, from)) {
      register_member(org, from, empty_name, "");
    }
    if (!shine_account_exists(org, to)) {
      register_member(org, to, empty_name, "");
    }
  } else {
    check(false, "authenticating shine accounts for posting not supported yet");
    // name onchain_account = get_onchain_account_for_shine_account(org, voter);
    // require_auth(onchain_account);
    // payer = onchain_account;
  }

  configs_index configs(get_self(), org.value);
  auto config = configs.find("config"_n.value);
  check(config != configs.end(), "organization not configured");

  configs.modify(config, eosio::same_payer, [&](auto& row) {
    row.last_post_id++;
  });

  posts_index posts(get_self(), org.value);
  auto post_itr = posts.emplace(payer, [&](auto& post) {
    post.id = config->last_post_id;
    post.from = from;
    post.to = to;
    post.memo = memo;
  });

  update_weight(org, payer, from, [&](auto& row) { row.weight += config->poster_weight; });
  update_weight(org, payer, to, [&](auto& row) { row.weight += config->recipient_weight; });
}

void shine::vote(const name org, const bool org_auth, const name voter, const post_id post_id) {
  name payer;
  if (org_auth) {
    require_auth(org);
    payer = org;

    // Register accounts only if it's the organization signing.
    if (!shine_account_exists(org, voter)) {
      register_member(org, voter, empty_name, "");
    }
  } else {
    name onchain_account = get_onchain_account_for_shine_account(org, voter);
    require_auth(onchain_account);
    payer = onchain_account;
  }

  posts_index posts(get_self(), org.value);
  auto post_itr = posts.find(post_id);
  check(post_itr != posts.end(), "post with this id does not exist.");

  uint64_t seenhash = post_id ^ voter.value; // simple hash (!)
  seen_index seentable(get_self(), org.value);
  auto seen_itr = seentable.find(seenhash);
  check(seen_itr == seentable.end(), "voter already voted for that post");

  seentable.emplace(payer, [&](auto& row) {
    row.seenhash = seenhash;
  });

  configs_index configs(get_self(), org.value);
  auto config = configs.find("config"_n.value);
  check(config != configs.end(), "organization not configured");

  update_weight(org, payer, voter, [&](auto& row) { row.weight += config->voter_weight; });
  update_weight(org, payer, post_itr->to, [&](auto& row) { row.weight += config->recipient_weight; });
  update_weight(org, payer, post_itr->from, [&](auto& row) { row.weight += config->poster_weight; });
}

void shine::regaccount(const name org, const name shine_account, const name onchain_account, const string offchain_account){
  eosio::print("shine - regaccount\n");
  require_auth(org);

  register_member(org, shine_account, onchain_account, offchain_account);
}

void shine::unregaccount(const name org, const name shine_account){
  eosio::print("shine - unregaccount\n");
  require_auth(org);

  unregister_member(org, shine_account);
}

/**
 * Reset statistics for posts, votes, stats and rewards. Mapping between
 * member and account will be kept.
 */
void shine::reset(const name org) {
  eosio::print("shine - reset\n");
  require_auth(org);

  posts_index posts(get_self(), org.value);
  table_clear(posts);

  seen_index seentbl(get_self(), org.value);
  table_clear(seentbl);

  weights_index weights(get_self(), org.value);
  auto itr = weights.begin();
  while (itr != weights.end()) {
    weights.modify(itr, eosio::same_payer, [&](auto& row) {
      row.weight = 0;
    });
  }
}

/**
 * `purgeall` is a system call authorized by the contract account, to remove all
 * rows, including the configuration and members from an organization.
 */
void shine::purgeall(const name org) {
  //check(false, "purge all disabled");

  eosio::print("shine - purgeall\n");
  require_auth(get_self());

  posts_index posts(get_self(), org.value);
  table_clear(posts);

  seen_index seentbl(get_self(), org.value);
  table_clear(seentbl);

  weights_index weights(get_self(), org.value);
  table_clear(weights);

  members_index members(get_self(), org.value);
  table_clear(members);

  configs_index configs(get_self(), org.value);
  table_clear(configs);
}

void shine::configure(const name org, uint64_t recipient_weight, uint64_t voter_weight, uint64_t poster_weight) {
  eosio::print("shine - configure\n");
  // if config doesn't exist, create it
  // otherwise, modify it
  require_auth(org);

  configs_index configs(get_self(), org.value);
  auto config_itr = configs.find("config"_n.value);
  if (config_itr == configs.end()) {
    configs.emplace(org, [&](auto& row) {
      row.last_post_id = 1024;
      row.recipient_weight = recipient_weight;
      row.voter_weight = voter_weight;
      row.poster_weight = poster_weight;
    });
  } else {
    configs.modify(config_itr, eosio::same_payer, [&](auto& row) {
      row.recipient_weight = recipient_weight;
      row.voter_weight = voter_weight;
      row.poster_weight = poster_weight;
    });
  }
}

///
//// Notifications
///

/**
 * Transfer the actual pot into rewards to all members.
 */
void shine::on_transfer(const name from, const name to, const asset quantity, const string& memo) {
  if (to != _self) {
    return;
  }

  struct token_transfer {
    name from;
    name to;
    asset quantity;
    string memo;
  };

  // for_each(rewards.begin(), rewards.end(), [&](auto& reward) {
  //   token_transfer transfer{_self, reward.account, reward.amount_total, "Shine on you!"};
  //   eosio::action(permission_level{_self, "active"_n}, "eosio.token"_n, "transfer"_n, transfer).send();
  // });
}


///
//// Helpers
///

void shine::update_weight(const name org, const name payer, const name account, const function<void(weights_row&)> updater) {
  weights_index weights(get_self(), org.value);

  auto row_itr = weights.find(account.value);
  check(row_itr != weights.end(), "account to update weight does not exist"); // TODO add the account name in there!

  weights.modify(row_itr, eosio::same_payer, [&](auto& row) { updater(row); });
}

name shine::get_onchain_account_for_shine_account(const name org, const name account) {
  members_index members(get_self(), org.value);
  auto member_itr = members.find(account.value);
  check(member_itr != members.end(), "shine account not found");
  check(member_itr->onchain_account != empty_name, "no registered onchain account for the given shine account");
  return member_itr->onchain_account;
}

bool shine::shine_account_exists(const name org, const name account) {
  members_index members(get_self(), org.value);
  auto member_itr = members.find(account.value);
  if (member_itr != members.end()) {
    return true;
  }
  return false;
}

void shine::register_member(const name org, const name shine_account, const name onchain_account, const string offchain_account) {
  members_index members(get_self(), org.value);
  weights_index weights(get_self(), org.value);

  auto member_itr = members.find(shine_account.value);
  if (member_itr != members.end()) {
    members.modify(member_itr, eosio::same_payer, [&](auto& member) {
      member.onchain_account = onchain_account;
      member.offchain_account = offchain_account;
    });
  } else {
    members.emplace(org, [&](auto& member) {
      member.shine_account = shine_account;
      member.onchain_account = onchain_account;
      member.offchain_account = offchain_account;
    });
    weights.emplace(org, [&](auto& row) {
      row.shine_account = shine_account;
      row.weight = 0;
    });
  }
}

void shine::unregister_member(const name org, const name shine_account) {
  members_index members(get_self(), org.value);
  weights_index weights(get_self(), org.value);

  auto member_itr = members.find(shine_account.value);
  if (member_itr != members.end()) {
    members.erase(member_itr);
  }

  auto weight_itr = weights.find(shine_account.value);
  if (weight_itr != weights.end()) {
    weights.erase(weight_itr);
  }
}

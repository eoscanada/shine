#include "shine.hpp"

///
/// Actions
///

void shine::post(const name from, const name to, const string& memo) {
  require_auth(from);
  name payer = from;
  auto self = get_self();

  configs_index configs(self, self.value);
  auto config = configs.find("config"_n.value);
  check(config != configs.end(), "contract not configured");

  configs.modify(config, eosio::same_payer, [&](auto& row) {
    row.last_post_id++;
  });

  posts_index posts(self, self.value);
  auto post_itr = posts.emplace(payer, [&](auto& post) {
    post.id = config->last_post_id;
    post.from = from;
    post.to = to;
    post.memo = memo;
  });

  update_weight(payer, from, [&](auto& row) { row.weight += config->poster_weight; });
  update_weight(payer, to, [&](auto& row) { row.weight += config->recipient_weight; });
}

void shine::vote(const name voter, const post_id post_id) {
  require_auth(voter);

  name payer = voter;

  name self = get_self();

    posts_index posts(self, self.value);
  auto post_itr = posts.find(post_id);
  check(post_itr != posts.end(), "post with this id does not exist.");

  uint64_t seenhash = post_id ^ voter.value; // simple hash (!)
  seen_index seentable(self, self.value);
  auto seen_itr = seentable.find(seenhash);
  check(seen_itr == seentable.end(), "voter already voted for that post");

  seentable.emplace(payer, [&](auto& row) {
    row.seenhash = seenhash;
  });

  configs_index configs(self, self.value);
  auto config = configs.find("config"_n.value);
  check(config != configs.end(), "contract not configured");

  update_weight(payer, voter, [&](auto& row) { row.weight += config->voter_weight; });
  update_weight(payer, post_itr->to, [&](auto& row) { row.weight += config->recipient_weight; });
  update_weight(payer, post_itr->from, [&](auto& row) { row.weight += config->poster_weight; });
}

void shine::regaccount(const name account, const string slack_id){
  eosio::print("shine - regaccount\n");
  auto self = get_self();

  require_auth(self);
  check( is_account(account), "account does not exist on chain, create it first");

  members_index members(self, self.value);
  weights_index weights(self, self.value);

  auto member_itr = members.find(account.value);
  if (member_itr != members.end()) {
    members.modify(member_itr, eosio::same_payer, [&](auto& member) {
      member.account = account;
      member.slack_id = slack_id;
    });
  } else {
    members.emplace(self, [&](auto& member) {
      member.account = account;
      member.slack_id = slack_id;
    });
    weights.emplace(self, [&](auto& row) {
      row.account = account;
      row.weight = 0;
    });
  }
}

void shine::unregaccount(const name account){
  eosio::print("shine - unregaccount\n");
  auto self = get_self();
  require_auth(self);

  members_index members(self, self.value);
  weights_index weights(self, self.value);

  auto member_itr = members.find(account.value);
  if (member_itr != members.end()) {
    members.erase(member_itr);
  }

  auto weight_itr = weights.find(account.value);
  if (weight_itr != weights.end()) {
    weights.erase(weight_itr);
  }
}

/**
 * Reset statistics for posts, votes, stats and rewards. Mapping between
 * member and account will be kept.
 */
void shine::reset() {
  eosio::print("shine - reset\n");

  auto self = get_self();

  require_auth(self);

  posts_index posts(self, self.value);
  table_clear(posts);

  seen_index seentbl(self, self.value);
  table_clear(seentbl);

  weights_index weights(self, self.value);
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
void shine::purgeall() {
  //check(false, "purge all disabled");

  auto self = get_self();

  eosio::print("shine - purgeall\n");
  require_auth(self);

  posts_index posts(self, self.value);
  table_clear(posts);

  seen_index seentbl(self, self.value);
  table_clear(seentbl);

  weights_index weights(self, self.value);
  table_clear(weights);

  members_index members(self, self.value);
  table_clear(members);

  configs_index configs(self, self.value);
  table_clear(configs);
}

void shine::configure(uint64_t recipient_weight, uint64_t voter_weight, uint64_t poster_weight) {
  eosio::print("shine - configure\n");
  // if config doesn't exist, create it
  // otherwise, modify it
  auto self = get_self();

  require_auth(self);

  configs_index configs(self, self.value);
  auto config_itr = configs.find("config"_n.value);
  if (config_itr == configs.end()) {
    configs.emplace(self, [&](auto& row) {
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

void shine::update_weight(const name payer, const name account, const function<void(weights_row&)> updater) {
  auto self = get_self();
  weights_index weights(self, self.value);

  auto row_itr = weights.find(account.value);
  check(row_itr != weights.end(), "account to update weight does not exist"); // TODO add the account name in there!

  weights.modify(row_itr, eosio::same_payer, [&](auto& row) { updater(row); });
}

bool shine::is_member(const name account) {
  auto self = get_self();
  members_index members(self, self.value);
  auto member_itr = members.find(account.value);
  if (member_itr != members.end()) {
    return true;
  }
  return false;
}

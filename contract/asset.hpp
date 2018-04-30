#pragma once

#include <cmath>

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>

namespace eoscanada {

static const uint64_t EOS_PRECISION = 4;
static const asset_symbol EOS_SYMBOL = S(EOS_PRECISION, EOS);

inline static eosio::asset double_to_asset(double amount) {
  return eosio::asset((uint64_t)(pow(10, EOS_PRECISION) * amount), EOS_SYMBOL);
}

inline static double asset_to_double(const eosio::asset& asset) { return asset.amount / pow(10, EOS_PRECISION); }

}  // namespace eoscanada

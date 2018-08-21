#pragma once

#include <cmath>

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>

namespace eoscanada {

static const eosio::symbol_type EOS_SYMBOL = S(4, EOS);

inline static eosio::asset double_to_asset(double amount, eosio::symbol_type symbol) {
  return eosio::asset((uint64_t)(pow(10, symbol.precision()) * amount), symbol);
}

inline static double asset_to_double(const eosio::asset& asset) { return asset.amount / pow(10, asset.symbol.precision()); }

}  // namespace eoscanada

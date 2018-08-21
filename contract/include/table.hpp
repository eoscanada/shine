#pragma once

#include <vector>

#include <eosiolib/eosio.hpp>

namespace eoscanada {
namespace table {

/**
 * Clear completely a EOS table (`multi_index`) from all its data.
 */
template <uint64_t TableName, typename T, typename... Indices>
void clear(eosio::multi_index<TableName, T, Indices...>& table) {
  auto itr = table.begin();
  while (itr != table.end()) {
    itr = table.erase(itr);
  }
}

}  // namespace table
}  // namespace eoscanada

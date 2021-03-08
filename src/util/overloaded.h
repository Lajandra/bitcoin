// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UTIL_OVERLOADED_H
#define BITCOIN_UTIL_OVERLOADED_H

#include <optional>
#include <utility>

//! Overloaded helper for std::visit, useful to write code that switches on a
//! variant type and triggers a compile error if there are any unhandled cases.
//!
//! Implementation comes from and example usage can be found at
//! https://en.cppreference.com/w/cpp/utility/variant/visit#Example
template<class... Ts> struct Overloaded : Ts... { using Ts::operator()...; };

//! Explicit deduction guide (not needed as of C++20)
template<class... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

#endif // BITCOIN_UTIL_OVERLOADED_H

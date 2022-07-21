// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UTIL_RESULT_H
#define BITCOIN_UTIL_RESULT_H

#include <util/translation.h>

#include <variant>

namespace util {

struct Error { bilingual_str message; };

//! The util::Result class provides a standard way for functions to return error
//! and in addition to optional result values.
//!
//! It is intended for high-level functions that need to report error strings to
//! end users. Lower-level functions that don't need this error-reporting and
//! only need error-handling should avoid util::Result and instead use standard
//! classes like std::optional, std::variant, and std::tuple, or custom structs
//! and enum types to return function results.
//!
//! Usage examples can be found in \example ../test/result_tests.cpp, but in
//! general code returning `util::Result<T>` values is very similar to code
//! returning `std::optional<T>` values. Existing functions returning
//! `std::optional<T>` can be updated to return `util::Result<T>` and return
//! error strings usually just replacing `return std::nullopt;` with `return
//! util::Error{error_string};`.
template<class T>
class Result {
private:
    std::variant<bilingual_str, T> m_variant;

    template <typename FT>
    friend class Result;
    template <typename FT>
    friend bilingual_str ErrorString(const Result<FT>& result);

public:
    Result(T obj) : m_variant{std::move(obj)} {}
    Result(Error error) : m_variant{std::move(error.message)} {}
    template <typename OT>
    Result(Error error, Result<OT>&& other) : m_variant{std::move(std::get<0>(other.m_variant))} {}

    //! std::optional methods, so functions returning optional<T> can change to
    //! return Result<T> with minimal changes to existing code, and vice versa.
    bool has_value() const { return m_variant.index() == 1; }
    const T& value() const { assert(*this); return std::get<1>(m_variant); }
    T& value() { assert(*this); return std::get<1>(m_variant); }
    template <typename U>
    T value_or(const U& default_value) const { return has_value() ? value() : default_value; }
    operator bool() const { return has_value(); }
    const T* operator->() const { return &value(); }
    const T& operator*() const { return value(); }
    T* operator->() { return &value(); }
    T& operator*() { return value(); }
};

template <typename T>
bilingual_str ErrorString(const Result<T>& result)
{
    return result ? bilingual_str{} : std::get<0>(result.m_variant);
}
} // namespace util

#endif // BITCOIN_UTIL_RESULT_H

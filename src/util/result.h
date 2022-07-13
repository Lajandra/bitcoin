// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UTIL_RESULT_H
#define BITCOIN_UTIL_RESULT_H

#include <util/translation.h>
#include <variant>

namespace util {

template<typename E>
struct _ErrorType;

template<>
struct _ErrorType<void>
{
    template<typename Str, typename... Args>
    _ErrorType(Str&& str, Args&&... args) : str{std::forward<Str>(str)} {}

    bilingual_str str;
};

template<typename E>
struct _ErrorType : _ErrorType<void>
{
    template<typename Str, typename... Args>
    _ErrorType(Str&& _str, Args&&... args) : _ErrorType<void>{std::forward<Str>(_str)}, error(std::forward<Args>(args)...) {}

    E error;
};

template<typename... Args>
struct _ErrorArgs {
    std::tuple<Args&...> args;
};

template<typename... Args>
_ErrorArgs<Args...> Error(Args&&... args)
{
    return _ErrorArgs<Args...>{{args...}};
}

/**
 * Function return type similar to std::optional<T> except it can return error
 * data and an error string description if the function fails. This is intended
 * for high level functions that can produce error strings. Low level functions
 * that just need results and non-string errors should use std::optional or
 * std::variant directly instead.
 *
 * See unit tests in result_tests.cpp for example usages.
 */
template<typename T, typename E = void>
class Result {
private:
    using SuccessType = std::conditional_t<std::is_same<T, void>::value, std::monostate, T>;
    using ErrorType = _ErrorType<E>;
    using ResultType = std::variant<SuccessType, ErrorType>;
    ResultType m_result;
public:
    template<typename... Args>
    Result(Args&&... args) : m_result{std::in_place_index_t<0>(), std::forward<Args>(args)...} {}

    template<typename... Args>
    Result(_ErrorArgs<Args...> error) : m_result{std::make_from_tuple<ErrorType>(error.args)} {}

    // Methods for std::optional compatibility in success case.
    bool has_value() const { return m_result.index() == 0; }
    const SuccessType& value() const { assert(has_value()); return std::get<0>(m_result); }
    SuccessType& value() { assert(has_value()); return std::get<0>(m_result); }
    template<typename U> SuccessType value_or(const U& default_value) const
    {
        return has_value() ? value() : default_value;
    }
    operator bool() const { return has_value(); }
    const SuccessType* operator->() const { return &value(); }
    const SuccessType& operator*() const { return value(); }
    SuccessType* operator->() { return &value(); }
    SuccessType& operator*() { return value(); }

    // Methods for getting failure information in error case.
    auto& Error() { assert(!has_value()); return std::get<1>(m_result).error; }
    const auto& Error() const { assert(!has_value()); return std::get<1>(m_result).error; }
    const bilingual_str& ErrorDescription() const { assert(!has_value()); return std::get<1>(m_result).str; }
};

} // namespace util

/**
 * Backwards-compatible interface for util::Result class. New code should prefer
 * util::Result class which supports returning error information along with
 * result information and supports returing `void` and `bilingual_str` results.
*/
template<class T>
class BResult {
private:
    util::Result<T> m_result;

public:
    BResult() : m_result{util::Error(Untranslated(""))} {};
    BResult(const T& value) : m_result{value} {}
    BResult(const bilingual_str& error) : m_result{util::Error(error)} {}
    bool HasRes() const { return m_result.has_value(); }
    const T& GetObj() const { return m_result.value(); }
    const bilingual_str& GetError() const { return m_result.ErrorDescription(); }
    explicit operator bool() const { return m_result.has_value(); }
};

#endif // BITCOIN_UTIL_RESULT_H

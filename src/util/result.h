// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UTIL_RESULT_H
#define BITCOIN_UTIL_RESULT_H

#include <util/translation.h>

#include <memory>
#include <optional>
#include <tuple>
#include <variant>
#include <vector>

namespace util {
namespace detail {
//! Empty string list
const std::vector<bilingual_str> _EMPTY_LIST{};

//! Helper to move elements from one container to another.
template<typename T>
void MoveElements(T& src, T& dest)
{
    dest.insert(dest.end(), std::make_move_iterator(src.begin()), std::make_move_iterator(src.end()));
    src.clear();
}

//! Extra result information only allocated if there's at least one error or warning.
template<typename F>
struct ErrorInfo
{
    std::optional<std::conditional_t<std::is_same<F, void>::value, std::monostate, F>> failure;
    std::vector<bilingual_str> errors;
    std::vector<bilingual_str> warnings;
};

//! Base class of util::Result<T, F>
template<typename T, typename F>
class ResultBase;

//! Specialization of result containing no success value, only error information.
template<typename F>
class ResultBase<void, F>
{
protected:
    std::unique_ptr<ErrorInfo<F>> m_info;

    template<typename...Args>
    void InitFailure(Args&&... args)
    {
        if (!m_info) m_info = std::make_unique<ErrorInfo<F>>();
        m_info->failure.emplace(std::forward<Args>(args)...);
    }
    void InitValue() {}
    void MoveValue(ResultBase& other) {}
    void DestroyValue() {}

public:
    void AddError(bilingual_str error)
    {
        if (error.empty()) return;
        if (!m_info) m_info = std::make_unique<ErrorInfo<F>>();
        m_info->errors.emplace_back(std::move(error));
    }

    void AddWarning(bilingual_str warning)
    {
        if (warning.empty()) return;
        if (!m_info) m_info = std::make_unique<ErrorInfo<F>>();
        m_info->warnings.emplace_back(std::move(warning));
    }

    //! Success check.
    operator bool() const { return !m_info || !m_info->failure; }

    //! Error retrieval.
    template<typename _F = F>
    std::enable_if_t<!std::is_same<_F, void>::value, const _F&> GetFailure() const { assert(!*this); return *m_info->failure; }
    const std::vector<bilingual_str>& GetErrors() const { return m_info ? m_info->errors : _EMPTY_LIST; }
    const std::vector<bilingual_str>& GetWarnings() const { return m_info ? m_info->warnings : _EMPTY_LIST; }
    std::tuple<const std::vector<bilingual_str>&, const std::vector<bilingual_str>&> GetErrorsAndWarnings() const { return {GetErrors(), GetWarnings()}; }
};

//! Result base class holding optional T value and providing accessor methods.
template<typename T, typename F>
class ResultBase : public ResultBase<void, F>
{
 protected:
    union { T m_value; };

    ResultBase() {}
    ~ResultBase() {}

    template<typename... Args>
    void InitValue(Args&&... args)
    {
        new (&m_value) T{std::forward<Args>(args)...};
    }
    void MoveValue(ResultBase& other)
    {
        new (&m_value) T{std::move(other.m_value)};
    }
    void DestroyValue()
    {
        m_value.~T();
    }

public:
    //! std::optional methods, so functions returning std::optional<T> can be
    //! easily changed to Result<T> without affecting existing code, and vice
    //! versa.
    bool has_value() const { return *this; }
    const T& value() const { assert(*this); return m_value; }
    T& value() { assert(*this); return m_value; }
    template<typename U> T value_or(const U& default_value) const
    {
        return has_value() ? value() : default_value;
    }
    const T* operator->() const { return &value(); }
    const T& operator*() const { return value(); }
    T* operator->() { return &value(); }
    T& operator*() { return value(); }
};
} // namespace detail

//! Function result type intended for high-level functions that return error and
//! warning strings in addition to normal result types.
//!
//! The Result<T> class is meant to be a drop-in replacement for
//! std::optional<T> except it has additional methods to return error and
//! warning strings for error reporting.
//!
//! This class is not intended to be used by low-level functions that do not
//! return error or warning strings. These functions should use plain
//! std::optional or std::variant types instead.
//!
//! See unit tests in result_tests.cpp for example usages.
//!
//! Implementation note: Result class is optimized for the success case by
//! storing all error information in a unique_ptr and not allocating memory
//! unless an error or warning is actually generated.
template<typename T, typename F = void>
class Result;

//! Tag types for result constructors.
struct Error : bilingual_str {
    using bilingual_str::bilingual_str;
    explicit Error(bilingual_str str) : bilingual_str{std::move(str)} {}
};
struct Warning : bilingual_str {
    using bilingual_str::bilingual_str;
    explicit Warning(bilingual_str str) : bilingual_str{std::move(str)} {}
};
template<typename... R>
struct Chain {
    std::tuple<R&...> args;
    Chain(R&&... r) : args{r...} {}
};

template<typename... R>
Chain(R&&... r) -> Chain<R...>;

template<typename T, typename F>
class Result : public detail::ResultBase<T, F>
{
protected:
    template<typename FT, typename FE>
    friend class Result;

    //! Success case initializer.
    template<typename...Args>
    void Construct(Args&&... args)
    {
        this->InitValue(std::forward<Args>(args)...);
    }

    //! Error case initializer.
    template<typename...Args>
    void Construct(Error error, Args&&... args)
    {
        this->AddError(std::move(error));
        this->InitFailure(std::forward<Args>(args)...);
    }

    //! Warning case initializer.
    template<typename...Args>
    void Construct(Warning warning, Args&&... args)
    {
        this->AddWarning(std::move(warning));
        this->InitValue(std::forward<Args>(args)...);
    }

    //! Chained initializer.
    template<typename... Prev, typename... Args>
    void Construct(Chain<Prev...>&& chain, Args&&... args)
    {
        std::apply([this](auto& ...prev){ (..., this->AddPrev(prev)); }, chain.args);
        Construct(std::forward<Args>(args)...);
    }

    template<typename PrevT, typename PrevE>
    void AddPrev(Result<PrevT, PrevE>& prev)
    {
        if (prev.m_info && !this->m_info) {
            this->m_info.reset(new detail::ErrorInfo<F>{.errors{std::move(prev.m_info->errors)}, .warnings{std::move(prev.m_info->warnings)}});
        } else if (prev.m_info && this->m_info) {
            detail::MoveElements(prev.m_info->errors, this->m_info->errors);
            detail::MoveElements(prev.m_info->warnings, this->m_info->warnings);
        }
    }

public:
    template<typename... Args>
    Result(Args&&... args) { Construct(std::forward<Args>(args)...); }

    Result(Result&& other) { *this = std::move(other); }

    Result& operator=(Result&& other)
    {
        if (other.m_info) this->m_info.reset(new detail::ErrorInfo<F>{std::move(*other.m_info)});
        if (*this) this->MoveValue(other);
        return *this;
    }
};

//! Helper methods to format error strings.
bilingual_str ErrorString(const std::vector<bilingual_str>& errors);
bilingual_str ErrorString(const std::vector<bilingual_str>& errors, const std::vector<bilingual_str>& warnings);
template<typename T, typename F>
bilingual_str ErrorString(const Result<T, F>& result) { return ErrorString(result.GetErrors(), result.GetWarnings()); }
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
    BResult() : m_result{util::Error{}, Untranslated("")} {};
    BResult(const T& value) : m_result{value} {}
    BResult(T&& value) : m_result{std::move(value)} {}
    BResult(const bilingual_str& error) : m_result{util::Error{}, error} {}
    bool HasRes() const { return m_result.has_value(); }
    const T& GetObj() const { return m_result.value(); }
    T ReleaseObj() { return std::move(m_result.value()); }
    const bilingual_str& GetError() const { assert(!HasRes()); return m_result.GetErrors().back(); }
    explicit operator bool() const { return HasRes(); }
};

#endif // BITCOIN_UTIL_RESULT_H

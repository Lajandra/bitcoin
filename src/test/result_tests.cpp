// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <util/result.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(result_tests)

struct NoCopy
{
    NoCopy(int n) : m_n{std::make_unique<int>(n)} {}
    std::unique_ptr<int> m_n;
};

bool operator==(const NoCopy& a, const NoCopy& b)
{
    return *a.m_n == *b.m_n;
}

std::ostream& operator<<(std::ostream& os, const NoCopy& o)
{
    if (o.m_n) os << "NoCopy(" << *o.m_n << ")"; else os << "NoCopy(nullptr)";
    return os;
}

util::Result<int> IntFn(int i, bool success)
{
    if (success) return i;
    return {util::Error{Untranslated(strprintf("int %i error.", i))}};
}

util::Result<NoCopy> NoCopyFn(int i, bool success)
{
    if (success) return {i};
    return {util::Error{Untranslated(strprintf("nocopy %i error.", i))}};
}

template<typename T>
void ExpectResult(const util::Result<T>& result, bool success, const bilingual_str& str)
{
    BOOST_CHECK_EQUAL(bool(result), success);
    BOOST_CHECK_EQUAL(ErrorString(result).original, str.original);
    BOOST_CHECK_EQUAL(ErrorString(result).translated, str.translated);
}

template<typename T, typename... Args>
void ExpectSuccess(const util::Result<T>& result, const bilingual_str& str, Args&&... args)
{
    ExpectResult(result, true, str);
    BOOST_CHECK_EQUAL(result.has_value(), true);
    BOOST_CHECK_EQUAL(result.value(), T{std::forward<Args>(args)...});
    BOOST_CHECK_EQUAL(&result.value(), &*result);
}

template<typename T, typename... Args>
void ExpectFail(const util::Result<T>& result, const bilingual_str& str)
{
    ExpectResult(result, false, str);
}

BOOST_AUTO_TEST_CASE(check_returned)
{
    ExpectSuccess(IntFn(5, true), {}, 5);
    ExpectFail(IntFn(5, false), Untranslated("int 5 error."));
    ExpectSuccess(NoCopyFn(5, true), {}, 5);
    ExpectFail(NoCopyFn(5, false), Untranslated("nocopy 5 error."));
}

BOOST_AUTO_TEST_CASE(check_value_or)
{
    BOOST_CHECK_EQUAL(IntFn(10, true).value_or(20), 10);
    BOOST_CHECK_EQUAL(IntFn(10, false).value_or(20), 20);
    BOOST_CHECK_EQUAL(NoCopyFn(10, true).value_or(20), 10);
    BOOST_CHECK_EQUAL(NoCopyFn(10, false).value_or(20), 20);
}

BOOST_AUTO_TEST_SUITE_END()

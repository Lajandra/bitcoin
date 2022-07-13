// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <util/result.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(result_tests)

util::Result<void> VoidSuccessFn()
{
    return {};
}

util::Result<void> VoidFailFn()
{
    return util::Error(Untranslated("void fail"));
}

util::Result<int> IntSuccessFn(int ret)
{
    return {ret};
}

util::Result<int> IntFailFn()
{
    return util::Error(Untranslated("int fail"));
}

enum MultiErr { ERR1, ERR2 };

util::Result<int, MultiErr> MultiSuccessFn(int ret)
{
    return {ret};
}

util::Result<int, MultiErr> MultiFailFn(MultiErr err)
{
    return util::Error(Untranslated("multi fail"), err);
}

template<typename T, typename E>
void ExpectSuccess(const util::Result<T, E>& result)
{
    BOOST_CHECK(result.has_value());
}

template<typename T, typename E, typename... Args>
void ExpectSuccessValue(const util::Result<T, E>& result, Args&&... args)
{
    ExpectSuccess(result);
    BOOST_CHECK(result.value() == T{std::forward<Args>(args)...});
}

template<typename T, typename E>
void ExpectFail(const util::Result<T, E>& result, bilingual_str str)
{
    BOOST_CHECK(!result.has_value());
    BOOST_CHECK(result.ErrorDescription().original == str.original);
    BOOST_CHECK(result.ErrorDescription().translated == str.translated);
}

template<typename T, typename E, typename... Args>
void ExpectFailValue(const util::Result<T, E>& result, bilingual_str str, Args&&... args)
{
    ExpectFail(result, str);
    BOOST_CHECK(result.Error() == E{std::forward<Args>(args)...});
}

BOOST_AUTO_TEST_CASE(util_datadir)
{
    ExpectSuccess(VoidSuccessFn());
    ExpectFail(VoidFailFn(), Untranslated("void fail"));
    ExpectSuccessValue(IntSuccessFn(5), 5);
    ExpectFail(IntFailFn(), Untranslated("int fail"));
    ExpectSuccessValue(MultiSuccessFn(5), 5);
    ExpectFailValue(MultiFailFn(ERR2), Untranslated("int fail"), ERR2);
}

BOOST_AUTO_TEST_SUITE_END()

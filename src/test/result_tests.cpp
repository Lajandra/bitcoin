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

struct NoCopyNoMove
{
    NoCopyNoMove(int n) : m_n{n} {}
    NoCopyNoMove(const NoCopyNoMove&) = delete;
    NoCopyNoMove(NoCopyNoMove&&) = delete;
    int m_n;
};

bool operator==(const NoCopyNoMove& a, const NoCopyNoMove& b)
{
    return a.m_n == b.m_n;
}

std::ostream& operator<<(std::ostream& os, const NoCopyNoMove& o)
{
    os << "NoCopyNoMove(" << o.m_n << ")";
    return os;
}

util::Result<void> VoidSuccessFn()
{
    return {};
}

util::Result<void> VoidFailFn()
{
    return {util::Error{Untranslated("void fail")}};
}

util::Result<int> IntSuccessFn(int ret)
{
    return {ret};
}

util::Result<int> IntFailFn()
{
    return {util::Error{Untranslated("int fail")}};
}

enum FnError { ERR1, ERR2 };

util::Result<NoCopy, FnError> NoCopySuccessFn(int ret)
{
    return {ret};
}

util::Result<NoCopyNoMove, FnError> EnumFailFn(FnError ret)
{
    return {util::Error{Untranslated("status fail")}, ret};
}

util::Result<void> WarnFn()
{
    return {util::Warning{Untranslated("warn")}};
}

util::Result<int> MultiWarnFn(int ret)
{
    util::Result<void> result;
    for (int i = 0; i < ret; ++i) {
        result.AddWarning(strprintf(Untranslated("warn %i"), i));
    }
    return {util::Chain{result}, ret};
}

util::Result<void, int> ChainedFailFn(FnError arg, int ret)
{
    return {util::Chain{EnumFailFn(arg), EnumFailFn(arg)}, util::Error{Untranslated("chained fail")}, ret};
}


util::Result<NoCopyNoMove> NoCopyNoMoveSuccessFn(int ret)
{
    return {ret};
}

template<typename T, typename F>
void ExpectSuccess(const util::Result<T, F>& result)
{
    BOOST_CHECK(result);
}

template<typename T, typename F>
void ExpectFail(const util::Result<T, F>& result, bilingual_str str)
{
    BOOST_CHECK(!result);
    BOOST_CHECK_EQUAL(ErrorString(result).original, str.original);
    BOOST_CHECK_EQUAL(ErrorString(result).translated, str.translated);
}

template<typename T, typename F, typename... Args>
void ExpectSuccessValue(const util::Result<T, F>& result, Args&&... args)
{
    ExpectSuccess(result);
    BOOST_CHECK_EQUAL(result.has_value(), true);
    BOOST_CHECK_EQUAL(result.value(), T{std::forward<Args>(args)...});
    BOOST_CHECK_EQUAL(&result.value(), &*result);
}

template<typename T, typename F, typename... Args>
void ExpectFailValue(const util::Result<T, F>& result, bilingual_str str, Args&&... args)
{
    ExpectFail(result, str);
    BOOST_CHECK_EQUAL(result.GetFailure(), F{std::forward<Args>(args)...});
}

BOOST_AUTO_TEST_CASE(check_returned)
{
    ExpectSuccess(VoidSuccessFn());
    ExpectFail(VoidFailFn(), Untranslated("void fail"));
    ExpectSuccessValue(IntSuccessFn(5), 5);
    ExpectFail(IntFailFn(), Untranslated("int fail"));
    ExpectSuccessValue(NoCopySuccessFn(5), 5);
    ExpectFailValue(EnumFailFn(ERR2), Untranslated("status fail"), ERR2);
    ExpectFailValue(ChainedFailFn(ERR1, 5), Untranslated("status fail, status fail, chained fail"), 5);
    ExpectSuccessValue(NoCopyNoMoveSuccessFn(5), 5);
}

BOOST_AUTO_TEST_CASE(check_dereference_operators)
{
    util::Result<std::pair<int, std::string>> mutable_result;
    const auto& const_result{mutable_result};
    mutable_result.value() = {1, "23"};
    BOOST_CHECK_EQUAL(mutable_result->first, 1);
    BOOST_CHECK_EQUAL(const_result->second, "23");
    (*mutable_result).first = 5;
    BOOST_CHECK_EQUAL((*const_result).first, 5);
}

BOOST_AUTO_TEST_SUITE_END()

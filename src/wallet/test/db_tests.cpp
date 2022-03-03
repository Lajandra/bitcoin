// Copyright (c) 2018-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <fs.h>
#include <test/util/setup_common.h>
#include <wallet/bdb.h>

#include <fstream>
#include <memory>
#include <string>

namespace wallet {
BOOST_FIXTURE_TEST_SUITE(db_tests, BasicTestingSetup)

static std::shared_ptr<BerkeleyEnvironment> GetWalletEnv(const fs::path& path, fs::path& database_filename)
{
    fs::path data_file = BDBDataFile(path);
<<<<<<< HEAD
    database_filename = fs::PathToString(data_file.filename());
    return GetBerkeleyEnv(data_file.parent_path(), false);
||||||| parent of 67ca71e5b7e (Disallow more unsafe string->path conversions allowed by path append operators)
    database_filename = fs::PathToString(data_file.filename());
    return GetBerkeleyEnv(data_file.parent_path());
=======
    database_filename = data_file.filename();
    return GetBerkeleyEnv(data_file.parent_path());
>>>>>>> 67ca71e5b7e (Disallow more unsafe string->path conversions allowed by path append operators)
}

BOOST_AUTO_TEST_CASE(getwalletenv_file)
{
<<<<<<< HEAD
    std::string test_name = "test_name.dat";
    const fs::path datadir = m_args.GetDataDirNet();
||||||| parent of 67ca71e5b7e (Disallow more unsafe string->path conversions allowed by path append operators)
    std::string test_name = "test_name.dat";
    const fs::path datadir = gArgs.GetDataDirNet();
=======
    fs::path test_name = "test_name.dat";
    const fs::path datadir = gArgs.GetDataDirNet();
>>>>>>> 67ca71e5b7e (Disallow more unsafe string->path conversions allowed by path append operators)
    fs::path file_path = datadir / test_name;
    std::ofstream f{file_path};
    f.close();

    fs::path filename;
    std::shared_ptr<BerkeleyEnvironment> env = GetWalletEnv(file_path, filename);
    BOOST_CHECK_EQUAL(filename, test_name);
    BOOST_CHECK_EQUAL(env->Directory(), datadir);
}

BOOST_AUTO_TEST_CASE(getwalletenv_directory)
{
<<<<<<< HEAD
    std::string expected_name = "wallet.dat";
    const fs::path datadir = m_args.GetDataDirNet();
||||||| parent of 67ca71e5b7e (Disallow more unsafe string->path conversions allowed by path append operators)
    std::string expected_name = "wallet.dat";
    const fs::path datadir = gArgs.GetDataDirNet();
=======
    fs::path expected_name = "wallet.dat";
    const fs::path datadir = gArgs.GetDataDirNet();
>>>>>>> 67ca71e5b7e (Disallow more unsafe string->path conversions allowed by path append operators)

    fs::path filename;
    std::shared_ptr<BerkeleyEnvironment> env = GetWalletEnv(datadir, filename);
    BOOST_CHECK_EQUAL(filename, expected_name);
    BOOST_CHECK_EQUAL(env->Directory(), datadir);
}

BOOST_AUTO_TEST_CASE(getwalletenv_g_dbenvs_multiple)
{
<<<<<<< HEAD
    fs::path datadir = m_args.GetDataDirNet() / "1";
    fs::path datadir_2 = m_args.GetDataDirNet() / "2";
    std::string filename;
||||||| parent of 67ca71e5b7e (Disallow more unsafe string->path conversions allowed by path append operators)
    fs::path datadir = gArgs.GetDataDirNet() / "1";
    fs::path datadir_2 = gArgs.GetDataDirNet() / "2";
    std::string filename;
=======
    fs::path datadir = gArgs.GetDataDirNet() / "1";
    fs::path datadir_2 = gArgs.GetDataDirNet() / "2";
    fs::path filename;
>>>>>>> 67ca71e5b7e (Disallow more unsafe string->path conversions allowed by path append operators)

    std::shared_ptr<BerkeleyEnvironment> env_1 = GetWalletEnv(datadir, filename);
    std::shared_ptr<BerkeleyEnvironment> env_2 = GetWalletEnv(datadir, filename);
    std::shared_ptr<BerkeleyEnvironment> env_3 = GetWalletEnv(datadir_2, filename);

    BOOST_CHECK(env_1 == env_2);
    BOOST_CHECK(env_2 != env_3);
}

BOOST_AUTO_TEST_CASE(getwalletenv_g_dbenvs_free_instance)
{
    fs::path datadir = gArgs.GetDataDirNet() / "1";
    fs::path datadir_2 = gArgs.GetDataDirNet() / "2";
    fs::path filename;

    std::shared_ptr <BerkeleyEnvironment> env_1_a = GetWalletEnv(datadir, filename);
    std::shared_ptr <BerkeleyEnvironment> env_2_a = GetWalletEnv(datadir_2, filename);
    env_1_a.reset();

    std::shared_ptr<BerkeleyEnvironment> env_1_b = GetWalletEnv(datadir, filename);
    std::shared_ptr<BerkeleyEnvironment> env_2_b = GetWalletEnv(datadir_2, filename);

    BOOST_CHECK(env_1_a != env_1_b);
    BOOST_CHECK(env_2_a == env_2_b);
}

BOOST_AUTO_TEST_SUITE_END()
} // namespace wallet

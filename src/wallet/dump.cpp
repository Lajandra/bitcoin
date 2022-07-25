// Copyright (c) 2020-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/dump.h>

#include <fs.h>
#include <util/translation.h>
#include <wallet/wallet.h>

#include <algorithm>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace wallet {
static const std::string DUMP_MAGIC = "BITCOIN_CORE_WALLET_DUMP";
uint32_t DUMP_VERSION = 1;

util::Result<void> DumpWallet(const ArgsManager& args, CWallet& wallet)
{
    // Get the dumpfile
    std::string dump_filename = args.GetArg("-dumpfile", "");
    if (dump_filename.empty()) {
        return {util::Error{_("No dump file provided. To use dump, -dumpfile=<filename> must be provided.")}};
    }

    fs::path path = fs::PathFromString(dump_filename);
    path = fs::absolute(path);
    if (fs::exists(path)) {
        return {util::Error{strprintf(_("File %s already exists. If you are sure this is what you want, move it out of the way first."), fs::PathToString(path))}};
    }
    std::ofstream dump_file;
    dump_file.open(path);
    if (dump_file.fail()) {
        return {util::Error{strprintf(_("Unable to open %s for writing"), fs::PathToString(path))}};
    }

    HashWriter hasher{};

    WalletDatabase& db = wallet.GetDatabase();
    std::unique_ptr<DatabaseBatch> batch = db.MakeBatch();

    util::Result<void> ret;
    if (!batch->StartCursor()) {
        ret = {util::Error{_("Error: Couldn't create cursor into database")}};
    }

    // Write out a magic string with version
    std::string line = strprintf("%s,%u\n", DUMP_MAGIC, DUMP_VERSION);
    dump_file.write(line.data(), line.size());
    hasher.write(MakeByteSpan(line));

    // Write out the file format
    line = strprintf("%s,%s\n", "format", db.Format());
    dump_file.write(line.data(), line.size());
    hasher.write(MakeByteSpan(line));

    if (ret) {

        // Read the records
        while (true) {
            CDataStream ss_key(SER_DISK, CLIENT_VERSION);
            CDataStream ss_value(SER_DISK, CLIENT_VERSION);
            bool complete;
            if (batch->ReadAtCursor(ss_key, ss_value, complete) || complete) ret = {}; else ret = {util::Error{}};
            if (complete) {
                break;
            } else if (!ret) {
                ret.AddError(_("Error reading next record from wallet database"));
                break;
            }
            std::string key_str = HexStr(ss_key);
            std::string value_str = HexStr(ss_value);
            line = strprintf("%s,%s\n", key_str, value_str);
            dump_file.write(line.data(), line.size());
            hasher.write(MakeByteSpan(line));
        }
    }

    batch->CloseCursor();
    batch.reset();

    // Close the wallet after we're done with it. The caller won't be doing this
    wallet.Close();

    if (ret) {
        // Write the hash
        tfm::format(dump_file, "checksum,%s\n", HexStr(hasher.GetHash()));
        dump_file.close();
    } else {
        // Remove the dumpfile on failure
        dump_file.close();
        fs::remove(path);
    }

    return ret;
}

// The standard wallet deleter function blocks on the validation interface
// queue, which doesn't exist for the bitcoin-wallet. Define our own
// deleter here.
static void WalletToolReleaseWallet(CWallet* wallet)
{
    wallet->WalletLogPrintf("Releasing wallet\n");
    wallet->Close();
    delete wallet;
}

util::Result<void> CreateFromDump(const ArgsManager& args, const std::string& name, const fs::path& wallet_path)
{
    // Get the dumpfile
    std::string dump_filename = args.GetArg("-dumpfile", "");
    if (dump_filename.empty()) {
        return {util::Error{_("No dump file provided. To use createfromdump, -dumpfile=<filename> must be provided.")}};
    }

    fs::path dump_path = fs::PathFromString(dump_filename);
    dump_path = fs::absolute(dump_path);
    if (!fs::exists(dump_path)) {
        return {util::Error{strprintf(_("Dump file %s does not exist."), fs::PathToString(dump_path))}};
    }
    std::ifstream dump_file{dump_path};

    // Compute the checksum
    HashWriter hasher{};
    uint256 checksum;

    // Check the magic and version
    std::string magic_key;
    std::getline(dump_file, magic_key, ',');
    std::string version_value;
    std::getline(dump_file, version_value, '\n');
    if (magic_key != DUMP_MAGIC) {
        dump_file.close();
        return {util::Error{strprintf(_("Error: Dumpfile identifier record is incorrect. Got \"%s\", expected \"%s\"."), magic_key, DUMP_MAGIC)}};
    }
    // Check the version number (value of first record)
    uint32_t ver;
    if (!ParseUInt32(version_value, &ver)) {
        dump_file.close();
        return {util::Error{strprintf(_("Error: Unable to parse version %u as a uint32_t"), version_value)}};
    }
    if (ver != DUMP_VERSION) {
        dump_file.close();
        return {util::Error{strprintf(_("Error: Dumpfile version is not supported. This version of bitcoin-wallet only supports version 1 dumpfiles. Got dumpfile with version %s"), version_value)}};
    }
    std::string magic_hasher_line = strprintf("%s,%s\n", magic_key, version_value);
    hasher.write(MakeByteSpan(magic_hasher_line));

    // Get the stored file format
    std::string format_key;
    std::getline(dump_file, format_key, ',');
    std::string format_value;
    std::getline(dump_file, format_value, '\n');
    if (format_key != "format") {
        dump_file.close();
        return {util::Error{strprintf(_("Error: Dumpfile format record is incorrect. Got \"%s\", expected \"format\"."), format_key)}};
    }
    // Get the data file format with format_value as the default
    std::string file_format = args.GetArg("-format", format_value);
    if (file_format.empty()) {
        return {util::Error{_("No wallet file format provided. To use createfromdump, -format=<format> must be provided.")}};
    }
    DatabaseFormat data_format;
    if (file_format == "bdb") {
        data_format = DatabaseFormat::BERKELEY;
    } else if (file_format == "sqlite") {
        data_format = DatabaseFormat::SQLITE;
    } else {
        return {util::Error{strprintf(_("Unknown wallet file format \"%s\" provided. Please provide one of \"bdb\" or \"sqlite\"."), file_format)}};
    }
    util::Result<void> ret;
    if (file_format != format_value) {
        ret.AddWarning(strprintf(_("Warning: Dumpfile wallet format \"%s\" does not match command line specified format \"%s\"."), format_value, file_format));
    }
    std::string format_hasher_line = strprintf("%s,%s\n", format_key, format_value);
    hasher.write(MakeByteSpan(format_hasher_line));

    DatabaseOptions options;
    ReadDatabaseArgs(args, options);
    options.require_create = true;
    options.require_format = data_format;
    auto database = MakeDatabase(wallet_path, options) >> ret;
    if (!database) return {util::Error{}, std::move(ret)};

    // dummy chain interface
<<<<<<< HEAD
    bool ret = true;
    std::shared_ptr<CWallet> wallet(new CWallet(/*chain=*/nullptr, name, gArgs, std::move(database)), WalletToolReleaseWallet);
||||||| parent of 7bd578a5caf (refactor: Use util::Result class for wallet loading)
    bool ret = true;
    std::shared_ptr<CWallet> wallet(new CWallet(nullptr /* chain */, name, gArgs, std::move(database)), WalletToolReleaseWallet);
=======
    std::shared_ptr<CWallet> wallet(new CWallet(nullptr /* chain */, name, gArgs, std::move(*database)), WalletToolReleaseWallet);
>>>>>>> 7bd578a5caf (refactor: Use util::Result class for wallet loading)
    {
        LOCK(wallet->cs_wallet);
        DBErrors load_wallet_ret = wallet->LoadWallet();
        if (load_wallet_ret != DBErrors::LOAD_OK) {
            return {util::Error{strprintf(_("Error creating %s"), name)}, std::move(ret)};
        }

        // Get the database handle
        WalletDatabase& db = wallet->GetDatabase();
        std::unique_ptr<DatabaseBatch> batch = db.MakeBatch();
        batch->TxnBegin();

        // Read the records from the dump file and write them to the database
        while (dump_file.good()) {
            std::string key;
            std::getline(dump_file, key, ',');
            std::string value;
            std::getline(dump_file, value, '\n');

            if (key == "checksum") {
                std::vector<unsigned char> parsed_checksum = ParseHex(value);
                if (parsed_checksum.size() != checksum.size()) {
                    ret = {util::Error{Untranslated("Error: Checksum is not the correct size")}};
                    break;
                }
                std::copy(parsed_checksum.begin(), parsed_checksum.end(), checksum.begin());
                break;
            }

            std::string line = strprintf("%s,%s\n", key, value);
            hasher.write(MakeByteSpan(line));

            if (key.empty() || value.empty()) {
                continue;
            }

            if (!IsHex(key)) {
                ret = {util::Error{strprintf(_("Error: Got key that was not hex: %s"), key)}};
                break;
            }
            if (!IsHex(value)) {
                ret = {util::Error{strprintf(_("Error: Got value that was not hex: %s"), value)}};
                break;
            }

            std::vector<unsigned char> k = ParseHex(key);
            std::vector<unsigned char> v = ParseHex(value);

            CDataStream ss_key(k, SER_DISK, CLIENT_VERSION);
            CDataStream ss_value(v, SER_DISK, CLIENT_VERSION);

            if (!batch->Write(ss_key, ss_value)) {
                ret = {util::Error{strprintf(_("Error: Unable to write record to new wallet"))}};
                break;
            }
        }

        if (ret) {
            uint256 comp_checksum = hasher.GetHash();
            if (checksum.IsNull()) {
                ret = {util::Error{_("Error: Missing checksum")}};
            } else if (checksum != comp_checksum) {
                ret = {util::Error{strprintf(_("Error: Dumpfile checksum does not match. Computed %s, expected %s"), HexStr(comp_checksum), HexStr(checksum))}};
            }
        }

        if (ret) {
            batch->TxnCommit();
        } else {
            batch->TxnAbort();
        }

        batch.reset();

        dump_file.close();
    }
    wallet.reset(); // The pointer deleter will close the wallet for us.

    // Remove the wallet dir if we have a failure
    if (!ret) {
        fs::remove_all(wallet_path);
    }

    return ret;
}
} // namespace wallet

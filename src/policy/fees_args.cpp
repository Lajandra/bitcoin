#include <policy/fees_args.h>

#include <util/system.h>

namespace {
const char* FEE_ESTIMATES_FILENAME = "fee_estimates.dat";
} // namespace

fs::path FeeestPath(const ArgsManager& argsman)
{
    return argsman.GetDataDirNet() / FEE_ESTIMATES_FILENAME;
}

fs::path FeeestLogPath(const ArgsManager& args)
{
    fs::path log_filename = args.GetPathArg("-estlog");
    if (log_filename.empty()) return {};
    return args.GetDataDirNet() / log_filename;
}

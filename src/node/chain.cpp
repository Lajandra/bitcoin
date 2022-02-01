// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chain.h>
#include <interfaces/chain.h>
#include <uint256.h>

namespace node {
interfaces::BlockInfo MakeBlockInfo(const CBlockIndex* index, const CBlock* data)
{
    interfaces::BlockInfo info{index ? *index->phashBlock : uint256::ZERO};
    if (index) {
        info.prev_hash = index->pprev ? index->pprev->phashBlock : nullptr;
        info.height = index->nHeight;
        LOCK(::cs_main);
        info.file_number = index->nFile;
        info.data_pos = index->nStatus & BLOCK_HAVE_DATA ? index->nDataPos : -1;
        info.undo_pos = index->nStatus & BLOCK_HAVE_UNDO ? index->nUndoPos : -1;
    }
    info.data = data;
    return info;
}
} // namespace node

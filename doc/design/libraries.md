# Libraries

| Name                   | Description |
|------------------------|-------------|
| libbitcoin_cli         | RPC client functionality used by `bitcoin-cli` executable |
| libbitcoin_common      | Home for common functionality shared by different executables and libraries. Similar to `libbitcoin_util`, but higher-level (see [Dependencies](#dependencies). |
| libbitcoin_consensus   | Stable, backwards-compatible consensus functionality used by `libbitcoin_node` and `libbitcoin_wallet` and also exposed as a [shared library](../doc/shared-libraries.md). |
| libbitcoinconsensus    | Shared library build of static `libbitcoin_consensus` library |
| libbitcoin_kernel      | Consensus "engine" and support library used for validation by `libbitcoin_node` and also exposed as a [shared library](../doc/shared-libraries.md). |
| libbitcoinqt           | GUI functionality used by `bitcoin-qt` and `bitcoin-gui` executables |
| libbitcoin_ipc         | IPC functionality used by `bitcoin-node`, `bitcoin-wallet`, `bitcoin-gui` executables to communicate when [`--enable-multiprocess`](multiprocess.md) is used. |
| libbitcoin_node        | P2P and RPC server functionality used by `bitcoind` and `bitcoin-qt` executables. |
| libbitcoin_util        | Home for common functionality shared by different executables and libraries. Similar to `libbitcoin_common`, but lower-level (see [Dependencies](#dependencies). |
| libbitcoin_wallet      | Wallet functionality used by `bitcoind` and `bitcoin-wallet` executables. |
| libbitcoin_wallet_tool | Lower-level wallet functionality used by `bitcoin-wallet` executable. |
| libbitcoin_zmq         | [ZeroMQ](../zmq.md) functionality used by `bitcoind` and `bitcoin-qt` executables. |

## Conventions

- Most libraries are internal libraries and have APIs which are completely unstable! There are few or no restrictions on backwards compatibility or rules about external dependencies. Exceptions are `libbitcoin_consensus` and `libbitcoin_kernel` which have external interfaces documented at [../shared-libraries.md](../shared-libraries.md).

- Generally speaking each library should correspond to a source directory and a namespace. Source code organization is a work in progress, so it is true that some namespaces are applied inconsistently, and if you look at [`libbitcoin_*_SOURCES`](../../src/Makefile.am) lists you can see that many libraries pull in files from outside their source directory. But generally when working with libraries, it is good to follow an establishing pattern like:

  - `libbitcoin_node` code lives in `src/node/` in the `node::` namespace
  - `libbitcoin_wallet` code lives in `src/wallet/` in the `wallet::` namespace
  - `libbitcoin_ipc` code lives in `src/ipc/` in the `ipc::` namespace
  - `libbitcoin_util` code lives in `src/util/` in the `util::` namespace
  - `libbitcoin_consensus` code lives in `src/consensus/` in the `Consensus::` namespace

## Dependencies

- Libraries should generally be careful about what other libraries they depend on, and only reference symbols following the arrows shown in the dependency graph below:

```mermaid
graph TD;

bitcoin-cli-->libbitcoin_cli;

bitcoind-->libbitcoin_node;
bitcoind-->libbitcoin_wallet;

bitcoin-qt-->libbitcoin_node;
bitcoin-qt-->libbitcoinqt;
bitcoin-qt-->libbitcoin_wallet;

bitcoin-wallet-->libbitcoin_wallet;
bitcoin-wallet-->libbitcoin_wallet_tool;

libbitcoin_cli-->libbitcoin_common;
libbitcoin_cli-->libbitcoin_util;

libbitcoin_common-->libbitcoin_util;
libbitcoin_common-->libbitcoin_consensus;

libbitcoin_kernel-->libbitcoin_consensus;
libbitcoin_kernel-->libbitcoin_util;

libbitcoin_node-->libbitcoin_common;
libbitcoin_node-->libbitcoin_consensus;
libbitcoin_node-->libbitcoin_kernel;
libbitcoin_node-->libbitcoin_util;

libbitcoinqt-->libbitcoin_common;
libbitcoinqt-->libbitcoin_util;

libbitcoin_wallet-->libbitcoin_common;
libbitcoin_wallet-->libbitcoin_util;

libbitcoin_wallet_tool-->libbitcoin_util;
libbitcoin_wallet_tool-->libbitcoin_wallet;
```

- The graph shows what _symbols_ (functions and variables) from each library other libraries can call and reference directly, but it is not a call graph. For example, there is no arrow connecting `libbitcoin_wallet` and `libbitcoin_node` libraries, because these libraries are intended to be modular and not depend on each other's internal implementation details. But wallet code still is still able to call node code through the `interfaces::Chain` abstract class in [`interfaces/chain.h`](../../src/interfaces/chain.h) and node code calls wallet code through the `interfaces::ChainClient` and `interfaces::Chain::Notifications` abstract classes in the same file. In general, defining abstract classes in [`src/interfaces/`](../../src/interfaces/) can be a convenient way of avoiding unwanted direct dependencies or circular dependencies between libraries.

- `libbitcoin_consensus` should be a standalone dependency that any library can depend on, and it should not depend on any other libraries itself.

- `libbitcoin_util` should also be a standalone dependency that any library can depend on, and it should not depend on other internal libraries. (It does use currently use boost and univalue external libraries.)

- `libbitcoin_common` should serve a similar function as `libbitcoin_util` and be a place for miscellaneous code used by various daemon, GUI, and CLI applications and libraries to live. It should not depend on anything other than `libbitcoin_util` and `libbitcoin_consensus`. The boundary between _util_ and _common_ is a little fuzzy but historically _util_ has been used for more generic, lower-level things like parsing hex, and _common_ has been used for bitcoin-specific, higher-level things like parsing base58. The difference between _util_ and _common_ may become more important in the future depending on whether `libbitcoin_kernel` will be allowed to depend on `libbitcoin_common` (this is TBD). In general, if it is ever unclear whether it is better to add code to _util_ or _common_, it is probably better to add it to _common_ unless it is very generically useful.

- `libbitcoin_kernel` dependencies should be kept to minimum, and should probably be restricted to using `libbitcoin_util` and `libbitcoin_consensus`. Depending on how it evolves, there is a chance it may be ok for `libbitcoin_kernel` to depend on `libbitcoin_common` as well (for example if there is some scripting or signing functionality that kernel and wallet code both need to call, but cannot be added to _util_ or _consensus_). But the list of what `libbitcoin_kernel` can depend on is TBD.

- Probably, the only thing that should depend on `libbitcoin_kernel` internally should be `libbitcoin_node`. GUI and wallet libraries `libbitcoinqt` and `libbitcoin_wallet` in particular probably should not depend on `libbitcoin_kernel` and the unneeded functionality it pulls in, like block validation. To the extent that GUI and wallet code need scripting and signing functionality, they should be get able it from `libbitcoin_consensus` and `libbitcoin_common`, not `libbitcoin_kernel`.

- GUI, node, and wallet code internal implementations should all be independent of each other, and the `libbitcoinqt`, `libbitcoin_node`, `libbitcoin_wallet` libraries should never reference each other's symbols. They should only call each other through [`src/interfaces/`](`../../src/interfaces/`) abstract interfaces.

## Work in progress

- Validation code is moving from `libbitcoin_node` to `libbitcoin_kernel` as part of [The libbitcoinkernel Project #24303](https://github.com/bitcoin/bitcoin/issues/24303)
- Source code organization is discussed in general in [Library source code organization #15732](https://github.com/bitcoin/bitcoin/issues/15732)
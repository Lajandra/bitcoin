# Copyright (c) 2021 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

@0xcd2c6232cb484a28;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("ipc::capnp::messages");

using Proxy = import "/mp/proxy.capnp";
$Proxy.include("ipc/capnp/common.h");
$Proxy.includeTypes("ipc/capnp/common-types.h");

struct Settings $Proxy.wrap("util::Settings") {
   forcedSettings @0 :List(Pair(Text, Text)) $Proxy.name("forced_settings");
   commandLineOptions @1 :List(Pair(Text, List(Text))) $Proxy.name("command_line_options");
   rwSettings @2 :List(Pair(Text, Text)) $Proxy.name("rw_settings");
   roConfig @3 :List(Pair(Text, List(Pair(Text, List(Text))))) $Proxy.name("ro_config");
}

struct GlobalArgs $Proxy.count(0) {
   settings @0 :Settings;
}

struct BilingualStr $Proxy.wrap("bilingual_str") {
    original @0 :Text;
    translated @1 :Text;
}

struct Pair(Key, Value) {
    key @0 :Key;
    value @1 :Value;
}

struct PairStr64 {
    key @0 :Text;
    value @1 :UInt64;
}

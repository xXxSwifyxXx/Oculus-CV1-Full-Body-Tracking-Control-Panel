#pragma once

#include <string>

namespace odtkra {

struct SteamDiscoveryResult {
    std::string steamRoot;
    std::string steamLibrary;
    std::string steamVrRoot;
    std::string steamVrDrivers;
    bool found = false;
};

SteamDiscoveryResult detect_steamvr();

}

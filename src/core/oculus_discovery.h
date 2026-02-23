#pragma once

#include <string>

namespace odtkra {

struct OculusDiscoveryResult {
    std::string diagnosticsDir;
    std::string oculusClientExe;
    std::string oculusDebugToolCliExe;
    bool foundAny = false;
};

OculusDiscoveryResult detect_oculus_paths();

}

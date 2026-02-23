/*
 * File: src/core/oculus_discovery.cpp
 *
 * Purpose:
 *   Implements Oculus/Meta runtime path discovery by scanning known installation roots.
 *
 * Design Notes:
 *   - This file is part of the production-grade refactor where responsibilities are intentionally split.
 *   - The intent is to keep logic predictable, observable, and recoverable under partial-runtime scenarios.
 *   - Error paths are expected in real user environments (missing Oculus runtime, missing SteamVR, permissions).
 *
 * Maintenance Guidance:
 *   - Keep behavior deterministic and avoid hidden side effects.
 *   - Prefer explicit logging and explicit return values over implicit assumptions.
 *   - If a change alters runtime behavior, update tests and diagnostics messaging in the same change.
 */
#include "core/oculus_discovery.h"

#include "core/path_utils.h"

#include <cctype>
#include <filesystem>
#include <vector>

namespace odtkra {

namespace {

bool equals_ci(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        const char ca = static_cast<char>(std::tolower(static_cast<unsigned char>(a[i])));
        const char cb = static_cast<char>(std::tolower(static_cast<unsigned char>(b[i])));
        if (ca != cb) {
            return false;
        }
    }
    return true;
}

void scan_root(const std::string& root, OculusDiscoveryResult* out) {
    if (root.empty() || !path_exists(root)) {
        return;
    }

    std::error_code ec;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root, ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_regular_file()) {
            continue;
        }

        const auto filename = entry.path().filename().string();
        if (out->oculusDebugToolCliExe.empty() && equals_ci(filename, "OculusDebugToolCLI.exe")) {
            out->oculusDebugToolCliExe = entry.path().string();
            out->diagnosticsDir = entry.path().parent_path().string();
            out->foundAny = true;
        }

        if (out->oculusClientExe.empty() && equals_ci(filename, "OculusClient.exe")) {
            out->oculusClientExe = entry.path().string();
            out->foundAny = true;
        }

        if (!out->oculusDebugToolCliExe.empty() && !out->oculusClientExe.empty()) {
            return;
        }
    }
}

} // namespace

OculusDiscoveryResult detect_oculus_paths() {
    OculusDiscoveryResult result;

    const std::vector<std::string> roots = {
        "C:\\Program Files\\Meta Horizon",
        "C:\\Program Files\\Meta",
        "C:\\Program Files\\Oculus"
    };

    for (const auto& root : roots) {
        scan_root(root, &result);
        if (!result.oculusDebugToolCliExe.empty() && !result.oculusClientExe.empty()) {
            break;
        }
    }

    return result;
}

}

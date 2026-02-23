/*
 * File: src/core/oculus_discovery.h
 *
 * Purpose:
 *   Declares Oculus path discovery result model and lookup function.
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

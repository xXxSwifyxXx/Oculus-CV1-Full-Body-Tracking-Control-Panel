/*
 * File: src/core/steam_discovery.h
 *
 * Purpose:
 *   Declares Steam/SteamVR discovery model and public lookup API.
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

struct SteamDiscoveryResult {
    std::string steamRoot;
    std::string steamLibrary;
    std::string steamVrRoot;
    std::string steamVrDrivers;
    bool found = false;
};

SteamDiscoveryResult detect_steamvr();

}

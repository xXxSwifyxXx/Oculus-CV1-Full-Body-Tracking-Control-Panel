/*
 * File: src/core/path_utils.h
 *
 * Purpose:
 *   Declares shared path helper utilities for runtime and installer flows.
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

std::string get_program_data_dir();
std::string join_path(const std::string& left, const std::string& right);
bool ensure_directory(const std::string& path);
bool path_exists(const std::string& path);
std::string executable_dir();

}

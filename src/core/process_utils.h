/*
 * File: src/core/process_utils.h
 *
 * Purpose:
 *   Declares process and command utility functions used by runtime operations.
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
#include <vector>

namespace odtkra {

int find_process_id(const std::wstring& process_name);
int run_command(const std::string& command_line, std::string* output = nullptr);
int run_command_with_timeout(const std::string& command_line, int timeout_ms);
std::vector<std::string> tail_file(const std::string& path, int max_lines);

}

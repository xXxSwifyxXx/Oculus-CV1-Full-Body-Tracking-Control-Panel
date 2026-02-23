/*
 * File: src/core/path_utils.cpp
 *
 * Purpose:
 *   Provides normalized Windows path helpers and filesystem convenience functions used by all modules.
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
#include "core/path_utils.h"

#include <Windows.h>
#include <ShlObj.h>

#include <cstdlib>
#include <filesystem>

namespace odtkra {

std::string get_program_data_dir() {
    char buffer[MAX_PATH] = {0};
    if (SHGetFolderPathA(nullptr, CSIDL_COMMON_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buffer) == S_OK) {
        return std::string(buffer);
    }

    const char* env = std::getenv("ProgramData");
    if (env != nullptr) {
        return std::string(env);
    }
    return ".";
}

std::string join_path(const std::string& left, const std::string& right) {
    if (left.empty()) {
        return right;
    }
    if (right.empty()) {
        return left;
    }
    if (left.back() == '\\' || left.back() == '/') {
        return left + right;
    }
    return left + "\\" + right;
}

bool ensure_directory(const std::string& path) {
    if (path.empty()) {
        return false;
    }
    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        return true;
    }
    return std::filesystem::create_directories(path, ec);
}

bool path_exists(const std::string& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

std::string executable_dir() {
    char path[MAX_PATH] = {0};
    const auto count = GetModuleFileNameA(nullptr, path, MAX_PATH);
    if (count == 0) {
        return ".";
    }
    std::string full(path, count);
    const auto slash = full.find_last_of("\\/");
    if (slash == std::string::npos) {
        return ".";
    }
    return full.substr(0, slash);
}

}

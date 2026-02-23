/*
 * File: src/core/touchlink_installer.cpp
 *
 * Purpose:
 *   Implements download, extraction, verification, and installation/update flow for OculusTouchLink driver files.
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
#include "core/touchlink_installer.h"

#include "core/path_utils.h"
#include "core/process_utils.h"
#include "core/steam_discovery.h"

#include <Windows.h>
#include <urlmon.h>

#include <filesystem>
#include <sstream>

#pragma comment(lib, "urlmon.lib")

namespace odtkra {

namespace {

// We pull from main branch zip to keep installation simple and dependency-free.
// A future hardening step could pin to commit hash for strict reproducibility.
const char* kRepoZipUrl = "https://github.com/mm0zct/Oculus_Touch_Steam_Link/archive/refs/heads/main.zip";

std::string quote(const std::string& value) {
    return "\"" + value + "\"";
}

std::string temp_dir() {
    // Windows temp path is used so extraction does not require elevated rights.
    char buffer[MAX_PATH] = {0};
    const DWORD len = GetTempPathA(MAX_PATH, buffer);
    if (len == 0 || len > MAX_PATH) {
        return ".";
    }
    return std::string(buffer, len);
}

} // namespace

TouchLinkInstaller::TouchLinkInstaller(Logger* logger) : logger_(logger) {}

std::string TouchLinkInstaller::resolve_target_path(const AppConfig& config, const std::string& steamvr_drivers_path) const {
    // Resolution priority:
    // 1) explicit function argument
    // 2) config value (if valid)
    // 3) discovery fallback
    if (!steamvr_drivers_path.empty()) {
        return steamvr_drivers_path;
    }
    if (path_exists(config.steamVrDriversPath)) {
        return config.steamVrDriversPath;
    }
    const auto discovery = detect_steamvr();
    return discovery.steamVrDrivers;
}

bool TouchLinkInstaller::backup_existing(const std::string& target) const {
    // Keep one rolling backup folder to reduce accidental data loss.
    const std::string existing = join_path(target, "OculusTouchLink");
    if (!path_exists(existing)) {
        return true;
    }

    const std::string backup = join_path(target, "OculusTouchLink.backup");
    std::error_code ec;
    std::filesystem::remove_all(backup, ec);
    std::filesystem::rename(existing, backup, ec);
    return !ec;
}

bool TouchLinkInstaller::verify_driver_files(const std::string& driver_root, std::string* missing_file) const {
    // Verification is manifest-based instead of filename-based so we can detect
    // valid driver payloads even if folder/file naming evolves upstream.
    std::error_code ec;
    if (!std::filesystem::exists(driver_root, ec)) {
        if (missing_file != nullptr) {
            *missing_file = "driver root missing: " + driver_root;
        }
        return false;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(driver_root, ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto name = entry.path().filename().string();
        if (name.find(".vrdrivermanifest") != std::string::npos) {
            return true;
        }
    }

    if (missing_file != nullptr) {
        *missing_file = "No *.vrdrivermanifest found under " + driver_root;
    }
    return false;
}

std::vector<std::string> TouchLinkInstaller::list_top_level_entries(const std::string& path) const {
    // Debug helper for verbose error reporting in UI dialogs.
    std::vector<std::string> entries;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
        if (ec) {
            break;
        }
        entries.push_back(entry.path().filename().string());
    }
    return entries;
}

TouchLinkInstallResult TouchLinkInstaller::install_or_update(const AppConfig& config, const std::string& steamvr_drivers_path) {
    TouchLinkInstallResult result;

    // Phase 1: path resolution and permission-safe directory preparation.
    const std::string target_drivers = resolve_target_path(config, steamvr_drivers_path);
    logger_->write(LogLevel::Info, "TouchLink install requested. target_drivers=" + target_drivers);
    if (!ensure_directory(target_drivers)) {
        result.message = "Unable to create/access SteamVR drivers directory: " + target_drivers;
        logger_->write(LogLevel::Error, result.message);
        return result;
    }

    if (!backup_existing(target_drivers)) {
        result.message = "Backup of existing OculusTouchLink failed (permissions?).";
        logger_->write(LogLevel::Error, result.message);
        return result;
    }

    // Phase 2: download + extract source package.
    const std::string temp = temp_dir();
    const std::string zip_path = join_path(temp, "OculusTouchLink-main.zip");
    const std::string extract_path = join_path(temp, "OculusTouchLink-main");

    logger_->write(LogLevel::Info, "Downloading OculusTouchLink package from GitHub");
    const HRESULT hr = URLDownloadToFileA(nullptr, kRepoZipUrl, zip_path.c_str(), 0, nullptr);
    if (FAILED(hr)) {
        result.message = "Download failed from GitHub (URLDownloadToFileA).";
        logger_->write(LogLevel::Error, result.message);
        return result;
    }

    {
        std::string out;
        const int code = run_command("powershell -NoProfile -ExecutionPolicy Bypass -Command \"Remove-Item -Recurse -Force " + quote(extract_path) + " -ErrorAction SilentlyContinue; Expand-Archive -Path " + quote(zip_path) + " -DestinationPath " + quote(extract_path) + " -Force\"", &out);
        logger_->write(code == 0 ? LogLevel::Info : LogLevel::Error, "Expand-Archive exit=" + std::to_string(code) + " output=" + out);
        if (code != 0) {
            result.message = "Expand-Archive failed with exit code: " + std::to_string(code);
            return result;
        }
    }

    const std::string source = join_path(join_path(extract_path, "Oculus_Touch_Steam_Link-main"), "ReleasePackage\\OculusTouchLink");
    if (!path_exists(source)) {
        result.message = "Expected source folder not found: ReleasePackage/OculusTouchLink";
        logger_->write(LogLevel::Error, result.message);
        return result;
    }
    logger_->write(LogLevel::Info, "TouchLink source found: " + source);

    // Phase 3: copy driver files with native filesystem API (avoids shell
    // quoting edge cases such as Program Files (x86) path handling).
    const std::string target = join_path(target_drivers, "OculusTouchLink");
    {
        std::error_code ec;
        std::filesystem::create_directories(target, ec);
        if (ec) {
            result.message = "Failed to create target directory: " + target + " error=" + ec.message();
            logger_->write(LogLevel::Error, result.message);
            return result;
        }

        for (const auto& entry : std::filesystem::directory_iterator(target, ec)) {
            if (ec) {
                break;
            }
            std::filesystem::remove_all(entry.path(), ec);
            if (ec) {
                break;
            }
        }
        if (ec) {
            result.message = "Failed to clean target directory: " + target + " error=" + ec.message();
            logger_->write(LogLevel::Error, result.message);
            return result;
        }

        std::filesystem::copy(
            source,
            target,
            std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing,
            ec);
        if (ec) {
            result.message = "Filesystem copy failed from " + source + " to " + target + " error=" + ec.message();
            logger_->write(LogLevel::Error, result.message);
            return result;
        }

        logger_->write(LogLevel::Info, "TouchLink copied successfully with std::filesystem");
    }

    // Phase 4: optional SteamVR driver registration.
    const std::string vrpathreg = join_path(join_path(join_path(target_drivers, ".."), "bin\\win64"), "vrpathreg.exe");
    if (path_exists(vrpathreg)) {
        std::string out;
        const int reg_code = run_command("\"" + vrpathreg + "\" adddriver \"" + target + "\"", &out);
        logger_->write(reg_code == 0 ? LogLevel::Info : LogLevel::Warning, "vrpathreg adddriver exit=" + std::to_string(reg_code) + " output=" + out);
    } else {
        logger_->write(LogLevel::Warning, "vrpathreg.exe not found, skipped driver registration. path=" + vrpathreg);
    }

    // Phase 5: integrity verification and enriched error diagnostics.
    std::string missing;
    if (!verify_driver_files(target, &missing)) {
        const auto entries = list_top_level_entries(target);
        std::ostringstream detail;
        detail << "Post-copy verification failed: " << missing;
        if (!entries.empty()) {
            detail << " | top-level entries: ";
            for (size_t i = 0; i < entries.size(); ++i) {
                detail << entries[i];
                if (i + 1 < entries.size()) {
                    detail << ",";
                }
            }
        }
        result.message = detail.str();
        logger_->write(LogLevel::Error, result.message);
        return result;
    }

    // Success path with explicit final message consumed by both CLI and UI.
    result.success = true;
    result.installed_path = target;
    result.message = "OculusTouchLink installed successfully.";
    logger_->write(LogLevel::Info, result.message + " path=" + target);
    return result;
}

}

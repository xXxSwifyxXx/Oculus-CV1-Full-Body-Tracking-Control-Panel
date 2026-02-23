#include "core/anti_sleep_service.h"

#include "core/path_utils.h"
#include "core/process_utils.h"

#include <chrono>
#include <fstream>
#include <thread>

namespace odtkra {

AntiSleepService::AntiSleepService(AppConfig config, Logger* logger, std::string state_path)
    : config_(std::move(config)), logger_(logger), state_path_(std::move(state_path)), stop_requested_(false) {}

bool AntiSleepService::cli_exists() const {
    return path_exists(join_path(config_.oculusDiagnosticsPath, "OculusDebugToolCLI.exe"));
}

bool AntiSleepService::apply_cli_profile() {
    const std::string cli = join_path(config_.oculusDiagnosticsPath, "OculusDebugToolCLI.exe");
    const int ppd = run_command_with_timeout("echo service set-pixels-per-display-pixel-override 0.99 | \"" + cli + "\"", 10000);
    if (ppd != 0 && ppd != 124) {
        logger_->write(LogLevel::Error, "Failed pixels-per-display command. code=" + std::to_string(ppd) + " (timeout=10s)");
        return false;
    }
    if (ppd == 124) {
        logger_->write(LogLevel::Warning, "Pixels-per-display command timed out after 10s; treating as soft success (CLI is known to stay open).");
    }

    const int asw = run_command_with_timeout("echo server: asw.Off | \"" + cli + "\"", 10000);
    if (asw != 0 && asw != 124) {
        logger_->write(LogLevel::Warning, "ASW command failed. code=" + std::to_string(asw) + " (timeout=10s)");
    } else if (asw == 124) {
        logger_->write(LogLevel::Warning, "ASW command timed out after 10s; treating as soft success.");
    }

    return true;
}

bool AntiSleepService::write_state(bool running, bool active, const std::string& message) const {
    std::ofstream out(state_path_, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    out << "{\n";
    out << "  \"running\": " << (running ? "true" : "false") << ",\n";
    out << "  \"antiSleepActive\": " << (active ? "true" : "false") << ",\n";
    out << "  \"message\": \"" << message << "\"\n";
    out << "}\n";
    return true;
}

bool AntiSleepService::run_once(bool dry_run) {
    if (!cli_exists()) {
        logger_->write(LogLevel::Error, "OculusDebugToolCLI.exe not found. Update --path or config.");
        write_state(true, false, "CLI missing");
        return false;
    }

    if (dry_run) {
        logger_->write(LogLevel::Info, "Dry-run active. No runtime command executed.");
        write_state(true, false, "Dry-run check passed");
        return true;
    }

    const bool ok = apply_cli_profile();
    write_state(true, ok, ok ? "Anti-sleep refresh succeeded" : "Anti-sleep refresh failed");
    return ok;
}

int AntiSleepService::run_forever() {
    logger_->write(LogLevel::Info, "ODTKRA service loop started");
    write_state(true, false, "Service started");

    const int period_minutes = config_.refreshMinutes <= 0 ? 9 : config_.refreshMinutes;
    while (!stop_requested_.load()) {
        run_once(config_.dryRun);
        std::this_thread::sleep_for(std::chrono::minutes(period_minutes));
    }

    write_state(false, false, "Service stopped");
    logger_->write(LogLevel::Info, "ODTKRA service loop stopped");
    return 0;
}

}

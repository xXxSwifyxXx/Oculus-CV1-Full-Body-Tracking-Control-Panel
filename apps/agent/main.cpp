#include "core/anti_sleep_service.h"
#include "core/config.h"
#include "core/diagnostics.h"
#include "core/logger.h"
#include "core/oculus_discovery.h"
#include "core/path_utils.h"
#include "core/steam_discovery.h"
#include "core/touchlink_installer.h"

#include <iostream>

namespace {

std::string data_root() {
    const std::string base = odtkra::join_path(odtkra::get_program_data_dir(), "ODTKRA");
    odtkra::ensure_directory(base);
    return base;
}

bool has_arg(int argc, char* argv[], const std::string& needle) {
    for (int i = 1; i < argc; ++i) {
        if (needle == argv[i]) {
            return true;
        }
    }
    return false;
}

}

int main(int argc, char* argv[]) {
    const std::string root = data_root();
    const std::string config_path = odtkra::join_path(root, "config.json");
    const std::string log_path = odtkra::join_path(root, "odtkra.log");
    const std::string state_path = odtkra::join_path(root, "state.json");

    odtkra::ConfigStore store(config_path);
    auto config = store.load();
    config = odtkra::apply_cli_args(argc, argv, config);
    if (!odtkra::path_exists(odtkra::join_path(config.oculusDiagnosticsPath, "OculusDebugToolCLI.exe")) || !odtkra::path_exists(config.oculusClientPath)) {
        const auto oculus = odtkra::detect_oculus_paths();
        if (!oculus.diagnosticsDir.empty()) {
            config.oculusDiagnosticsPath = oculus.diagnosticsDir;
        }
        if (!oculus.oculusClientExe.empty()) {
            config.oculusClientPath = oculus.oculusClientExe;
        }
    }
    if (!odtkra::path_exists(config.steamVrDriversPath)) {
        const auto discovery = odtkra::detect_steamvr();
        config.steamVrDriversPath = discovery.steamVrDrivers;
    }
    store.save(config);

    odtkra::Logger logger(log_path);
    logger.write(odtkra::LogLevel::Info, "ODTKRA agent booting");

    if (has_arg(argc, argv, "--diagnostics")) {
        odtkra::Diagnostics diagnostics;
        const auto report = diagnostics.run(config, true);
        std::cout << report.summary_text();
        logger.write(report.success ? odtkra::LogLevel::Info : odtkra::LogLevel::Warning, "Diagnostics executed in dry-run mode");
        return report.success ? 0 : 2;
    }

    if (has_arg(argc, argv, "--install-touchlink")) {
        odtkra::TouchLinkInstaller installer(&logger);
        const auto result = installer.install_or_update(config);
        std::cout << result.message << std::endl;
        return result.success ? 0 : 3;
    }

    odtkra::AntiSleepService service(config, &logger, state_path);
    if (has_arg(argc, argv, "--once") || config.dryRun) {
        return service.run_once(config.dryRun) ? 0 : 1;
    }

    return service.run_forever();
}

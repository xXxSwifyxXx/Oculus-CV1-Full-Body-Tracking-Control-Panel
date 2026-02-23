#include "core/diagnostics.h"
#include "test_utils.h"

TEST_CASE(diagnostics_report_contains_core_checks) {
    odtkra::AppConfig cfg;
    cfg.oculusDiagnosticsPath = "C:\\unlikely-odt-path";
    cfg.steamVrDriversPath = "C:\\unlikely-steamvr-drivers";

    odtkra::Diagnostics diagnostics;
    const auto report = diagnostics.run(cfg, true);

    REQUIRE(!report.items.empty());
    REQUIRE(report.items.size() >= 4);

    const auto summary = report.summary_text();
    REQUIRE(summary.find("Diagnostic verdict") != std::string::npos);
}

#pragma once

#include "core/config.h"

#include <string>
#include <vector>

namespace odtkra {

struct DiagnosticItem {
    std::string name;
    bool ok;
    std::string detail;
    std::string remediation;
};

struct DiagnosticReport {
    bool success = false;
    std::vector<DiagnosticItem> items;

    std::string summary_text() const;
};

class Diagnostics {
public:
    DiagnosticReport run(const AppConfig& config, bool dry_run) const;
};

}

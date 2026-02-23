#include "core/steam_discovery.h"

#include "core/path_utils.h"

#include <Windows.h>

#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>

namespace odtkra {

namespace {

std::string read_reg_string(HKEY root, const char* subkey, const char* value_name) {
    char buffer[1024] = {0};
    DWORD size = sizeof(buffer);
    const LONG code = RegGetValueA(root, subkey, value_name, RRF_RT_REG_SZ, nullptr, buffer, &size);
    if (code != ERROR_SUCCESS) {
        return {};
    }
    return std::string(buffer);
}

bool file_exists(const std::string& path) {
    std::ifstream in(path);
    return in.good();
}

std::vector<std::string> candidate_steam_roots() {
    std::vector<std::string> roots;

    roots.push_back(read_reg_string(HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\Valve\\Steam", "InstallPath"));
    roots.push_back(read_reg_string(HKEY_CURRENT_USER, "SOFTWARE\\Valve\\Steam", "SteamPath"));
    roots.push_back("C:\\Program Files (x86)\\Steam");

    std::vector<std::string> filtered;
    for (const auto& root : roots) {
        if (!root.empty() && path_exists(root)) {
            bool duplicate = false;
            for (const auto& seen : filtered) {
                if (_stricmp(seen.c_str(), root.c_str()) == 0) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) {
                filtered.push_back(root);
            }
        }
    }
    return filtered;
}

std::vector<std::string> parse_library_paths(const std::string& library_vdf_path) {
    std::vector<std::string> paths;
    std::ifstream in(library_vdf_path);
    if (!in.is_open()) {
        return paths;
    }

    std::string line;
    while (std::getline(in, line)) {
        const auto path_key = line.find("\"path\"");
        if (path_key == std::string::npos) {
            continue;
        }

        const auto first_quote = line.find('"', path_key + 6);
        if (first_quote == std::string::npos) {
            continue;
        }
        const auto second_quote = line.find('"', first_quote + 1);
        if (second_quote == std::string::npos) {
            continue;
        }

        std::string value = line.substr(first_quote + 1, second_quote - first_quote - 1);
        if (!value.empty()) {
            // Steam stores escaped backslashes in VDF files.
            std::string unescaped;
            for (size_t i = 0; i < value.size(); ++i) {
                if (value[i] == '\\' && i + 1 < value.size() && value[i + 1] == '\\') {
                    unescaped.push_back('\\');
                    ++i;
                } else {
                    unescaped.push_back(value[i]);
                }
            }
            if (path_exists(unescaped)) {
                paths.push_back(unescaped);
            }
        }
    }

    return paths;
}

bool find_steamvr_in_library(const std::string& library, SteamDiscoveryResult* out) {
    const std::string common = join_path(join_path(library, "steamapps"), "common");
    const std::string steamvr = join_path(common, "SteamVR");
    const std::string drivers = join_path(steamvr, "drivers");
    if (path_exists(drivers)) {
        out->steamLibrary = library;
        out->steamVrRoot = steamvr;
        out->steamVrDrivers = drivers;
        out->found = true;
        return true;
    }
    return false;
}

} // namespace

SteamDiscoveryResult detect_steamvr() {
    SteamDiscoveryResult result;

    const auto roots = candidate_steam_roots();
    for (const auto& root : roots) {
        result.steamRoot = root;

        if (find_steamvr_in_library(root, &result)) {
            return result;
        }

        const std::string library_vdf = join_path(join_path(root, "steamapps"), "libraryfolders.vdf");
        if (file_exists(library_vdf)) {
            const auto libs = parse_library_paths(library_vdf);
            for (const auto& lib : libs) {
                if (find_steamvr_in_library(lib, &result)) {
                    return result;
                }
            }
        }
    }

    result.steamVrDrivers = "C:\\Program Files (x86)\\Steam\\steamapps\\common\\SteamVR\\drivers";
    return result;
}

}

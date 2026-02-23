#pragma once

#include <string>

namespace odtkra {

std::string get_program_data_dir();
std::string join_path(const std::string& left, const std::string& right);
bool ensure_directory(const std::string& path);
bool path_exists(const std::string& path);
std::string executable_dir();

}

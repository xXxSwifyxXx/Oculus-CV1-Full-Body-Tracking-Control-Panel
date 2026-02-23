#pragma once

#include <string>
#include <vector>

namespace odtkra {

int find_process_id(const std::wstring& process_name);
int run_command(const std::string& command_line, std::string* output = nullptr);
int run_command_with_timeout(const std::string& command_line, int timeout_ms);
std::vector<std::string> tail_file(const std::string& path, int max_lines);

}

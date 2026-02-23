/*
 * File: src/core/process_utils.cpp
 *
 * Purpose:
 *   Provides process lookup and command execution helpers, including timeout-safe command execution.
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
#include "core/process_utils.h"

#include <Windows.h>
#include <TlHelp32.h>

#include <fstream>
#include <sstream>
#include <vector>

namespace odtkra {

int find_process_id(const std::wstring& process_name) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    int pid = 0;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (process_name == entry.szExeFile) {
                pid = static_cast<int>(entry.th32ProcessID);
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return pid;
}

int run_command(const std::string& command_line, std::string* output) {
    std::string final_command = "cmd.exe /C \"" + command_line + "\"";

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE read_pipe = nullptr;
    HANDLE write_pipe = nullptr;
    if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
        return -1;
    }

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.hStdOutput = write_pipe;
    si.hStdError = write_pipe;
    si.dwFlags = STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi{};
    std::vector<char> cmd(final_command.begin(), final_command.end());
    cmd.push_back('\0');

    if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(read_pipe);
        CloseHandle(write_pipe);
        return -1;
    }

    CloseHandle(write_pipe);

    std::string out;
    char buffer[512];
    DWORD read = 0;
    while (ReadFile(read_pipe, buffer, sizeof(buffer), &read, nullptr) && read > 0) {
        out.append(buffer, buffer + read);
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(read_pipe);

    if (output != nullptr) {
        *output = out;
    }

    return static_cast<int>(code);
}

int run_command_with_timeout(const std::string& command_line, int timeout_ms) {
    std::string final_command = "cmd.exe /C \"" + command_line + "\"";

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    std::vector<char> cmd(final_command.begin(), final_command.end());
    cmd.push_back('\0');

    if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        return -1;
    }

    const DWORD wait_ms = timeout_ms <= 0 ? INFINITE : static_cast<DWORD>(timeout_ms);
    const DWORD wait_result = WaitForSingleObject(pi.hProcess, wait_ms);
    if (wait_result == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 124);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return 124;
    }

    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return static_cast<int>(code);
}

std::vector<std::string> tail_file(const std::string& path, int max_lines) {
    std::ifstream in(path);
    if (!in.is_open() || max_lines <= 0) {
        return {};
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        lines.push_back(line);
        if (static_cast<int>(lines.size()) > max_lines) {
            lines.erase(lines.begin());
        }
    }
    return lines;
}

}

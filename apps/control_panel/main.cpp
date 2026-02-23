/*
 * File: apps/control_panel/main.cpp
 *
 * Purpose:
 *   Main Win32 control panel UI that displays status, paths, logs, and operational actions for runtime control and diagnostics.
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
#include "core/config.h"
#include "core/diagnostics.h"
#include "core/logger.h"
#include "core/oculus_discovery.h"
#include "core/path_utils.h"
#include "core/process_utils.h"
#include "core/steam_discovery.h"
#include "core/touchlink_installer.h"

#include <Windows.h>
#include <shellapi.h>
#include <ShlObj.h>

#include <algorithm>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <cstring>
#include <vector>

namespace {

// Centralized control IDs used by WM_COMMAND routing.
enum ControlId {
    IdStatusPrimary = 101,
    IdStatusSecondary = 102,
    IdStart = 103,
    IdStop = 104,
    IdRestart = 105,
    IdDiagnostics = 106,
    IdExportLogs = 107,
    IdInstallTouchLink = 108,
    IdChooseSteamPath = 109,
    IdGetSpaceCal = 110,
    IdChooseOculusPath = 111,
    IdCredits = 112,
    IdPaths = 113,
    IdLogs = 114,
    IdAutoScroll = 115,
    IdPathsTitle = 116,
    IdLogsTitle = 117,
    IdLogFilter = 118,
};

enum class LogFilterMode {
    LatestSession = 0,
    ErrorsOnly = 1,
    FullHistory = 2,
};

struct AppState {
    std::string data_root;
    std::string config_path;
    std::string log_path;
    std::string first_run_marker_path;
    odtkra::AppConfig config;
    odtkra::SteamDiscoveryResult steam;
    odtkra::ConfigStore* store = nullptr;
    odtkra::Logger* logger = nullptr;
};

std::unique_ptr<AppState> g_state;

// Global HWND handles are intentionally grouped by role to make layout and
// style updates explicit and easy to reason about in a single-file Win32 app.
HWND g_title = nullptr;
HWND g_status_primary = nullptr;
HWND g_status_secondary = nullptr;
HWND g_paths_title = nullptr;
HWND g_paths = nullptr;
HWND g_logs_title = nullptr;
HWND g_logs = nullptr;
HWND g_auto_scroll = nullptr;
HWND g_log_filter = nullptr;

HWND g_btn_start = nullptr;
HWND g_btn_stop = nullptr;
HWND g_btn_restart = nullptr;
HWND g_btn_diag = nullptr;
HWND g_btn_export = nullptr;
HWND g_btn_touchlink = nullptr;
HWND g_btn_spacecal = nullptr;
HWND g_btn_steam = nullptr;
HWND g_btn_odt = nullptr;
HWND g_btn_credits = nullptr;

HFONT g_font_title = nullptr;
HFONT g_font_body = nullptr;
HFONT g_font_mono = nullptr;

HBRUSH g_brush_bg = nullptr;
HBRUSH g_brush_card = nullptr;
HBRUSH g_brush_edit = nullptr;

RECT g_card_status{};
RECT g_card_actions_primary{};
RECT g_card_actions_secondary{};
RECT g_card_paths{};
RECT g_card_logs{};

std::string g_last_paths_text;
std::string g_last_logs_text;
int g_current_dpi = 96;
LogFilterMode g_log_filter_mode = LogFilterMode::LatestSession;

// DPI scaling helper. Every pixel value used for layout should pass through
// this function to keep spacing consistent across DPI settings.
int s(int px) {
    return MulDiv(px, g_current_dpi, 96);
}

COLORREF rgb(int r, int g, int b) {
    return RGB(r, g, b);
}

COLORREF color_bg() { return rgb(240, 245, 252); }
COLORREF color_card_status() { return rgb(228, 238, 252); }
COLORREF color_card_neutral() { return rgb(246, 250, 255); }
COLORREF color_card_border() { return rgb(202, 214, 232); }
COLORREF color_status_accent() { return rgb(76, 144, 224); }

void recreate_brushes() {
    // Recreate brushes whenever theme colors or DPI-dependent styles change.
    if (g_brush_bg) DeleteObject(g_brush_bg);
    if (g_brush_card) DeleteObject(g_brush_card);
    if (g_brush_edit) DeleteObject(g_brush_edit);

    g_brush_bg = CreateSolidBrush(color_bg());
    g_brush_card = CreateSolidBrush(color_card_neutral());
    g_brush_edit = CreateSolidBrush(rgb(255, 255, 255));
}

HFONT create_ui_font(int height, int weight, const char* face) {
    return CreateFontA(
        height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, face);
}

void apply_font(HWND control, HFONT font) {
    if (control && font) {
        SendMessage(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
}

void update_fonts() {
    // Fonts are recreated on startup and WM_DPICHANGED to preserve readability.
    if (g_font_title) DeleteObject(g_font_title);
    if (g_font_body) DeleteObject(g_font_body);
    if (g_font_mono) DeleteObject(g_font_mono);

    g_font_title = create_ui_font(-s(20), FW_SEMIBOLD, "Segoe UI");
    g_font_body = create_ui_font(-s(12), FW_NORMAL, "Segoe UI");
    g_font_mono = create_ui_font(-s(11), FW_NORMAL, "Consolas");
}

void apply_fonts_to_controls() {
    // Keep font assignment explicit so future control additions do not silently
    // inherit inconsistent typography.
    apply_font(g_title, g_font_title);
    apply_font(g_status_primary, g_font_body);
    apply_font(g_status_secondary, g_font_body);
    apply_font(g_paths_title, g_font_body);
    apply_font(g_paths, g_font_body);
    apply_font(g_logs_title, g_font_body);
    apply_font(g_logs, g_font_mono);
    apply_font(g_auto_scroll, g_font_body);
    apply_font(g_log_filter, g_font_body);

    apply_font(g_btn_start, g_font_body);
    apply_font(g_btn_stop, g_font_body);
    apply_font(g_btn_restart, g_font_body);
    apply_font(g_btn_diag, g_font_body);
    apply_font(g_btn_export, g_font_body);
    apply_font(g_btn_touchlink, g_font_body);
    apply_font(g_btn_spacecal, g_font_body);
    apply_font(g_btn_steam, g_font_body);
    apply_font(g_btn_odt, g_font_body);
    apply_font(g_btn_credits, g_font_body);
}

std::string data_root() {
    const std::string base = odtkra::join_path(odtkra::get_program_data_dir(), "ODTKRA");
    odtkra::ensure_directory(base);
    return base;
}

std::string local_exe_dir() {
    return odtkra::executable_dir();
}

std::string agent_exe() {
    return odtkra::join_path(local_exe_dir(), "odtkra_agent.exe");
}

std::string browse_for_folder(HWND owner, const std::string& title) {
    BROWSEINFOA bi{};
    bi.hwndOwner = owner;
    bi.lpszTitle = title.c_str();
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    PIDLIST_ABSOLUTE pid = SHBrowseForFolderA(&bi);
    if (!pid) return {};

    char path[MAX_PATH] = {0};
    std::string selected;
    if (SHGetPathFromIDListA(pid, path)) selected = path;
    CoTaskMemFree(pid);
    return selected;
}

void set_edit_text_preserve_scroll(HWND edit, const std::string& text, bool follow_if_bottom) {
    // This helper avoids the common Win32 UX issue where periodic log refreshes
    // force the viewer to jump and lose operator context.
    if (!edit) return;

    const int first_before = static_cast<int>(SendMessage(edit, EM_GETFIRSTVISIBLELINE, 0, 0));
    const int count_before = static_cast<int>(SendMessage(edit, EM_GETLINECOUNT, 0, 0));
    const bool was_bottom = first_before >= (count_before - 4 > 0 ? count_before - 4 : 0);

    SetWindowTextA(edit, text.c_str());

    if (follow_if_bottom && was_bottom) {
        SendMessage(edit, EM_LINESCROLL, 0, 1 << 20);
        return;
    }

    const int first_after = static_cast<int>(SendMessage(edit, EM_GETFIRSTVISIBLELINE, 0, 0));
    const int delta = first_before - first_after;
    if (delta != 0) {
        SendMessage(edit, EM_LINESCROLL, 0, delta);
    }
}

std::vector<std::string> read_log_lines_limited(const std::string& path, size_t max_lines) {
    std::ifstream in(path);
    if (!in.is_open()) return {};

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        lines.push_back(line);
        if (lines.size() > max_lines) {
            lines.erase(lines.begin());
        }
    }
    return lines;
}

bool contains_token(const std::string& text, const char* token) {
    return text.find(token) != std::string::npos;
}

bool start_agent() {
    // Launches the agent detached/no-window so the control panel remains the
    // only visible UI while runtime logic executes headlessly.
    if (!odtkra::path_exists(agent_exe())) {
        MessageBoxA(nullptr, "odtkra_agent.exe introuvable dans le dossier de l'application.", "ODTKRA", MB_ICONERROR);
        return false;
    }

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    const std::string cmd = "\"" + agent_exe() + "\"";
    std::vector<char> cmdline(cmd.begin(), cmd.end());
    cmdline.push_back('\0');

    if (!CreateProcessA(nullptr, cmdline.data(), nullptr, nullptr, FALSE, DETACHED_PROCESS | CREATE_NO_WINDOW, nullptr, local_exe_dir().c_str(), &si, &pi)) {
        MessageBoxA(nullptr, "Echec lancement odtkra_agent.exe", "ODTKRA", MB_ICONERROR);
        return false;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

void stop_agent() {
    odtkra::run_command("taskkill /F /IM odtkra_agent.exe >nul 2>&1");
}

void restart_agent() {
    stop_agent();
    Sleep(500);
    start_agent();
}

std::string credits_text() {
    std::ostringstream out;
    out << "Credits - Oculus CV1 Full Body Tracking Control Panel | MK1\n\n";
    out << "Core service:\n";
    out << "- ODTKRA (DeltaNeverUsed)\n";
    out << "  https://github.com/DeltaNeverUsed/ODTKRA\n\n";
    out << "Touch controller bridge:\n";
    out << "- Oculus_Touch_Steam_Link (mm0zct)\n";
    out << "  https://github.com/mm0zct/Oculus_Touch_Steam_Link\n\n";
    out << "Calibration:\n";
    out << "- Space Calibrator (Steam app 3368750)\n";
    out << "  https://store.steampowered.com/app/3368750/Space_Calibrator/\n\n";
    out << "Optional/related tools:\n";
    out << "- OVR Advanced Settings\n";
    out << "  https://store.steampowered.com/app/1009850/OVR_Advanced_Settings/\n";
    out << "- Driver4VR\n";
    out << "  https://www.driver4vr.com/\n\n";
    out << "Platforms:\n";
    out << "- Meta Horizon / Oculus runtime\n";
    out << "- SteamVR\n";
    return out.str();
}

std::string build_verbose_debug_bundle(const std::string& title, const std::string& core_message) {
    // This diagnostic payload is intentionally verbose because installation
    // issues are frequently environment-specific and difficult to reproduce.
    std::ostringstream out;
    out << title << "\n\n";
    out << "Core result: " << core_message << "\n";
    out << "SteamVR drivers: " << g_state->config.steamVrDriversPath << "\n";
    out << "Oculus diagnostics: " << g_state->config.oculusDiagnosticsPath << "\n";
    out << "Oculus client: " << g_state->config.oculusClientPath << "\n";
    out << "Detected Steam root: " << (g_state->steam.steamRoot.empty() ? "(none)" : g_state->steam.steamRoot) << "\n";
    out << "Detected Steam library: " << (g_state->steam.steamLibrary.empty() ? "(none)" : g_state->steam.steamLibrary) << "\n";
    out << "Detected SteamVR root: " << (g_state->steam.steamVrRoot.empty() ? "(none)" : g_state->steam.steamVrRoot) << "\n";

    odtkra::Diagnostics diagnostics;
    const auto report = diagnostics.run(g_state->config, true);
    out << "\nDiagnostics snapshot:\n" << report.summary_text();

    const auto lines = odtkra::tail_file(g_state->log_path, 120);
    out << "\nLast 120 log lines:\n";
    for (const auto& line : lines) out << line << "\n";

    return out.str();
}

void run_first_launch_wizard(HWND hwnd) {
    // First-launch guidance is persisted via marker file to avoid nagging users
    // after initial onboarding.
    if (odtkra::path_exists(g_state->first_run_marker_path)) return;

    const int run_now = MessageBoxA(
        hwnd,
        "ODTKRA installed.\n\nRecommended next steps:\n1) Start Meta/Oculus app\n2) Start SteamVR\n3) Click Run Diagnostics\n\nRun dry-run diagnostics now?",
        "ODTKRA First Launch",
        MB_ICONINFORMATION | MB_YESNO);

    if (run_now == IDYES) {
        odtkra::Diagnostics diagnostics;
        const auto report = diagnostics.run(g_state->config, true);
        const auto summary = report.summary_text();
        MessageBoxA(hwnd, summary.c_str(), "ODTKRA Diagnostics (dry-run)", report.success ? MB_ICONINFORMATION : MB_ICONWARNING);
    }

    std::ofstream marker(g_state->first_run_marker_path, std::ios::out | std::ios::trunc);
    marker << "done\n";
}

void layout_controls(HWND hwnd, int width, int height) {
    // One centralized layout function keeps resize behavior deterministic and
    // avoids fragmented geometry logic across message handlers.
    const int margin = s(16);
    const int gap = s(10);
    const int card_pad = s(12);

    const int content_w = width - margin * 2;
    int y = margin;

    const int title_h = s(32);
    MoveWindow(g_title, margin, y, content_w, title_h, TRUE);
    y += title_h + gap;

    const int status_h = s(90);
    g_card_status = {margin, y, margin + content_w, y + status_h};
    MoveWindow(g_status_primary, margin + card_pad, y + s(20), content_w - card_pad * 2, s(24), TRUE);
    MoveWindow(g_status_secondary, margin + card_pad, y + s(50), content_w - card_pad * 2, s(20), TRUE);
    y += status_h + gap;

    const int actions_primary_h = s(72);
    g_card_actions_primary = {margin, y, margin + content_w, y + actions_primary_h};
    {
        const int bw = (content_w - card_pad * 2 - gap * 4) / 5;
        const int by = y + s(20);
        MoveWindow(g_btn_start, margin + card_pad + (bw + gap) * 0, by, bw, s(36), TRUE);
        MoveWindow(g_btn_stop, margin + card_pad + (bw + gap) * 1, by, bw, s(36), TRUE);
        MoveWindow(g_btn_restart, margin + card_pad + (bw + gap) * 2, by, bw, s(36), TRUE);
        MoveWindow(g_btn_diag, margin + card_pad + (bw + gap) * 3, by, bw, s(36), TRUE);
        MoveWindow(g_btn_export, margin + card_pad + (bw + gap) * 4, by, bw, s(36), TRUE);
    }
    y += actions_primary_h + gap;

    const int actions_secondary_h = s(72);
    g_card_actions_secondary = {margin, y, margin + content_w, y + actions_secondary_h};
    {
        const int bw = (content_w - card_pad * 2 - gap * 4) / 5;
        const int by = y + s(20);
        MoveWindow(g_btn_touchlink, margin + card_pad + (bw + gap) * 0, by, bw, s(36), TRUE);
        MoveWindow(g_btn_spacecal, margin + card_pad + (bw + gap) * 1, by, bw, s(36), TRUE);
        MoveWindow(g_btn_steam, margin + card_pad + (bw + gap) * 2, by, bw, s(36), TRUE);
        MoveWindow(g_btn_odt, margin + card_pad + (bw + gap) * 3, by, bw, s(36), TRUE);
        MoveWindow(g_btn_credits, margin + card_pad + (bw + gap) * 4, by, bw, s(36), TRUE);
    }
    y += actions_secondary_h + gap;

    const int paths_h = s(130);
    g_card_paths = {margin, y, margin + content_w, y + paths_h};
    MoveWindow(g_paths_title, margin + card_pad, y + s(14), content_w - card_pad * 2, s(22), TRUE);
    MoveWindow(g_paths, margin + card_pad, y + s(40), content_w - card_pad * 2, s(74), TRUE);
    y += paths_h + gap;

    const int logs_h = (height - y - margin > s(180)) ? (height - y - margin) : s(180);
    g_card_logs = {margin, y, margin + content_w, y + logs_h};
    MoveWindow(g_logs_title, margin + card_pad, y + s(14), s(220), s(22), TRUE);
    MoveWindow(g_log_filter, margin + card_pad + s(230), y + s(10), s(190), s(320), TRUE);
    MoveWindow(g_auto_scroll, margin + content_w - card_pad - s(160), y + s(12), s(160), s(24), TRUE);
    MoveWindow(g_logs, margin + card_pad, y + s(40), content_w - card_pad * 2, logs_h - s(52), TRUE);

    InvalidateRect(hwnd, nullptr, TRUE);
}

void fill_round_rect(HDC dc, const RECT& r, int radius, COLORREF fill, COLORREF border) {
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HBRUSH brush = CreateSolidBrush(fill);

    auto old_pen = SelectObject(dc, pen);
    auto old_brush = SelectObject(dc, brush);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, radius, radius);

    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void paint_window(HWND hwnd, HDC hdc) {
    // Parent paint draws card backgrounds and accents only. Child controls draw
    // their own content to avoid overdraw artifacts.
    RECT rc{};
    GetClientRect(hwnd, &rc);
    FillRect(hdc, &rc, g_brush_bg);

    auto draw_card = [&](const RECT& r, COLORREF fill, COLORREF border) {
        HBRUSH fill_brush = CreateSolidBrush(fill);
        HBRUSH border_brush = CreateSolidBrush(border);
        FillRect(hdc, &r, fill_brush);
        FrameRect(hdc, &r, border_brush);
        DeleteObject(fill_brush);
        DeleteObject(border_brush);
    };

    draw_card(g_card_status, color_card_status(), color_card_border());
    draw_card(g_card_actions_primary, color_card_neutral(), color_card_border());
    draw_card(g_card_actions_secondary, color_card_neutral(), color_card_border());
    draw_card(g_card_paths, color_card_neutral(), color_card_border());
    draw_card(g_card_logs, color_card_neutral(), color_card_border());

    RECT accent = g_card_status;
    accent.right = accent.left + s(6);
    HBRUSH accent_brush = CreateSolidBrush(color_status_accent());
    FillRect(hdc, &accent, accent_brush);
    DeleteObject(accent_brush);
}

bool is_primary_green_button(int id) {
    return id == IdStart;
}

void draw_button(const DRAWITEMSTRUCT* dis) {
    const RECT r = dis->rcItem;
    const int id = static_cast<int>(dis->CtlID);
    const bool pressed = (dis->itemState & ODS_SELECTED) != 0;
    const bool disabled = (dis->itemState & ODS_DISABLED) != 0;

    COLORREF fill = rgb(233, 239, 248);
    COLORREF border = rgb(186, 198, 214);
    COLORREF text = rgb(34, 44, 58);

    if (is_primary_green_button(id)) {
        fill = rgb(62, 172, 95);
        border = rgb(41, 139, 72);
        text = rgb(255, 255, 255);
    }

    if (pressed) {
        fill = RGB(
            (GetRValue(fill) > 14 ? GetRValue(fill) - 14 : 0),
            (GetGValue(fill) > 14 ? GetGValue(fill) - 14 : 0),
            (GetBValue(fill) > 14 ? GetBValue(fill) - 14 : 0));
    }
    if (disabled) {
        fill = rgb(220, 224, 230);
        border = rgb(200, 206, 215);
        text = rgb(130, 140, 152);
    }

    fill_round_rect(dis->hDC, r, s(8), fill, border);

    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, text);

    char caption[256] = {0};
    GetWindowTextA(dis->hwndItem, caption, sizeof(caption));
    DrawTextA(dis->hDC, caption, -1, const_cast<RECT*>(&r), DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if ((dis->itemState & ODS_FOCUS) != 0) {
        RECT fr = r;
        InflateRect(&fr, -3, -3);
        DrawFocusRect(dis->hDC, &fr);
    }
}

void refresh_status_ui() {
    // Refresh cycle pulls from process state + state.json + log file, then
    // updates only changed text blocks to reduce flicker and scroll jumps.
    g_state->steam = odtkra::detect_steamvr();

    const int pid = odtkra::find_process_id(L"odtkra_agent.exe");

    bool state_running = false;
    bool state_anti = false;
    const std::string state_path = odtkra::join_path(g_state->data_root, "state.json");
    if (odtkra::path_exists(state_path)) {
        std::ifstream state(state_path);
        std::stringstream buffer;
        buffer << state.rdbuf();
        const auto txt = buffer.str();
        state_running = txt.find("\"running\": true") != std::string::npos;
        state_anti = txt.find("\"antiSleepActive\": true") != std::string::npos;
    }

    const std::string primary = (pid != 0)
        ? "Status: OKDRTKA Active"
        : "Status: OKDRTKA Stopped";

    std::string secondary = "Anti-sleep stopped";
    if (pid != 0 && state_running && state_anti) {
        secondary = "Anti-sleep active";
    } else if (pid != 0) {
        secondary = "Anti-sleep stopped";
    }

    SetWindowTextA(g_status_primary, primary.c_str());
    SetWindowTextA(g_status_secondary, secondary.c_str());

    std::ostringstream paths;
    paths << "Steam root detected: " << (g_state->steam.steamRoot.empty() ? "(none)" : g_state->steam.steamRoot) << "\r\n";
    paths << "Steam library detected: " << (g_state->steam.steamLibrary.empty() ? "(none)" : g_state->steam.steamLibrary) << "\r\n";
    paths << "SteamVR drivers: " << g_state->config.steamVrDriversPath << "\r\n";
    paths << "Oculus diagnostics: " << g_state->config.oculusDiagnosticsPath << "\r\n";
    paths << "Oculus client: " << g_state->config.oculusClientPath;

    const std::string paths_text = paths.str();
    if (paths_text != g_last_paths_text) {
        set_edit_text_preserve_scroll(g_paths, paths_text, false);
        g_last_paths_text = paths_text;
    }

    const auto lines = read_log_lines_limited(g_state->log_path, 4000);
    size_t latest_session_index = 0;
    for (size_t i = lines.size(); i > 0; --i) {
        if (contains_token(lines[i - 1], "ODTKRA agent booting")) {
            latest_session_index = i - 1;
            break;
        }
    }

    std::vector<std::string> filtered;
    if (g_log_filter_mode == LogFilterMode::FullHistory) {
        filtered = lines;
    } else {
        filtered.assign(lines.begin() + static_cast<std::ptrdiff_t>(latest_session_index), lines.end());
        if (g_log_filter_mode == LogFilterMode::ErrorsOnly) {
            std::vector<std::string> only_errors;
            for (const auto& l : filtered) {
                if (contains_token(l, "[ERROR]")) {
                    only_errors.push_back(l);
                }
            }
            filtered.swap(only_errors);
        }
    }

    std::ostringstream logs;
    for (const auto& line : filtered) logs << line << "\r\n";

    const std::string logs_text = logs.str();
    if (logs_text != g_last_logs_text) {
        const bool follow = (SendMessage(g_auto_scroll, BM_GETCHECK, 0, 0) == BST_CHECKED);
        set_edit_text_preserve_scroll(g_logs, logs_text, follow);
        g_last_logs_text = logs_text;
    }
}

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) {
    switch (msg) {
        case WM_CREATE: {
            // Explicit style enforcement ensures the window remains resizable
            // even if defaults differ across toolchains or manifests.
            const LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
            SetWindowLongPtr(hwnd, GWL_STYLE, style | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_SIZEBOX);
            SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);

            auto get_dpi_for_window = reinterpret_cast<UINT(WINAPI*)(HWND)>(
                GetProcAddress(GetModuleHandleA("user32.dll"), "GetDpiForWindow"));
            g_current_dpi = get_dpi_for_window ? static_cast<int>(get_dpi_for_window(hwnd)) : 96;

            recreate_brushes();
            update_fonts();

            g_title = CreateWindowA("STATIC", "Oculus CV1 Full Body Tracking Control Panel | MK1", WS_VISIBLE | WS_CHILD,
                0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);
            g_status_primary = CreateWindowA("STATIC", "Status: OKDRTKA Stopped", WS_VISIBLE | WS_CHILD,
                0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IdStatusPrimary), nullptr, nullptr);
            g_status_secondary = CreateWindowA("STATIC", "Anti-sleep stopped", WS_VISIBLE | WS_CHILD,
                0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IdStatusSecondary), nullptr, nullptr);

            g_btn_start = CreateWindowA("BUTTON", "Start", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IdStart), nullptr, nullptr);
            g_btn_stop = CreateWindowA("BUTTON", "Stop", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IdStop), nullptr, nullptr);
            g_btn_restart = CreateWindowA("BUTTON", "Restart", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IdRestart), nullptr, nullptr);
            g_btn_diag = CreateWindowA("BUTTON", "Run Diagnostics", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IdDiagnostics), nullptr, nullptr);
            g_btn_export = CreateWindowA("BUTTON", "Export Logs", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IdExportLogs), nullptr, nullptr);

            g_btn_touchlink = CreateWindowA("BUTTON", "Install/Update OculusTouchLink", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IdInstallTouchLink), nullptr, nullptr);
            g_btn_spacecal = CreateWindowA("BUTTON", "Download Space Calibrator", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IdGetSpaceCal), nullptr, nullptr);
            g_btn_steam = CreateWindowA("BUTTON", "Choose SteamVR Folder", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IdChooseSteamPath), nullptr, nullptr);
            g_btn_odt = CreateWindowA("BUTTON", "Choose ODT Path", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IdChooseOculusPath), nullptr, nullptr);
            g_btn_credits = CreateWindowA("BUTTON", "Credits", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IdCredits), nullptr, nullptr);

            g_paths_title = CreateWindowA("STATIC", "System Paths", WS_VISIBLE | WS_CHILD,
                0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IdPathsTitle), nullptr, nullptr);
            g_paths = CreateWindowA("EDIT", "", WS_VISIBLE | WS_CHILD | ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_HSCROLL | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_BORDER,
                0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IdPaths), nullptr, nullptr);

            g_logs_title = CreateWindowA("STATIC", "Recent Logs", WS_VISIBLE | WS_CHILD,
                0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IdLogsTitle), nullptr, nullptr);

            g_log_filter = CreateWindowA("COMBOBOX", "", WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
                0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IdLogFilter), nullptr, nullptr);
            SendMessageA(g_log_filter, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Latest session"));
            SendMessageA(g_log_filter, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Errors only"));
            SendMessageA(g_log_filter, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Full history"));
            SendMessage(g_log_filter, CB_SETCURSEL, 0, 0);
            g_log_filter_mode = LogFilterMode::LatestSession;

            g_auto_scroll = CreateWindowA("BUTTON", "Auto-scroll", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
                0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IdAutoScroll), nullptr, nullptr);
            SendMessage(g_auto_scroll, BM_SETCHECK, BST_CHECKED, 0);

            g_logs = CreateWindowA("EDIT", "", WS_VISIBLE | WS_CHILD | ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_HSCROLL | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_BORDER,
                0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IdLogs), nullptr, nullptr);

            apply_fonts_to_controls();

            RECT rc{};
            GetClientRect(hwnd, &rc);
            layout_controls(hwnd, rc.right - rc.left, rc.bottom - rc.top);

            SetTimer(hwnd, 1, 1500, nullptr);
            refresh_status_ui();
            return 0;
        }
        case WM_TIMER:
            // Periodic refresh keeps status and logs live without requiring user
            // interaction.
            refresh_status_ui();
            return 0;
        case WM_GETMINMAXINFO: {
            auto* mmi = reinterpret_cast<MINMAXINFO*>(l_param);
            mmi->ptMinTrackSize.x = 980;
            mmi->ptMinTrackSize.y = 700;
            return 0;
        }
        case WM_DPICHANGED: {
            g_current_dpi = HIWORD(w_param);
            update_fonts();
            recreate_brushes();
            apply_fonts_to_controls();

            auto* suggested = reinterpret_cast<RECT*>(l_param);
            SetWindowPos(hwnd, nullptr,
                suggested->left, suggested->top,
                suggested->right - suggested->left,
                suggested->bottom - suggested->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        case WM_SIZE: {
            if (w_param == SIZE_MINIMIZED) return 0;
            layout_controls(hwnd, LOWORD(l_param), HIWORD(l_param));
            return 0;
        }
        case WM_DRAWITEM: {
            const auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(l_param);
            if (dis && dis->CtlType == ODT_BUTTON) {
                draw_button(dis);
                return TRUE;
            }
            break;
        }
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(w_param);
            SetBkMode(dc, OPAQUE);
            const HWND target = reinterpret_cast<HWND>(l_param);
            if (target == g_status_primary || target == g_status_secondary) {
                SetBkColor(dc, color_card_status());
            } else if (target == g_paths_title || target == g_logs_title) {
                SetBkColor(dc, color_card_neutral());
            } else {
                SetBkColor(dc, color_bg());
            }
            if (target == g_status_secondary) {
                SetTextColor(dc, rgb(106, 120, 140));
            } else {
                SetTextColor(dc, rgb(36, 50, 68));
            }
            if (target == g_status_primary || target == g_status_secondary || target == g_paths_title || target == g_logs_title) {
                return reinterpret_cast<LRESULT>(g_brush_card);
            }
            return reinterpret_cast<LRESULT>(g_brush_bg);
        }
        case WM_CTLCOLOREDIT: {
            HDC dc = reinterpret_cast<HDC>(w_param);
            SetBkColor(dc, rgb(255, 255, 255));
            SetTextColor(dc, rgb(32, 40, 52));
            return reinterpret_cast<LRESULT>(g_brush_edit);
        }
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hwnd, &ps);
            paint_window(hwnd, hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_COMMAND: {
            // WM_COMMAND is intentionally kept as a flat dispatch table so each
            // operation is easy to follow and audit.
            const int id = LOWORD(w_param);
            const int code = HIWORD(w_param);
            if (id == IdLogFilter && code == CBN_SELCHANGE) {
                const int sel = static_cast<int>(SendMessage(g_log_filter, CB_GETCURSEL, 0, 0));
                if (sel == 1) g_log_filter_mode = LogFilterMode::ErrorsOnly;
                else if (sel == 2) g_log_filter_mode = LogFilterMode::FullHistory;
                else g_log_filter_mode = LogFilterMode::LatestSession;
                g_last_logs_text.clear();
                refresh_status_ui();
                return 0;
            }
            if (id == IdStart) {
                start_agent();
                refresh_status_ui();
            } else if (id == IdStop) {
                stop_agent();
                refresh_status_ui();
            } else if (id == IdRestart) {
                restart_agent();
                refresh_status_ui();
            } else if (id == IdDiagnostics) {
                odtkra::Diagnostics diagnostics;
                const auto report = diagnostics.run(g_state->config, true);
                const auto summary = report.summary_text();
                MessageBoxA(hwnd, summary.c_str(), "ODTKRA Diagnostics (dry-run)", report.success ? MB_ICONINFORMATION : MB_ICONWARNING);
                g_state->logger->write(report.success ? odtkra::LogLevel::Info : odtkra::LogLevel::Warning, "Control panel diagnostics executed");
            } else if (id == IdExportLogs) {
                const std::string export_path = odtkra::join_path(local_exe_dir(), "odtkra-export.log");
                CopyFileA(g_state->log_path.c_str(), export_path.c_str(), FALSE);
                MessageBoxA(hwnd, ("Logs exported to: " + export_path).c_str(), "ODTKRA", MB_ICONINFORMATION);
            } else if (id == IdChooseSteamPath) {
                const std::string selected = browse_for_folder(hwnd, "Select SteamVR drivers directory");
                if (!selected.empty()) {
                    g_state->config.steamVrDriversPath = selected;
                    g_state->store->save(g_state->config);
                    refresh_status_ui();
                }
            } else if (id == IdChooseOculusPath) {
                const std::string selected = browse_for_folder(hwnd, "Select Oculus diagnostics directory");
                if (!selected.empty()) {
                    g_state->config.oculusDiagnosticsPath = selected;
                    g_state->store->save(g_state->config);
                    refresh_status_ui();
                }
            } else if (id == IdInstallTouchLink) {
                odtkra::TouchLinkInstaller installer(g_state->logger);
                const auto result = installer.install_or_update(g_state->config);
                const std::string verbose = result.success
                    ? build_verbose_debug_bundle("OculusTouchLink install succeeded", result.message + " | installed_path=" + result.installed_path)
                    : build_verbose_debug_bundle("OculusTouchLink install failed", result.message);
                MessageBoxA(hwnd, verbose.c_str(), result.success ? "Install OculusTouchLink" : "Install OculusTouchLink Error", result.success ? MB_ICONINFORMATION : MB_ICONERROR);
            } else if (id == IdGetSpaceCal) {
                odtkra::run_command("powershell -NoProfile -ExecutionPolicy Bypass -Command \"Start-Process 'steam://store/3368750'\"");
            } else if (id == IdCredits) {
                const auto text = credits_text();
                MessageBoxA(hwnd, text.c_str(), "Credits", MB_ICONINFORMATION);
            }
            return 0;
        }
        case WM_DESTROY:
            KillTimer(hwnd, 1);
            if (g_font_title) DeleteObject(g_font_title);
            if (g_font_body) DeleteObject(g_font_body);
            if (g_font_mono) DeleteObject(g_font_mono);
            if (g_brush_bg) DeleteObject(g_brush_bg);
            if (g_brush_card) DeleteObject(g_brush_card);
            if (g_brush_edit) DeleteObject(g_brush_edit);
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProc(hwnd, msg, w_param, l_param);
    }
    return DefWindowProc(hwnd, msg, w_param, l_param);
}

} // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show) {
    // Process-level DPI awareness must be set before creating controls.
    auto set_dpi_ctx = reinterpret_cast<BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT)>(
        GetProcAddress(GetModuleHandleA("user32.dll"), "SetProcessDpiAwarenessContext"));
    if (set_dpi_ctx) {
        set_dpi_ctx(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    } else {
        SetProcessDPIAware();
    }

    auto state = std::make_unique<AppState>();
    state->data_root = data_root();
    state->config_path = odtkra::join_path(state->data_root, "config.json");
    state->log_path = odtkra::join_path(state->data_root, "odtkra.log");
    state->first_run_marker_path = odtkra::join_path(state->data_root, "first_launch_done.txt");

    odtkra::ConfigStore store(state->config_path);
    state->config = store.load();
    bool config_changed = false;

    if (!odtkra::path_exists(odtkra::join_path(state->config.oculusDiagnosticsPath, "OculusDebugToolCLI.exe")) || !odtkra::path_exists(state->config.oculusClientPath)) {
        const auto oculus = odtkra::detect_oculus_paths();
        if (!oculus.diagnosticsDir.empty()) {
            state->config.oculusDiagnosticsPath = oculus.diagnosticsDir;
            config_changed = true;
        }
        if (!oculus.oculusClientExe.empty()) {
            state->config.oculusClientPath = oculus.oculusClientExe;
            config_changed = true;
        }
    }

    state->steam = odtkra::detect_steamvr();
    if (!odtkra::path_exists(state->config.steamVrDriversPath)) {
        state->config.steamVrDriversPath = state->steam.steamVrDrivers;
        config_changed = true;
    }

    if (config_changed) {
        store.save(state->config);
    }

    state->store = &store;

    odtkra::Logger logger(state->log_path);
    state->logger = &logger;

    g_state = std::move(state);

    WNDCLASSA wc{};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = instance;
    wc.lpszClassName = "ODTKRAControlPanelWnd";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(
        0,
        wc.lpszClassName,
        "Oculus CV1 Full Body Tracking Control Panel | MK1",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        CW_USEDEFAULT, CW_USEDEFAULT, 1180, 820,
        nullptr, nullptr, instance, nullptr);

    if (!hwnd) {
        return 1;
    }

    ShowWindow(hwnd, show);
    run_first_launch_wizard(hwnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

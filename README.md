# ODTKRA (Industrial Refactor)

ODTKRA keeps Oculus Rift CV1 awake with a production-oriented architecture:
- decoupled core logic
- diagnostics dry-run
- Windows Control Panel UI
- SteamVR driver installer for `OculusTouchLink`
- CI + installer automation

## 1) Proposed Clean Architecture

Modules:
- `src/core`
  - `config.*`: JSON config persistence + CLI overrides
  - `logger.*`: file logging (`%ProgramData%\ODTKRA\odtkra.log`)
  - `process_utils.*`: process detection, shell execution, log tail
  - `path_utils.*`: Windows paths + filesystem helpers
  - `diagnostics.*`: technical checklist + verdict (dry-run aware)
  - `anti_sleep_service.*`: anti-sleep loop + state file (`state.json`)
  - `touchlink_installer.*`: download + copy `OculusTouchLink` to SteamVR
- `src/agent/main.cpp`
  - headless runtime (`odtkra_agent.exe`)
  - supports `--dry-run`, `--diagnostics`, `--install-touchlink`, `--once`
- `src/control_panel/main.cpp`
  - Win32 Control Panel:
    - ODTKRA running/stopped status
    - Start/Stop/Restart
    - anti-sleep status
    - recent logs
    - Run diagnostics (dry-run)
    - Export logs
    - Install/Update OculusTouchLink
    - `Download Space Calibrator` button (configurable URL)
    - verbose debug popup for install results (diagnostics snapshot + recent logs)
    - Steam auto-detection details (Steam root, Steam library, SteamVR drivers)
- `tests`
  - unit tests for config/diagnostics and CLI argument behavior
- `packaging/odtkra.iss`
  - setup exe installer (Program Files, desktop/start menu shortcuts, uninstall)
  - post-install OculusTouchLink install
  - runtime verification moved to first launch inside Control Panel

Runtime flow:
1. Agent loads `config.json` from `%ProgramData%\ODTKRA`.
2. Agent applies anti-sleep command profile via `OculusDebugToolCLI.exe`.
3. Agent writes `state.json` + logs continuously.
4. Control Panel reads state/logs and orchestrates operator actions.

## 2) Migration Plan (incremental)

1. Freeze legacy entrypoint in `ODTKRA/Main.cpp` (reference only).
2. Introduce modular `src/core` services.
3. Move runtime into `odtkra_agent.exe` with same baseline behavior.
4. Add dry-run diagnostics and state reporting.
5. Add Control Panel operations and log surface.
6. Add OculusTouchLink install/update workflow.
7. Add tests and CI.
8. Add installer and release automation.

Key risks + mitigations:
- Oculus/SteamVR runtime absent: diagnostics fail gracefully, no crash.
- Program Files permissions for driver copy: explicit error logging/remediation.
- GitHub download failures: installer returns clear failure reason.
- Driver layout changes upstream: post-copy file verification catches mismatch.

## 3) Stack Choice

- Language: C++17 (native Windows, no heavy runtime dependency)
- UI: Win32 API (small footprint, redistributable simplicity)
- Build: CMake + VS 2022 generator in CI
- Tests: lightweight built-in harness (no external test framework dependency)
- Installer: Inno Setup (`packaging/odtkra.iss`)
- CI/CD: GitHub Actions (`.github/workflows/ci.yml`, `release.yml`)

Why this stack:
- keeps deploy light for CV1 users
- avoids runtime install burden
- works with native Windows APIs needed for diagnostics/process control

## 4) Implemented Features

- Refactored modular codebase under `src/`.
- Dry-run diagnostics with checklist + corrective actions.
- New Control Panel app named:
  - `Oculus CV1 Full Body Tracking Control Panel`
- TouchLink install/update pipeline:
  - source: `https://github.com/mm0zct/Oculus_Touch_Steam_Link`
  - expected repo path: `ReleasePackage/OculusTouchLink`
  - default destination:
    - `C:\Program Files (x86)\Steam\steamapps\common\SteamVR\drivers\OculusTouchLink`
  - auto-detect Steam/SteamVR path (registry + library folders), fallback to default path
  - user can override path in installer and in control panel
  - backup + overwrite-safe + post-copy verification
- Oculus diagnostics default path set to:
  - `C:\Program Files\Meta Horizon\Support\oculus-diagnostics`
  - plus auto-discovery scan under `C:\Program Files\Meta Horizon\`, `C:\Program Files\Meta\`, `C:\Program Files\Oculus\`
- Space Calibrator Steam button implemented through configurable URL (`spaceCalibratorUrl`).
  - launched via `Start-Process "steam://store/3368750"`

## 5) Build, Test, Package

Prereqs (build machine):
- Visual Studio 2022 Build Tools (C++)
- CMake 3.22+
- Inno Setup 6 (for installer)

Build:
```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Test:
```powershell
ctest --test-dir build -C Release --output-on-failure
```

Installer:
```powershell
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" "packaging\odtkra.iss"
```
Output:
- `packaging\output\ODTKRA-Setup.exe`

CI:
- `ci.yml`: build + test + artifact upload
- `release.yml`: tag-based release + installer artifact publish

## CLI Quick Reference

```powershell
odtkra_agent.exe --once
odtkra_agent.exe --dry-run --once
odtkra_agent.exe --diagnostics
odtkra_agent.exe --install-touchlink
odtkra_agent.exe --path "C:\Program Files\Meta Horizon\Support\oculus-diagnostics"
odtkra_agent.exe --oculus-client "C:\Program Files\Meta\Support\oculus-client\OculusClient.exe"
odtkra_agent.exe --steamvr-drivers "C:\Program Files (x86)\Steam\steamapps\common\SteamVR\drivers"
```

## Legacy Note

Previous monolithic implementation remains in:
- `ODTKRA/Main.cpp`

New production code is under `src/`.

## Credits

- ODTKRA (DeltaNeverUsed): https://github.com/DeltaNeverUsed/ODTKRA
- Oculus_Touch_Steam_Link (mm0zct): https://github.com/mm0zct/Oculus_Touch_Steam_Link
- Space Calibrator (Steam): https://store.steampowered.com/app/3368750/Space_Calibrator/
- OVR Advanced Settings (Steam): https://store.steampowered.com/app/1009850/OVR_Advanced_Settings/
- Driver4VR: https://www.driver4vr.com/

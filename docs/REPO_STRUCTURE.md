# Repository Structure

## Active project code
- `src/core`: shared runtime services (config, diagnostics, install, discovery, logging)
- `src/agent`: headless executable entrypoint
- `src/control_panel`: Win32 control panel entrypoint
- `tests`: unit/smoke test binaries
- `packaging`: installer scripts and output folder
- `.github/workflows`: CI/release pipelines

## Documentation and assets
- `README.md`: main project documentation
- `docs/PROJECT_TRANSFER_HISTORY.md`: migration and transfer context
- `docs/images`: documentation-only images

## Archived legacy code
- `legacy/ODTKRA`: pre-refactor monolithic implementation (reference only)
- `legacy/ODTKRA.sln`: legacy Visual Studio solution

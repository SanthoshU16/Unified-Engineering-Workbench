# Build Tools and Dependency Record

**Updated:** 2026-11-15
**Lock record:** [`../dependencies.lock`](../dependencies.lock)

## Runtime dependencies

The application has no third-party runtime libraries and the frontend has no npm packages. No package lockfile is needed. The backend uses the C++ standard library and operating-system socket APIs.

| Component | Requirement / version | Purpose |
|---|---|---|
| C++ | C++17 | Backend language standard. |
| CMake | 3.20 or newer | Configure and build source. |
| Windows compiler | Visual Studio 2022 C++ x64 toolchain | Build the Windows runtime. The CMake configuration selects the static MSVC runtime. |
| Windows system libraries | `ws2_32`, `shell32` | Winsock server and opening the default browser. Included with Windows SDK. |
| macOS/Linux threads | Platform C++ thread library | POSIX build links `Threads::Threads`. |
| Node.js | Not required to run | Used only for optional frontend syntax checks. |
| ReportLab 4.5.1 | Package-time document-generation tool | Used by `scripts/render_report.py` to produce the six-page report PDF. |
| Poppler `pdfinfo` | Package-time PDF inspection tool | Used by the package preflight to enforce the six-page report limit. |
| Python standard library | Package-time video inspection | `scripts/probe_movie.py` checks MP4/MOV video-track presence and duration without an extra package. |
| Git fast-export/import | Included with Git CLI | Creates the sanitized main-branch history bundle for the submission archive. |

## Verified build environment

- macOS on Apple silicon
- Apple Clang 21.0 (`clang-2100.3.34.2`)
- CMake 4.4.5
- Node.js 22.19.0
- ReportLab 4.5.1
- Windows x64/MSVC: not built or run in this environment

## Reproducibility notes

The Windows build helper is `scripts/build_windows_runtime.ps1`. It creates root-level `Launch.exe` and `dist/windows-x64/workbench_server.exe`, then prints SHA-256 hashes. It still needs to be run and smoke-checked on Windows 11 x64 before the archive can be treated as final.

# Assignment Requirement Audit

**Assessment source:** `Assignment_Only_Phase_version.pdf` (three pages, 100-point rubric)
**Audit date:** 2026-11-15
**Current state:** **NOT READY TO SUBMIT**. Source features and local validation are in place, but the Windows launch gate and required demo evidence are still open.

## Requirement-by-requirement status

| Assignment item | Status | Evidence and remaining work |
|---|---|---|
| Task A: four-view interface, C++ backend, validation, status, export | Implemented in source | The Geometry, Thermal, Performance, and Hardware views and C++ REST backend are present. Desktop mode binds to loopback; cloud/container mode explicitly binds all interfaces. Local build, tests, and sample-import smoke checks are recorded in [`WALKTHROUGH.md`](WALKTHROUGH.md). Safe completion is used; no cancellation control is claimed. |
| Task A: standalone Windows x64 launch | **Open gate** | The Windows launcher source and build helper are present. The required root-level `Launch.exe` and `dist/windows-x64/workbench_server.exe` are absent and have not been built or tested on Windows. The assignment's successful standalone launch gate is therefore not met. |
| Task A: editor run configuration | Prepared, not verified | VS Code has a packaged-runtime configuration and a separate source-build configuration. Microsoft C/C++ and CodeLLDB prerequisites are documented. The packaged configuration requires the missing runtime. |
| Task B: ASCII STL and ANSYS-compatible CSV | Implemented | `data/geometry/` and `data/thermal/` contain examples, malformed fixtures, and provenance notes. The CSV is identified as synthetic. Parsers validate malformed and non-finite inputs and enforce documented limits. |
| Task C: 1D thermal FEA | Implemented | The solver uses the requested material/geometry values, two-node elements, prescribed end temperatures, midpoint error, N=10/20/40/80 convergence, CSV export, units, assumptions, and a debugging insight. Raw data and labelled plots are in `results/`. |
| Task D: multicore benchmark | Implemented with recorded results | The thread pool, fixed-seed batch, deterministic output, warm-up, five repetitions, median, speedup, efficiency, hardware/compiler metadata, and agreement check are implemented. The checked-in CSV matches the report. The short 2-worker measurement exceeds 100% efficiency slightly; the report describes this as timing variation, not sustained scaling. |
| Task E: embedded design and mocks | Implemented as simulation | The design spec has a block diagram, interface/pin-voltage assumptions, JTAG role explanation, safe integration sequence, three C++ mock adapters, and recoverable timeout behavior. No physical devices are claimed. |
| Task F: handwritten and C++ backpropagation | Implemented | Two photographed handwritten pages are included. C++ checks all initial gradients with central finite differences and exports the 100-step loss series and plot. |
| ZIP folder structure and source evidence | Prepared | The repository now uses the requested `src/`, `data/`, `tests/`, `results/`, and `docs/` folders. The packager includes the source, runtime, editor files, data, tests, results, docs, scripts, and Git bundle, and blocks if the runtime or candidate ID is missing. No final archive was produced because the runtime and candidate ID are still missing. |
| README and dependency/version information | Implemented | README covers setup, launch, architecture, supported formats, tests, limitations, and editor prerequisites. `docs/DEPENDENCIES.md` records tools and the absence of third-party runtime packages. |
| Report page limit | Complete for current content | `docs/REPORT.pdf` is a visually checked six-page A4 report. Its structured source is `scripts/render_report.py`; `docs/REPORT.md` carries the detailed report and candidate metadata. Keep facts aligned and rebuild the PDF after edits. |
| Demo video or timestamped screenshot walkthrough | **Open evidence item** | Eight still screenshots and a written walkthrough are present, but the images are not a timestamped sequence and there is no 3–5 minute video. Capture one of the accepted forms; do not label the current stills as timestamped. |
| Actual effort and archive identity | **Candidate input needed** | The brief asks for actual hours. Candidate ID and hours have not yet been supplied, so the package name and effort record cannot be completed truthfully. |
| Version-control evidence | Prepared | The repository has development history and a readable log. The packager creates a sanitized bundle from committed `main`, anonymizes author/committer email fields, and excludes the older bundle that contains personal contact details. Commit final changes first so the bundle includes them. |

## Final submission sequence

1. On Windows 11 x64 with Visual Studio 2022 C++ workload and CMake 3.20+, run `scripts/build_windows_runtime.ps1`.
2. Double-click the root-level `Launch.exe` on a clean extracted copy and confirm that the UI opens without a terminal, build tools, admin rights, or internet. Check `dist/windows-x64/workbench_server.exe` is present.
3. Record a 3–5 minute demo at `docs/demo/demo.mp4` or create a timestamped screenshot walkthrough with logs at `docs/demo/screenshot_walkthrough.md`.
4. Add the candidate ID and actual effort hours, review the AI-use and ownership statements, update both report sources if facts change, commit the final docs, and run `scripts/package_submission.sh <CandidateID>`. The packager regenerates the PDF and checks that it is six pages or fewer.
5. Inspect the ZIP contents and test the packaged launch one last time.

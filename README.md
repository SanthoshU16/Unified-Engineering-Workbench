# Unified Engineering Workbench (UEW)

[![C++17](https://img.shields.io/badge/Language-C%2B%2B17-success.svg)](https://en.cppreference.com/w/cpp/17)
[![CMake](https://img.shields.io/badge/Build-CMake%203.20%2B-brightgreen.svg)](https://cmake.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows%2011%20x64%20%7C%20macOS%20%7C%20Linux-lightgrey.svg)]()
[![Tests](https://img.shields.io/badge/CTest-14%2F14%20Passed%20(100%25)-success.svg)]()
[![Live Demo](https://img.shields.io/badge/Live%20Demo-Render-46E3B7?logo=render&logoColor=white)](https://unified-engineering-workbench.onrender.com)

> **PASS Science Works | R&D Internship Assessment 2026–2027**  
> Comprehensive, reliable engineering workbench combining CAD geometry processing, numerical thermal finite-element analysis (FEA), multicore parallel execution, embedded hardware workbench integration, and analytical backpropagation.

---

## 🚀 Live Application

The complete workbench is deployed and accessible live in the cloud:  
👉 **[https://unified-engineering-workbench.onrender.com](https://unified-engineering-workbench.onrender.com)**

---

## 1. Run the Workbench Locally

The application has no third-party runtime libraries. A source build requires **CMake 3.20+** and a **C++17 compiler**. Desktop runs listen on loopback; cloud/container runs bind to all interfaces when deployment environment variables are set. Tool versions and dependency details are recorded in [`dependencies.lock`](dependencies.lock) and [`docs/DEPENDENCIES.md`](docs/DEPENDENCIES.md).

### Clone Repository:
```bash
git clone https://github.com/SanthoshU16/Unified-Engineering-Workbench.git
cd Unified-Engineering-Workbench
```

### Windows 11 x64
The submitted ZIP must contain root-level `Launch.exe` and `dist/windows-x64/workbench_server.exe`; double-click `Launch.exe` immediately after extraction. `Launch.bat` is a development fallback that can build the source tree when CMake and a compiler are installed.

> **Packaging status:** The Windows x64 executables are not present in this checkout. The zero-install delivery requirement remains open until both files are built and checked on Windows 11 x64.

To build the Windows runtime on a Windows x64 development machine, install CMake 3.20+ and Visual Studio 2022 with the **Desktop development with C++** workload, then run:

```powershell
.\scripts\build_windows_runtime.ps1
```

This build-time toolchain is not required by the extracted application. After the runtime is checked, create the archive with `./scripts/package_submission.sh "<CandidateID>"` from a shell with `zip`, Git, Python 3 / ReportLab 4.5.1, and Poppler `pdfinfo` available.

Before packaging, provide the required 3–5 minute recording as `docs/demo/demo.mp4` or `docs/demo/demo.mov`, or provide a timestamped screenshot walkthrough with logs at `docs/demo/screenshot_walkthrough.md`. The package script checks the video track and duration with the standard-library `scripts/probe_movie.py` helper, Candidate ID, Windows runtime, and six-page report. The supplied MOV is 72.875 seconds, so this checkout uses the timestamped screenshot walkthrough and verification log as the alternate evidence.

### macOS / Linux
Run the shell launcher:
```bash
./launch.sh
```
When CMake is available, the script reconfigures and incrementally rebuilds the backend before launch; otherwise it can use an existing prebuilt backend.

### Editor Run Configuration (VS Code / Antigravity IDE)
1. Open the project folder.
2. For the packaged Windows app, select **Launch Packaged Workbench (Windows x64)** and press **F5**. The packaged runtime must already exist at `dist/windows-x64/`.
3. For source debugging, select **Build and Launch Source (Windows x64)** or **Launch Unified Workbench (macOS / Linux)**; the task configures and builds CMake first.
4. Windows debugging requires the Microsoft C/C++ extension. macOS/Linux debugging requires CodeLLDB. These editor extensions are not needed by the standalone launcher.

---

## 2. System Architecture

```
unified-engineering-workbench/
├── .gitattributes                 # Binary-file handling for PDFs, bundles, and recordings
├── .gitignore                     # Build/runtime files and temporary artifacts
├── .vscode/
│   ├── launch.json                # Packaged and source debugging profiles
│   └── tasks.json                 # Build and test tasks
├── CMakeLists.txt                 # Root CMake configuration (C++17, CTest)
├── Dockerfile                     # Container build and runtime
├── Launch.bat                     # Windows source-build fallback
├── Launch.exe                     # Generated on Windows before submission; absent in this checkout
├── README.md                      # Setup, architecture, formats, and requirements
├── dependencies.lock              # Runtime dependency and toolchain record
├── launch.sh                      # macOS/Linux launcher
├── render.yaml                    # Cloud deployment configuration
├── dist/windows-x64/
│   └── workbench_server.exe       # Generated Windows server; absent in this checkout
├── data/
│   ├── geometry/
│   │   ├── README.md              # Geometry provenance and units
│   │   ├── malformed_corrupt.stl  # Parser rejection fixture
│   │   ├── malformed_empty.stl    # Parser rejection fixture
│   │   └── sample_heatsink.stl    # Self-created sample geometry
│   └── thermal/
│       ├── README.md              # Dataset schema and synthetic-data note
│       ├── ansys_nodal_temperature.csv
│       ├── malformed_corrupt.csv
│       └── malformed_empty.csv
├── docs/
│   ├── AI_USE_LOG.md              # AI assistance and validation record
│   ├── APPENDIX_BACKPROP_DERIVATION.md
│   ├── DEPENDENCIES.md            # Tool and dependency versions
│   ├── EMBEDDED_SYSTEM_SPEC.md    # Hardware interfaces and voltage assumptions
│   ├── GIT_LOG.md                 # Development history and archive notes
│   ├── OWNERSHIP.md               # Original/reused work statement
│   ├── REPORT.md                  # Technical report source
│   ├── REPORT.pdf                 # Six-page submission report
│   ├── SUBMISSION_AUDIT.md        # Requirement-by-requirement status
│   ├── WALKTHROUGH.md             # Build and verification record
│   ├── demo/
│   │   ├── README.md              # Evidence capture instructions
│   │   ├── demo.mov               # Local user-provided partial; included directly in ZIP, not Git history
│   │   ├── screenshot_walkthrough.md # Timestamped frame sequence and verification notes
│   │   ├── verification.log        # Time-stamped local HTTP smoke check
│   │   └── screenshots/
│   │       ├── 01_145306_geometry.png
│   │       ├── 02_145314_wireframe.png
│   │       ├── 03_145322_geometry_detail.png
│   │       ├── 04_145330_thermal_fea.png
│   │       ├── 05_145346_thermal_3d.png
│   │       ├── 06_145354_performance_setup.png
│   │       ├── 07_145358_performance_results.png
│   │       ├── 08_145410_hardware.png
│   │       └── 09_145418_geometry_end.png
│   ├── handwritten_evidence/
│   │   ├── task_f_handwritten_notes_page_1.png
│   │   └── task_f_handwritten_notes_page_2.png
│   ├── screenshots/
│   │   ├── 01_geometry_shaded_surface.jpg
│   │   ├── 02_geometry_wireframe.jpg
│   │   ├── 03_thermal_fea_temperature_profile.jpg
│   │   ├── 04_thermal_3d_rod_visualizer.jpg
│   │   ├── 05_thermal_3d_ansys_point_cloud.jpg
│   │   ├── 06_performance_lab_execution_time.jpg
│   │   ├── 07_performance_lab_speedup_chart.jpg
│   │   ├── 08_hardware_workbench_block_diagram.jpg
│   │   └── README.md              # Screenshot index and evidence limits
│   └── workbench_repo.bundle      # Historical bundle; excluded from the submission ZIP
├── packaging/windows/launcher.cpp # Native Windows GUI launcher source
├── results/
│   ├── backprop_training_loss.csv
│   ├── benchmark_serial_parallel.csv
│   ├── fea_convergence.csv
│   └── plots/
│       ├── backprop_loss_curve.svg
│       ├── fea_mesh_convergence.svg
│       ├── fea_temperature_profile.svg
│       └── multicore_speedup_efficiency.svg
├── scripts/
│   ├── build.sh                   # Local build and tests
│   ├── build_windows_runtime.ps1  # Windows x64 runtime build and hash report
│   ├── create_sanitized_bundle.py # Anonymized history bundle for the ZIP
│   ├── package_submission.sh      # Submission ZIP preflight and archiver
│   ├── probe_movie.py             # MP4/MOV video-track and duration check
│   ├── render_report.py           # Build the six-page PDF report
│   └── run_benchmarks.sh          # Headless benchmark and results export
├── src/
│   ├── backend/
│   │   ├── CMakeLists.txt
│   │   ├── include/workbench/
│   │   │   ├── cad/
│   │   │   │   ├── ansys_parser.hpp
│   │   │   │   └── stl_parser.hpp
│   │   │   ├── common/
│   │   │   │   ├── json.hpp
│   │   │   │   ├── system_info.hpp
│   │   │   │   └── types.hpp
│   │   │   ├── embedded/device_adapter.hpp
│   │   │   ├── fea/
│   │   │   │   ├── thermal_1d.hpp
│   │   │   │   └── thermal_batch.hpp
│   │   │   ├── ml/backprop.hpp
│   │   │   ├── multicore/thread_pool.hpp
│   │   │   └── server/http_server.hpp
│   │   └── src/
│   │       ├── cad/
│   │       │   ├── ansys_parser.cpp
│   │       │   └── stl_parser.cpp
│   │       ├── common/system_info.cpp
│   │       ├── embedded/device_adapter.cpp
│   │       ├── fea/
│   │       │   ├── thermal_1d.cpp
│   │       │   └── thermal_batch.cpp
│   │       ├── main.cpp
│   │       ├── ml/backprop.cpp
│   │       ├── multicore/thread_pool.cpp
│   │       └── server/http_server.cpp
│   └── frontend/
│       ├── public/
│       │   ├── index.html
│       │   └── styles.css
│       └── src/
│           ├── app.js
│           ├── chart_engine.js
│           ├── renderer3d.js
│           ├── view_geometry.js
│           ├── view_hardware.js
│           ├── view_performance.js
│           └── view_thermal.js
└── tests/test_main.cpp            # Positive, negative, and numerical checks
```

`Launch.exe` and the Windows server are generated by `scripts/build_windows_runtime.ps1`; they are not present in this checkout. The supplied MOV is shorter than the video requirement, so the package uses its timestamped screenshot walkthrough and separate HTTP verification log. The packager also creates `docs/submission_history.bundle` from committed `main` immediately before building the final archive.

---

## 3. Supported CAD & Data Formats

| Format | File Extension | Schema / Columns | Enforced Limits | Validation Rules |
| :--- | :--- | :--- | :--- | :--- |
| **STL Geometry** | `.stl` | ASCII facet records or standard binary 50-byte facet records | Up to **150 MiB** and **200,000 triangles** for ASCII and binary STL | C++ parser enforces finite coordinates (rejects `NaN`, `Inf`), validates complete facets and the ASCII `endsolid` terminator, and computes missing normals. STL units are selected by the user because the format does not store them. |
| **ANSYS Nodal Temperature** | `.csv` | `node_id,x_m,y_m,z_m,temperature_C` | Up to **50 MiB** / **200,000 nodes** | Enforces positive integer node IDs, rejects duplicate IDs and non-finite floats, explains empty files. |

---

## 4. CMake Build & Test Instructions

### Building from Source:
```bash
# Configure Release build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Compile all targets (core library, server, and test suite)
cmake --build build --config Release
```

### Running the Test Suite:
```bash
# Run tests with CTest
ctest --test-dir build --output-on-failure

# Or execute test runner directly for detailed verbose output:
./build/src/backend/workbench_tests
```

### Running Headless Benchmarks & Exporting Results:
```bash
./build/src/backend/workbench_server --run-all-headless
```

---

## 5. Overview of Tasks & Rubric Alignment

1. **Task A: Coherent Interface & One-Click Delivery**: Single unified UI with 4 dedicated views (Geometry, Thermal, Performance, Hardware), responsive progress status, parameter validation, and actionable error banners.
2. **Task B: CAD / ANSYS-Compatible CSV Visualization**: Interactive 3D WebGL viewer for STL surfaces and temperature-colored nodal point clouds with a matching °C legend and hover/click node inspection.
3. **Task C: 1D Thermal FEA Solver**: Steady-state Galerkin rod solver using linear elements, $O(N)$ Thomas algorithm, midpoint error analysis demonstrating second-order $O(h^2)$ convergence rate $\alpha = 2.000$, and nodal superconvergence verification.
4. **Task D: Multicore CPU Parallel Worker Pool**: Bounded `ThreadPool` executing batch thermal solves with 1, 2, and 4 threads, total RAM reporting, one untimed warm-up per measured worker count, 5 measured repetitions, median time, measured speedup, and serial-parallel agreement verification ($\le 10^{-8\ \circ}\text{C}$). On one-logical-CPU systems it checks the parallel path for correctness and marks speedup as unavailable.
5. **Task E: Embedded-System Integration Design**: Complete block diagram, pin-voltage table, JTAG debug interface clarification, and C++ mock adapters (`McuAdapter`, `FpgaAdapter`, `JtagAdapter`) with simulated timeout fault and automated recovery.
6. **Task F: Analytical Backpropagation**: Step-by-step forward pass, chain-rule analytical derivatives, central finite-difference verification ($\epsilon = 10^{-6}$, max error $< 1.13 \times 10^{-10} \le 10^{-5}$), simultaneous parameter update, and 100-step monotonic loss descent.

---

## 6. Known Limitations & Safe Boundary Conditions

1. **FEA Spatial Dimensionality**: The current finite element solver is formulated for 1D rods with constant cross-section. Full 3D solid FEA requires tetrahedral or hexahedral 3D mesh discretizations.
2. **STL Units**: STL does not define a unit system. Select the source coordinate unit (mm, m, or in) in the Geometry panel; reported dimensions are converted to millimetres. The selection affects measurement reporting, not the rendered shape.
3. **Hardware Emulation**: Embedded devices operate through high-fidelity C++ mock adapters rather than physical USB COM ports, as specified in Task E.

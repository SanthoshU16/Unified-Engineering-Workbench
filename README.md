# Unified Engineering Workbench (UEW)

[![C++17](https://img.shields.io/badge/Language-C%2B%2B17-success.svg)](https://en.cppreference.com/w/cpp/17)
[![CMake](https://img.shields.io/badge/Build-CMake%203.20%2B-brightgreen.svg)](https://cmake.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows%2011%20x64%20%7C%20macOS%20%7C%20Linux-lightgrey.svg)]()
[![Tests](https://img.shields.io/badge/CTest-14%2F14%20Passed%20(100%25)-success.svg)]()

> **PASS Science Works | R&D Internship Assessment 2026–2027**  
> Comprehensive, reliable engineering workbench combining CAD geometry processing, numerical thermal finite-element analysis (FEA), multicore parallel execution, embedded hardware workbench integration, and analytical backpropagation.

---

## 1. Run the Workbench

The application has no third-party runtime libraries. A source build requires **CMake 3.20+** and a **C++17 compiler**.

### Clone Repository:
```bash
git clone https://github.com/SanthoshU16/Unified-Engineering-Workbench.git
cd Unified-Engineering-Workbench
```

### Windows 11 x64
The packaged application should include `Launch.exe` and `dist/windows-x64/workbench_server.exe`; double-click `Launch.exe` for the self-contained route. `Launch.bat` uses that package when present. In a source checkout it rebuilds the backend with CMake before launch, so recent source changes are not hidden behind an old executable.

> **Packaging status:** This source checkout does not contain a Windows executable. Build the Windows x64 runtime before distributing a zero-install archive; the source fallback is not a substitute for that deliverable.

### macOS / Linux
Run the shell launcher:
```bash
./launch.sh
```
When CMake is available, the script reconfigures and incrementally rebuilds the backend before launch; otherwise it can use an existing prebuilt backend.

### Editor Run Configuration (VS Code / Antigravity IDE)
1. Open the project folder.
2. Choose the Windows x64 configuration on Windows or the macOS/Linux configuration on Unix-like systems.
3. Press **F5**. The task configures CMake before building the backend.
4. The Windows configuration uses the Microsoft C++ debugger. The macOS/Linux configuration uses CodeLLDB.

---

## 2. System Architecture

```
unified-engineering-workbench/
├── CMakeLists.txt                 # Root CMake configuration (C++17, CTest)
├── Launch.bat                     # Windows one-click launcher
├── launch.sh                      # macOS/Linux one-click launcher
├── .vscode/                       # Editor run and debug configurations
│   ├── launch.json                # F5 debug/run configuration
│   └── tasks.json                 # Build & test task definitions
├── backend/                       # Core C++17 application logic
│   ├── include/workbench/
│   │   ├── common/                # Types, Vec3, STL/ANSYS models, JSON serializer
│   │   ├── fea/                   # 1D Thermal FEA solver & batch generator
│   │   ├── cad/                   # STL & ANSYS CSV parsers
│   │   ├── multicore/             # Bounded C++ thread pool
│   │   ├── embedded/              # MCU, FPGA, and JTAG mock adapters
│   │   ├── ml/                    # Analytical backpropagation & finite difference check
│   │   └── server/                # Cross-platform socket HTTP/REST server
│   ├── src/                       # C++ implementations
│   └── tests/                     # Test suite (test_main.cpp with positive, negative, and numerical tests)
├── frontend/                      # Presentation layer (100% offline, zero npm dependencies)
│   ├── public/
│   │   ├── index.html             # Single Page Application
│   │   └── styles.css             # Midnight plum and ion-orchid engineering workstation theme
│   └── src/
│       ├── app.js                 # UI coordinator, navigation, and REST transport
│       ├── renderer3d.js          # Zero-dependency WebGL 3D engine (STL + Point Cloud)
│       ├── chart_engine.js        # Zero-dependency 2D Canvas charting engine
│       ├── view_geometry.js       # CAD 3D STL viewer
│       ├── view_thermal.js        # ANSYS point cloud & 1D FEA solver
│       ├── view_performance.js    # Multicore benchmark runner & scaling charts
│       └── view_hardware.js       # Embedded block diagram, tables, and device cards
├── samples/                       # Sample datasets with provenance
│   ├── geometry/                  # STL samples (sample_heatsink.stl, validation fixtures)
│   └── thermal/                   # Synthetic ANSYS-style nodal CSV (sample and parser error cases)
├── results/                       # Generated evidence, raw CSVs, and labelled plots
│   ├── fea_convergence.csv        # FEA midpoint error data
│   ├── benchmark_serial_parallel.csv # 1, 2, 4 worker scaling measurements
│   ├── backprop_training_loss.csv # 100-step training loss history
│   └── plots/                     # High-resolution vector SVG plots
├── docs/                          # Comprehensive technical documentation
│   ├── REPORT.md                  # Technical report and measured results
│   ├── APPENDIX_BACKPROP_DERIVATION.md # Backpropagation derivation
│   ├── EMBEDDED_SYSTEM_SPEC.md    # Hardware interface & pin-voltage specification
│   ├── WALKTHROUGH.md             # Walkthrough script and verification log
│   ├── AI_USE_LOG.md              # AI assistance disclosure & challenge test
│   └── OWNERSHIP.md               # Statement of original vs reused work
└── scripts/
    ├── build.sh                   # Build & test automation script
    ├── run_benchmarks.sh          # Headless benchmark & results export
    └── package_submission.sh      # Submission zip archiver
```

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
./build/backend/workbench_tests
```

### Running Headless Benchmarks & Exporting Results:
```bash
./build/backend/workbench_server --run-all-headless
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

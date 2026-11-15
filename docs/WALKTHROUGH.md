# Unified Engineering Workbench — Walkthrough & Verification Evidence

**PASS Science Works | R&D Internship Assessment 2026–2027**  
**Document ID:** UEW-WALKTHROUGH-001  
**Updated:** 2026-11-15
**Verification Environment:** macOS / Apple silicon. Windows packaging was not built or verified in this environment.  

> This file documents the demonstration flow and captured command-line checks. It does not contain a live screen recording or screenshots; the interaction descriptions below are a walkthrough script, not recorded evidence.

---

## 1. Build & Test Suite Verification Log

```bash
$ cmake --build build --config Release
[100%] Built target workbench_server
[100%] Built target workbench_tests

$ ctest --test-dir build --output-on-failure
1/1 Test #1: AllWorkbenchTests ... Passed
100% tests passed, 0 tests failed

$ ./build/backend/workbench_tests
Test Summary: 14/14 tests passed

$ for file in frontend/src/*.js; do node --check "$file"; done
# All frontend JavaScript syntax checks exited successfully.
```

---

## 2. Interactive User Interface Walkthrough

### View 1: Geometry View (Task B STL 3D Viewer)
- **Action**: User navigates to the *Geometry (STL)* tab.
- **Log / Event**:
  `[HttpServer] POST /api/cad/stl/parse filepath="samples/geometry/sample_heatsink.stl"`
- **Observed Behavior**:
  - The C++ backend parses the sample STL and reports its triangle count and dimensions.
  - The self-created CC0 sample is identified in the selector, and its coordinate unit (metres) is set explicitly because STL does not store units.
  - Metadata card displays: Solid: `heatsink_assessment`, Triangles: `72`, Dimensions: `100.0 x 100.0 x 50.0 mm`, Size: `14.4 KiB`.
  - WebGL viewport renders the heat sink with brushed copper shading and directional lighting.
  - User drags to orbit, Shift-drags to pan, and scrolls to zoom. Clicking *Show wireframe* reveals the triangular mesh topology.
  - Negative validation tested: Selecting `malformed_corrupt.stl` immediately triggers the red actionable alert banner:  
    `STL Validation Rejection: Parse Error [line 4]: Non-finite vertex coordinate (NaN or Inf detected).`

---

### View 2: Thermal Analysis View (Task B ANSYS & Task C FEA)
- **Mode A: 1D Thermal FEA Solver (Task C)**:
  - Input parameters: $L = 1.0\text{ m}, A = 10^{-4}\text{ m}^2, k = 10\text{ W/(m K)}, q = 10000\text{ W/m}^3, T(0) = 100^\circ\text{C}, T(L) = 20^\circ\text{C}, N = 10$.
  - User clicks *Solve FEA (C++)*.
  - Status updates: `FEA Solved: N=10, Max Midpoint Error=1.250000 °C, Max Nodal Error=2.84e-14 °C`.
  - Chart 1 renders $T(x)$ parabola reaching peak $185.0^\circ\text{C}$ at midpoint, overlaying discrete FEA nodes on the exact analytical curve.
  - Chart 2 renders log-log mesh convergence across $N = 10, 20, 40, 80$, showing an exact second-order slope $\alpha = 2.000$.
  - Table populates with midpoint errors: $1.25^\circ\text{C} \to 0.3125^\circ\text{C} \to 0.078125^\circ\text{C} \to 0.0195312^\circ\text{C}$.
  - User clicks *Export FEA CSV*, which instantly downloads `fea_thermal_N10_results.csv`.
- **Mode B: ANSYS-Compatible 3D Point Cloud (Task B)**:
  - User switches dropdown to *Synthetic ANSYS-style CSV*.
  - The C++ backend parses `samples/thermal/ansys_nodal_temperature.csv` (1,440 nodes), which is labelled synthetic and was not exported by ANSYS.
  - WebGL viewport renders a 3D point cloud colored by temperature from $55.62^\circ\text{C}$ (cool green) to $134.93^\circ\text{C}$ (warm terracotta).
  - Hovering the mouse over a node displays the inspection tooltip:  
    `Node ID: 524 | X: 0.0000 m | Y: 0.0000 m | Z: 0.0100 m | Temp: 98.42 °C`.

---

### View 3: Performance View (Task D Multicore Scaling)
- **Action**: User enters workload parameters: 20,000 thermal cases, mesh size $N = 200$, RNG Seed = 42, and clicks *Run Parallel Benchmark (C++)*.
- **Observed Behavior**:
  - UI remains completely responsive with a pulsing progress badge (`Multicore C++ Pool Running`).
  - One untimed warm-up pass is executed per measured worker count, followed by 5 repetitions on 1, 2, and 4 threads.
  - Results Table populates:
    - 1 Worker (Serial): **50.716 ms** (Baseline 1.00x)
    - 2 Workers: **31.447 ms** (Speedup **1.61x**, Efficiency 80.6%)
    - 4 Workers: **18.971 ms** (Speedup **2.67x**, Efficiency 66.8%)
  - Agreement Badge displays: `Agreement: PASS (Max Diff = 0.00e+00 °C <= 1e-8)`.
  - The chart plots median execution time; the results table reports speedup and efficiency. Parallel timings include worker-pool startup and dispatch overhead.

---

### View 4: Hardware View (Task E Embedded Workbench)
- **Action**: User opens the *Hardware* view.
- **Observed Behavior**:
  - The architecture diagram shows the Host USB hub, MCU, FPGA, and dedicated JTAG adapter; the interface matrix lists power conversion, ground references, voltage domains, translation, host protocol, connector, and signal ownership.
  - The page identifies the adapters as a software simulation; no physical USB device is controlled.
  - User clicks *Discover*, then *Connect* on the STM32 MCU: status transitions `DISCONNECTED` → `DISCOVERED` → `CONNECTED` → `STREAMING`. Telemetry reports the simulated 3.298 V rail and core temperature.
  - *Capabilities* returns device architecture and interfaces. *Inject Timeout* is enabled only when connected and represents an expired 250 ms acknowledgement deadline.
  - *Auto-Recover* is enabled only during `ERROR_TIMEOUT`; recovery returns to `CONNECTED`, and the next telemetry poll resumes `STREAMING`.

---

### Task F: Backpropagation evidence (separate from the app)
- Task F is implemented and verified in the C++ backend; it is not an application view.
- The report and `results/` contain the numerical results, loss curve, and training CSV.
- Submit the required handwritten forward pass, chain-rule derivatives, simultaneous update, and gradient-direction explanation as a legible scan or photo of at most two pages. The digital derivation in `docs/` is a reference and does not replace the handwritten evidence.


# Unified Engineering Workbench — Technical Evaluation Report

**PASS Science Works | R&D Internship Assessment 2026–2027**  
**Candidate Evaluation Brief — 100 Points**  
**Author:** Candidate ID: *SanthoshU*
**Date:** November 2026  
**Target Platform:** Windows 11 x64 (Compatible with macOS and Linux x86_64/ARM64)  

---

## Executive Summary

The **Unified Engineering Workbench (UEW)** is a modular, high-reliability engineering workstation developed to unify computer-aided design (CAD) geometry processing, numerical mechanical thermal finite-element analysis (FEA), multicore parallel execution, embedded hardware workbench integration, and gradient-based backpropagation.

In strict adherence to the assessment brief:
- All core application logic, numerical routines, file parsing, and multithreading are implemented in modern, clean **C++17** with zero external runtime dependencies.
- A single coherent, responsive user interface provides interactive 3D WebGL visualizations, real-time charting, and hardware telemetry.
- CMake source builds and the browser interface are implemented. The Windows x64 packaged runtime is still missing, so the strict zero-install `Launch.exe` requirement is not yet met.

---

## 1. System Architecture & Design Choices (Task A)

### 1.1 Separation of Concerns
The workbench enforces strict separation between computational core logic, network transport, and graphical presentation:
1. **Core C++ Engine (`src/backend/include/workbench/` and `src/backend/src/`)**:
   - `fea::Thermal1DSolver`: Implements Galerkin linear finite-element formulation, element stiffness/load assembly, and $O(N)$ Thomas algorithm solving.
   - `fea::BatchThermalSolver`: Coordinates reproducible pseudo-random workload generation, serial execution, and thread-pool parallelization.
   - `cad::STLParser` & `cad::ANSYSParser`: High-speed parsers with complete validation, NaN/Inf rejection, and bounding box computation.
   - `multicore::ThreadPool`: Bounded worker pool using standard C++ threads (`std::thread`, `std::mutex`, `std::condition_variable`).
   - `embedded::DeviceManager`: Object-oriented mock hardware adapters (`McuAdapter`, `FpgaAdapter`, `JtagAdapter`) with simulated fault injection.
   - `ml::BackpropagationLab`: Exact analytical forward pass, chain-rule differentiation, and central finite-difference verification.
2. **Zero-Dependency Transport Layer (`server::HttpServer`)**:
   - A lightweight, cross-platform socket server written in standard C++17.
   - Desktop mode binds to `127.0.0.1:8080` (or searches the next available port). Cloud/container mode binds to all interfaces when deployment environment variables are present. It serves static web assets and REST JSON endpoints (`/api/...`).
   - Detaches client requests onto asynchronous threads to preserve UI responsiveness during heavy workloads.
3. **Presentation Layer (`src/frontend/`)**:
   - Built with Vanilla ES6+ and HTML5 WebGL / 2D Canvas without heavy npm or external CDN dependencies, ensuring **100% offline functionality**.

### 1.2 Responsiveness & Safe Execution
Request handlers run independently from the socket accept loop, and batch thermal solves use a bounded worker pool. The interface updates state through non-blocking `fetch()` calls with progress labels and actionable error alerts.

---

## 2. CAD & ANSYS Data Visualization (Task B)

### 2.1 STL Parser & 3D Surface Rendering
- **Parser Implementation**: `STLParser` accepts ASCII facet records and standard binary STL records (80-byte header, facet count, and 50-byte facets).
- **Robust Validation**: Rejects non-finite numbers (`NaN`, `Inf`), incomplete ASCII facets, missing ASCII `endsolid` terminators, and malformed binary records. Missing or zero-length normals are recomputed from triangle vertices.
- **Supported Limits**: STL uploads are capped at **150 MiB** and **200,000 triangles** for both ASCII and binary files. The C++ parser validates the file before the browser renders it. Larger models receive a clear validation error; no 60 FPS claim is made.
- **Units & provenance**: STL does not encode units. The UI asks for the source coordinate unit and converts reported dimensions to millimetres. The included heat sink is a self-created CC0 1.0 sample with metre coordinates.
- **3D Interaction**: Full 3D camera controls: Orbit (Left Drag), Pan (Right Drag / Shift Drag), Zoom (Mouse Wheel), Wireframe overlay toggle, and Bounding Box auto-fit.

### 2.2 ANSYS-Compatible CSV Nodal Temperature Point Cloud
- **Schema**: Expected columns: `node_id,x_m,y_m,z_m,temperature_C`.
- **Validation**: Enforces positive integer `node_id`, duplicate node ID detection, strict finite float parsing, and descriptive error messages for empty or corrupted files. Inputs are capped at 50 MiB and 200,000 nodes; the automated parser check verifies rejection above the byte limit.
- **Visualization**: Rendered as a 3D point cloud with a temperature scale in °C and hover/click node inspection displaying coordinates in metres and temperature. The bundled example is synthetic ANSYS-style data, not a solver export; point-cloud registration to the STL and the independent 1D rod model is outside scope.

---

## 3. Mechanical Thermal FEA & Interdisciplinary Adoption (Task C)

### 3.1 Mathematical Formulation
The steady-state 1D heat conduction along a uniform rod of length $L$, cross-sectional area $A$, and thermal conductivity $k$ with uniform volumetric heat generation $q$ is governed by:
$$-\frac{d}{dx}\left(k A \frac{dT}{dx}\right) = q A, \quad x \in [0, L]$$
Subject to Dirichlet boundary conditions:
$$T(0) = T_0 = 100^\circ\text{C}, \quad T(L) = T_L = 20^\circ\text{C}$$

Assuming insulated lateral surfaces, constant cross-sectional area $A = 10^{-4}\text{ m}^2$, $k = 10.0\text{ W/(m K)}$, $q = 10,000\text{ W/m}^3$, and $L = 1.0\text{ m}$.

### 3.2 Analytical Exact Solution
Integrating $-k \frac{d^2T}{dx^2} = q$ twice and applying boundary conditions yields:
$$T_{\text{exact}}(x) = T_0 + \frac{T_L - T_0}{L}x + \frac{q}{2k}x(L - x) = 100 - \frac{80x}{L} + \left[\frac{q}{2k}\right] x(L - x)$$
At the rod midpoint $x = 0.5\text{ m}$:
$$T_{\text{exact}}(0.5) = 100 - 40 + \frac{10000}{20}(0.5)(0.5) = 60 + 125 = 185.0^\circ\text{C}$$

### 3.3 Finite-Element Discretization (Linear 2-Node Elements)
For element $e$ of length $h = L/N$:
- **Element Stiffness Matrix**:
  $$\mathbf{K}^e = \frac{kA}{h} \begin{bmatrix} 1 & -1 \\ -1 & 1 \end{bmatrix}$$
- **Element Load Vector**:
  $$\mathbf{F}^e = \frac{qAh}{2} \begin{bmatrix} 1 \\ 1 \end{bmatrix}$$
- Global assembly results in an $(N+1) \times (N+1)$ tridiagonal system solved in $O(N)$ operations using the **Thomas algorithm (TDMA)**.

### 3.4 Midpoint Error Analysis & Mesh Convergence
Because the 1D linear FEA Green's function coincides with the exact solution space at discrete nodes, **nodal errors are identically zero** ($< 10^{-13\ \circ}\text{C}$, machine precision superconvergence). Therefore, true finite element discretization error must be assessed at **element midpoints**:
$$x_{\text{mid}}^e = \frac{x_i + x_{i+1}}{2}, \quad T_{\text{FE}}(x_{\text{mid}}^e) = \frac{T_i + T_{i+1}}{2}$$
The theoretical midpoint interpolation error for a quadratic curve with $T''(x) = -q/k$ is:
$$E_{\text{theory}} = \frac{1}{8} \left| \frac{d^2T}{dx^2} \right| h^2 = \frac{q}{8k} h^2 = \frac{10000}{80} h^2 = 125 h^2$$

#### Experimental Verification Results:
| Mesh Elements ($N$) | Element Size $h$ (m) | Max Midpoint Error ($^\circ\text{C}$) | Theoretical Error ($125 h^2$) | Convergence Rate ($\alpha$) | Max Nodal Error ($^\circ\text{C}$) |
| :---: | :---: | :---: | :---: | :---: | :---: |
| **10** | 0.1000 | 1.25000000 | 1.25000000 | Ref (Base) | $2.84 \times 10^{-14}$ |
| **20** | 0.0500 | 0.31250000 | 0.31250000 | **2.0000** | $1.99 \times 10^{-13}$ |
| **40** | 0.0250 | 0.07812500 | 0.07812500 | **2.0000** | $8.81 \times 10^{-13}$ |
| **80** | 0.0125 | 0.01953125 | 0.01953125 | **2.0000** | $1.22 \times 10^{-12}$ |

**Debugging Insight:** An engineer evaluating accuracy solely at mesh vertices would observe $\approx 10^{-13\ \circ}\text{C}$ and mistakenly conclude either that linear FEA is unconditionally exact or that mesh convergence cannot be demonstrated. Evaluating at element midpoints reveals the true physical $O(h^2)$ second-order discretization error.

---

## 4. Multicore CPU Parallel Execution & Scaling (Task D)

### 4.1 Thread Pool Architecture
Batch solves are dispatched to a bounded worker pool (`multicore::ThreadPool`) instantiated with $P \in \{1, 2, 4\}$ worker threads. Independent cases are chunked into contiguous index ranges to eliminate queue lock contention while ensuring **strict deterministic input ordering** in the output result vector.

### 4.2 Benchmark Methodology & Measurements
- **Workload**: 2,000 independent rods with varied positive thermal properties, mesh size $N = 100$, generated via fixed seed PRNG (`std::mt19937_64`, seed = 42). These values match the checked-in raw CSV and captured performance screenshots.
- **Environment**: Apple M2, 8 physical / 8 logical CPU cores, 16 GiB installed RAM, Clang 21.0, `-O3`.
- **Timing**: One untimed warm-up pass is executed for each measured worker count. Workload generation, warm-up, file loading, rendering, and compilation are excluded from reported compute time. Five measured repetitions per worker count are reported with the median. Each parallel timed call includes thread-pool creation/destruction and chunk scheduling, so its measurement represents the end-to-end parallel solve call.
- **Launch-to-ready**: The UI reports backend startup from entry into `main()` until the HTTP server is listening, separately from solver timing. It excludes operating-system process loading and browser page loading; headless runs label this metric not applicable.

#### Experimental Benchmark Results:
| Worker Count ($P$) | Rep 1 (ms) | Rep 2 (ms) | Rep 3 (ms) | Rep 4 (ms) | Rep 5 (ms) | Median Time ($T_p$) | Speedup ($T_1 / T_p$) | Efficiency ($S_p / P$) |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **1 Worker (Serial)** | 9.73 | 5.57 | 5.22 | 4.70 | 4.39 | **5.22 ms** | 1.00x | 100.0% |
| **2 Workers** | 2.73 | 2.71 | 2.52 | 2.39 | 2.36 | **2.52 ms** | **2.07x** | 103.6% |
| **4 Workers** | 1.46 | 1.40 | 1.32 | 1.33 | 1.32 | **1.33 ms** | **3.92x** | 98.1% |

- **Parallel Verification**:
  $$\max_{i, j} |T_{\text{serial}}(i, j) - T_{\text{parallel}}(i, j)| = 0.00000000^\circ\text{C} \le 10^{-8\ \circ}\text{C} \quad \text{[PASSED]}$$
  The measured medians give 2.07x speedup on 2 workers and 3.92x on 4. The 2-worker efficiency is slightly above 100%; with this short workload, cache/scheduling and timing variation can produce that result, so it is not a claim of sustained superlinear scaling. Thread creation, task dispatch, synchronization, and result allocation are included in each parallel call; these costs can reduce or erase the benefit for smaller workloads. The elevated first serial sample shows why five repetitions and a median are used. Results vary with CPU load, power state, compiler, and machine; no minimum speedup or high-end hardware is assumed. On a one-logical-CPU host, only the serial run is timed; the 2-worker path is still executed for correctness, and the UI/CSV state that speedup cannot be assessed.

---

## 5. Embedded-System Integration Design (Task E)

### 5.1 Architecture & Block Diagram
The workbench designs a modular interface connecting the Host PC to three heterogeneous embedded boards:
1. **Microcontroller Board (STM32F401)**: ARM Cortex-M4 @ 84 MHz, 3.3V logic. Dedicated USB-CDC Virtual COM Port for telemetry (115200 8N1).
2. **FPGA Board (Xilinx Artix-7 XC7A35T)**: Multi-rail FPGA (1.0V VCCINT core, 1.8V VCCAUX, 3.3V VCCO I/O). FTDI FT2232H High-Speed USB FIFO for telemetry streaming.
3. **Dedicated JTAG Debug Adapter (SEGGER J-Link / FTDI MPSSE)**: Dedicated hardware TAP state machine master with target VREF sensing (1.2V to 3.3V).

### 5.2 Architectural Role Distinction
> **Critical Architectural Clarification:** A JTAG adapter is a **hardware debug interface**, NOT a generic interchangeable compute board. It masters the IEEE 1149.1 boundary-scan TAP bus (driving TCK, TMS, TDI; sampling TDO; sensing VTREF) to halt CPUs, inspect registers, and program bitstreams. It cannot be treated as a compute node, and target boards cannot be assumed to share common connectors, voltages, or protocols.

### 5.3 Interface & Pin-Voltage Specification:
| Device | Role | Power Source | Voltage Domains | Level Shifting | Physical Connector | Host Protocol | JTAG Pinout & Ownership |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **STM32F401 MCU** | Target Compute Node | 5V USB $\to$ 3.3V LDO | 3.3V LVCMOS | Direct 3.3V | USB Micro-B + 10-pin Cortex SWD | USB-CDC UART (115200 8N1) | Slave: SWCLK, SWDIO, nRST, GND |
| **Artix-7 FPGA** | Target Accelerator | 5V USB $\to$ 1.0V, 1.8V, 3.3V PMIC | 1.0V core, 1.8V aux, 3.3V banks | Active TXS0108E bidirectional shifters | USB-B + 14-pin 2mm JTAG Header | FTDI FT2232H High-Speed FIFO | Slave: TCK, TMS, TDI, TDO, VTREF |
| **JTAG Debug Probe** | Hardware Debug Master | 5V USB Host Powered | 1.2V–3.3V Target Auto-Sense | Integrated active voltage followers | USB-A to 20-pin / 10-pin Target Cable | USB 2.0 Bulk Endpoint | Master: Drives TCK/TMS/TDI; Samples TDO/VTREF |

### 5.4 C++ Mock Adapters & Recoverable Timeout Simulation
- Implemented `McuAdapter`, `FpgaAdapter`, and `JtagAdapter` under common abstract base `IDeviceAdapter`.
- Demonstrated discovery, capability reporting, an enforced `DISCONNECTED → DISCOVERED → CONNECTED → STREAMING` lifecycle, and live telemetry.
- **Recoverable Timeout Failure**: Injected via `inject_failure("TIMEOUT")` only for connected devices. The deterministic mock represents an expired 250 ms acknowledgement deadline, transitions to `ERROR_TIMEOUT`, and alerts the host. Recovery is accepted only during an active timeout; it clears simulated buffers, resets link handshakes (or the JTAG TAP), and returns the adapter to `CONNECTED`. The next telemetry poll resumes `STREAMING`.
- All controls operate on software mock adapters and do not enumerate or control physical USB devices.

---

## 6. Analytical Neural Network Backpropagation (Task F)

### 6.1 Derivation & Step 0 Evaluation
- **Sample**: Input $x = 1.0$, Target $y = 2.0$.
- **Model**: $z = w x + b$, $h = \tanh(z)$, $\hat{y} = v h + c$. Loss: $J = \frac{1}{2}(\hat{y} - y)^2$.
- **Initial Values**: $w = 0.5, b = 0.0, v = 1.0, c = 0.0$, Learning Rate $\eta = 0.1$.

#### Forward Pass (Step 0):
$$z = 0.5(1.0) + 0.0 = 0.50000000$$
$$h = \tanh(0.5) = 0.46211716$$
$$\hat{y} = 1.0(0.46211716) + 0.0 = 0.46211716$$
$$e = \hat{y} - y = 0.46211716 - 2.0 = -1.53788284$$
$$J = \frac{1}{2}(-1.53788284)^2 = 1.18254182$$

#### Chain Rule Derivatives:
$$\frac{\partial J}{\partial c} = \hat{y} - y = -1.53788284$$
$$\frac{\partial J}{\partial v} = (\hat{y} - y) h = -1.53788284 \times 0.46211716 = -0.71068205$$
$$\frac{\partial J}{\partial h} = (\hat{y} - y) v = -1.53788284$$
$$\frac{dh}{dz} = 1 - \tanh^2(z) = 1 - h^2 = 1 - (0.46211716)^2 = 0.78644773$$
$$\frac{\partial J}{\partial z} = \frac{\partial J}{\partial h} \frac{dh}{dz} = -1.53788284 \times 0.78644773 = -1.20946448$$
$$\frac{\partial J}{\partial b} = \frac{\partial J}{\partial z} \times 1 = -1.20946448$$
$$\frac{\partial J}{\partial w} = \frac{\partial J}{\partial z} \times x = -1.20946448$$

### 6.2 Finite-Difference Check & Simultaneous Parameter Update
Using central finite differences with $\epsilon = 10^{-6}$:
$$\frac{\partial J}{\partial \theta} \approx \frac{J(\theta + \epsilon) - J(\theta - \epsilon)}{2\epsilon}$$
| Parameter ($\theta$) | Initial $\theta_0$ | Analytical $\nabla J$ | Finite Difference $\nabla_{\text{FD}}$ | Absolute Error | Status ($\le 10^{-5}$) | Updated $\theta_1$ ($\eta = 0.1$) |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **$w$** | 0.50000000 | -1.20946448 | -1.20946448 | $1.13 \times 10^{-10}$ | **PASSED** | **0.62094645** |
| **$b$** | 0.00000000 | -1.20946448 | -1.20946448 | $1.13 \times 10^{-10}$ | **PASSED** | **0.12094645** |
| **$v$** | 1.00000000 | -0.71068205 | -0.71068205 | $5.25 \times 10^{-11}$ | **PASSED** | **1.07106820** |
| **$c$** | 0.00000000 | -1.53788284 | -1.53788284 | $1.12 \times 10^{-10}$ | **PASSED** | **0.15378828** |

- **Gradient Direction Explanation**: Because $\hat{y} \approx 0.462 < 2.0$, the network underpredicts. Since $v > 0$ and $h > 0$, increasing any parameter increases $\hat{y}$ toward $2.0$, reducing the error. Thus all partial derivatives are negative, and gradient descent updates ($\theta \leftarrow \theta - \eta \nabla J$) increase all parameters.
- **100-Step Convergence**: Training loss monotonically descends from $1.1825$ at Step 0 to $1.70 \times 10^{-20}$ at Step 100.

---

## 7. Test Outcomes, Validation Evidence & Limitations

### 7.1 Test Suite Results
The test suite (`workbench_tests`) executed via `ctest` achieves **100% pass rate (14/14 tests)** across all modules:
1. `test_fea_positive_solve`: Exact boundary and nodal agreement ($< 10^{-12}$ C).
2. `test_fea_convergence_rate`: Verifies $O(h^2)$ rate $\alpha = 2.000 \pm 0.001$.
3. `test_fea_negative_heat_source_error_bound`: Confirms a heat sink still reports a non-negative O(h²) error bound and second-order convergence.
4. `test_fea_csv_includes_convergence_data`: Confirms the export contains the selected mesh profile and all four mesh convergence rows.
5. `test_fea_negative_validation`: Verifies rejection of invalid geometry/material values, element counts, and non-finite temperatures.
6. `test_stl_parser_positive`: Accurate ASCII/binary parsing, triangle bounding boxes, and normals.
7. `test_json_string_upload_decode`: Preserves escaped quotes, slashes, line endings, and Unicode during upload decoding.
8. `test_stl_parser_negative`: Empty and over-150-MiB rejection, missing terminator rejection, and non-finite ASCII/binary detection.
9. `test_ansys_parser_positive`: Temperature range and mean statistics verification.
10. `test_ansys_parser_negative`: Rejection of empty/malformed data, duplicate or non-positive IDs, non-finite values, and buffers over the 50 MiB cap.
11. `test_thread_pool_and_batch_solver`: Verifies repeatable seeded cases, deterministic output order, and serial vs 2-/4-worker agreement $\le 10^{-8\ \circ}\text{C}$.
12. `test_benchmark_metadata_and_median`: Verifies RAM and warm-up metadata, parallel verification, speedup availability, JSON fields, and even-count median calculation.
13. `test_embedded_mock_adapters`: Discovery, streaming, timeout fault injection, and recovery.
14. `test_backpropagation_analytical_and_fd`: Verifies analytical gradients vs finite differences within $10^{-10} \le 10^{-5}$.

### 7.2 Known Limitations
1. **1D FEA Geometry**: The thermal finite-element solver is currently specialized for 1D rods with constant cross-section. 3D solid thermal solves require 3D tetrahedral mesh elements.
2. **STL Units**: STL files do not encode units. Select the source coordinate unit (mm, m, or in) in the Geometry panel; reported dimensions are converted to millimetres. The selection affects measurement reporting, not the rendered shape.
3. **Hardware Emulation**: Embedded devices operate via robust software mock state machines rather than physical USB COM ports, as specified in Task E.


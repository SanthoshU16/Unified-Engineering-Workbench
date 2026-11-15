# Walkthrough & Verification Evidence

**PASS Science Works | R&D Internship Assessment 2026–2027**  
**Document ID:** UEW-WALKTHROUGH-001  
**Updated:** 2026-11-15

## Evidence status

Eight static screenshots showing the four workbench views are stored in [`screenshots/`](screenshots/README.md). The two handwritten Task F pages are in [`handwritten_evidence/`](handwritten_evidence/). This document describes those artifacts and records reproducible command-line checks.

The user supplied `docs/demo/demo.mov`; its QuickTime metadata reports one video track and a duration of 72.875 seconds, below the assignment's 3–5 minute video requirement. To use the assignment's screenshot alternative, nine frames were extracted with real timestamps derived from the recording metadata and are linked from [`demo/screenshot_walkthrough.md`](demo/screenshot_walkthrough.md). The separate local HTTP smoke log is [`demo/verification.log`](demo/verification.log); it is clearly identified as a separate run, not console output from the recording. The walkthrough covers all four views and shows a completed performance run.

Use [`demo/README.md`](demo/README.md) for the Windows capture sequence and save final evidence in `docs/demo/` before packaging.

## Build and source verification

Verification environment: macOS on Apple silicon, Apple Clang 21.0, CMake 4.4.5, and Node.js 22.19.0. Windows packaging and clean-machine launch have not been verified.

Commands run for this audit:

```text
$ ./scripts/build.sh
Build and tests completed successfully!
CTest: 1/1 test target passed; 14/14 backend checks passed.

$ for file in src/frontend/src/*.js; do node --check "$file"; done
All frontend JavaScript syntax checks exited successfully.

$ ./build/src/backend/workbench_tests
Test Summary: 14/14 tests passed (ALL TESTS PASSED 100%)

Fresh out-of-tree CMake configure/build also completed successfully; CTest passed 1/1 target and the runner passed 14/14 checks.

Local HTTP smoke check against the restructured source tree after the desktop listener was changed to loopback-only:
Listener: `127.0.0.1:18080` (confirmed with `lsof`; no wildcard listener)
GET /                         -> 200
GET /styles.css               -> 200
GET /src/app.js               -> 200
GET /api/system/info          -> READY (port 18080)
POST /api/cad/stl/parse       -> valid=true, 72 triangles
POST /api/cad/ansys/parse     -> valid=true, 1,440 nodes
```

See [`SUBMISSION_AUDIT.md`](SUBMISSION_AUDIT.md) for the full requirement-by-requirement status and the remaining submission gates.

## Existing screen evidence

| View | Screenshot | Evidence shown |
|---|---|---|
| Geometry | [`01_geometry_shaded_surface.jpg`](screenshots/01_geometry_shaded_surface.jpg), [`02_geometry_wireframe.jpg`](screenshots/02_geometry_wireframe.jpg) | The self-created 72-triangle heat sink in shaded and wireframe modes. |
| Thermal | [`03_thermal_fea_temperature_profile.jpg`](screenshots/03_thermal_fea_temperature_profile.jpg), [`04_thermal_3d_rod_visualizer.jpg`](screenshots/04_thermal_3d_rod_visualizer.jpg), [`05_thermal_3d_ansys_point_cloud.jpg`](screenshots/05_thermal_3d_ansys_point_cloud.jpg) | FEA temperature profile and mesh error, 3D rod view, and synthetic ANSYS-style point cloud. |
| Performance | [`06_performance_lab_execution_time.jpg`](screenshots/06_performance_lab_execution_time.jpg), [`07_performance_lab_speedup_chart.jpg`](screenshots/07_performance_lab_speedup_chart.jpg) | Five-repetition measurements for 2,000 cases at N=100, with median times, speedup, efficiency, and agreement status. |
| Hardware | [`08_hardware_workbench_block_diagram.jpg`](screenshots/08_hardware_workbench_block_diagram.jpg) | MCU, FPGA, and dedicated JTAG mock-device topology. |

The screenshot index and its capture-commit note are in [`screenshots/README.md`](screenshots/README.md). The raw benchmark data is [`../results/benchmark_serial_parallel.csv`](../results/benchmark_serial_parallel.csv).

## Task F handwritten evidence

The two photographed pages are [`backprop_page_1.png`](handwritten_evidence/backprop_page_1.png) and [`backprop_page_2.png`](handwritten_evidence/backprop_page_2.png). They contain the forward pass, chain-rule gradients, central finite-difference comparison, simultaneous update, and gradient-direction explanation. The C++ training output is in [`../results/backprop_training_loss.csv`](../results/backprop_training_loss.csv).

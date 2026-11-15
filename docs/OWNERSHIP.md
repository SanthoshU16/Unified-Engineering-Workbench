# Statement of Ownership & Attribution

**PASS Science Works | R&D Internship Assessment 2026–2027**  
**Document ID:** UEW-OWN-001  

---

## 1. Original Work
The following components were authored and developed from scratch specifically for this assessment:
1. **1D Mechanical Thermal Finite-Element Solver (`backend/src/fea/thermal_1d.cpp`)**:
   - Original Galerkin weak-form discretization, element stiffness/load integration, and tridiagonal Thomas algorithm implementation.
   - Original midpoint error interpolation and convergence rate analysis routines.
2. **Batch Multicore Execution Engine (`backend/src/fea/thermal_batch.cpp`)**:
   - Original bounded worker pool (`multicore::ThreadPool`), reproducible pseudo-random workload generator, and serial-parallel agreement verification engine.
3. **CAD & ANSYS CSV Parsers (`backend/src/cad/`)**:
   - Original ASCII STL parser with validation, normal verification, and bounding box computation.
   - Original ANSYS CSV parser with coordinate/temperature validation and duplicate node detection.
4. **Embedded Mock Adapters (`backend/src/embedded/`)**:
   - Original object-oriented mock adapters (`McuAdapter`, `FpgaAdapter`, `JtagAdapter`) with simulated fault injection and recovery state machines.
5. **Backpropagation & Central Finite Difference Checker (`backend/src/ml/`)**:
   - Original analytical chain-rule forward/backward pass and central finite-difference verification engine.
6. **Zero-Dependency HTTP Server & Frontend UI (`backend/src/server/` & `frontend/`)**:
   - Cross-platform socket-based HTTP server, WebGL 3D CAD/point-cloud engine (`renderer3d.js`), and 2D canvas charting engine (`chart_engine.js`).
7. **Documentation, Derivations, and Plots (`docs/`, `results/plots/`)**:
   - Technical report, derivation appendix, block diagram, and SVG visualizers.

---

## 2. Reused Code & Open-Source References
- **Standard Library**: Standard C++17 library (`<thread>`, `<mutex>`, `<condition_variable>`, `<chrono>`, `<random>`, `<cmath>`).
- **Operating System Socket APIs**: Windows Winsock2 (`ws2_32.lib`) and POSIX sockets (`<sys/socket.h>`, `<netinet/in.h>`).
- **Mathematical Formulations**:
  - 1D Steady-State Heat Conduction Governing Equation: Standard Fourier-Poisson differential equation.
  - Tridiagonal Matrix Algorithm (TDMA / Thomas Algorithm): Standard numerical algorithm for tridiagonal diagonally dominant linear systems.
  - Hyperbolic Tangent Derivative: Standard calculus identity $\frac{d}{dz}\tanh(z) = 1 - \tanh^2(z)$.
- **Synthetic Datasets**:
  - `sample_heatsink.stl`: Procedurally generated geometric model (CC0 1.0 Universal / Public Domain).
  - `ansys_nodal_temperature.csv`: Procedurally synthesized 3D nodal temperature dataset, explicitly labeled synthetic.

---

## 3. Candidate Declaration
I declare that the design choices, architectural implementation, code, derivations, and analysis presented in this submission represent original and authentic engineering work, with external references and AI assistance fully disclosed and verified.


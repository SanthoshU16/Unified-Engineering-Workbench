# Workbench Screenshots — Live Application Evidence

Captured from the running Unified Engineering Workbench (local + Render deployment).  
All views are from commit `7514553` on branch `main`.

| # | File | View | What it shows |
|---|------|------|---------------|
| 1 | `01_geometry_shaded_surface.jpg` | **Geometry — Shaded** | Heatsink STL (72 triangles, 100×100×50 mm) rendered in shaded-surface mode with validated part properties |
| 2 | `02_geometry_wireframe.jpg` | **Geometry — Wireframe** | Same heatsink model in wireframe display, confirming fin topology |
| 3 | `03_thermal_fea_temperature_profile.jpg` | **Thermal — 1D FEA Solver** | Temperature profile chart: continuous analytical curve vs. discrete FEA nodes (N=10, peak 188.2 °C, midpoint error 1.25 °C, nodal error 2.84e-14 °C) |
| 4 | `04_thermal_3d_rod_visualizer.jpg` | **Thermal — 3D Rod** | Solved rod mapped with Turbo colormap (20–188 °C), fine rings marking mesh elements |
| 5 | `05_thermal_3d_ansys_point_cloud.jpg` | **Thermal — ANSYS Point Cloud** | Synthetic ANSYS nodal temperature CSV (1,440 nodes, 55.6–134.9 °C) rendered as 3D point cloud with Turbo colormap and legend |
| 6 | `06_performance_lab_execution_time.jpg` | **Performance — Execution Time** | Benchmark: 2,000 cases, N=100 mesh, seed 42, 5 reps — fastest median 1.33 ms at 4 workers, peak speedup 3.92×, verification PASS (ΔT=0) |
| 7 | `07_performance_lab_speedup_chart.jpg` | **Performance — Speedup vs Ideal** | Measured vs. ideal linear speedup bar chart: 1W→1.00×, 2W→2.07×, 4W→3.92×; measurements table with all 5 repetitions visible |
| 8 | `08_hardware_workbench_block_diagram.jpg` | **Hardware — Block Diagram** | System topology diagram: Host PC ↔ STM32F401 (USB-CDC), Artix-7 FPGA (FTDI FIFO), JTAG Debug Adapter; 3 C++ mock adapters in simulation mode |

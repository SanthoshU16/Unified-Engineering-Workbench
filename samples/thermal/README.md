# Thermal FEA / ANSYS-Compatible Data & Provenance (Task B & C)

This directory contains nodal temperature datasets and export specifications for the Unified Engineering Workbench.

## Files

1. `ansys_nodal_temperature.csv`:
   - **Description**: 3D nodal temperature point cloud representing a synthetic steady-state thermal field on an electronics cooling heat sink shape.
   - **Provenance**: **SYNTHETIC DATASET** generated procedurally for this assessment. It was not produced by ANSYS and is not an authentic solver export.
   - **Schema**:
     - `node_id`: Integer, unique global node identifier.
     - `x_m`: Double, node X coordinate in meters ($m$).
     - `y_m`: Double, node Y coordinate in meters ($m$).
     - `z_m`: Double, node Z coordinate in meters ($m$).
     - `temperature_C`: Double, nodal temperature in degrees Celsius ($^\circ\text{C}$).
   - **Spatial Extent**: $X \in [-0.05, 0.05]\text{ m}$, $Y \in [-0.05, 0.05]\text{ m}$, $Z \in [0.0, 0.05]\text{ m}$.
   - **Temperature Extent**: Min: $55.62\ ^\circ\text{C}$, Max: $134.93\ ^\circ\text{C}$, Mean: $94.43\ ^\circ\text{C}$.
   - **Node Count**: 1,440 discrete finite-element mesh nodes.
   - **Export / interpretation notes**: UTF-8 comma-separated text with one header row and one row per node. Coordinates are metres and temperatures are degrees Celsius. The file contains points only; it has no element connectivity and is not registered to the separate STL or 1D rod model. The schema is ANSYS-compatible for demonstrating this viewer, but the values are synthetic.

2. `malformed_empty.csv`:
   - **Description**: Zero-byte CSV file to verify empty-file detection and explanatory error handling.

3. `malformed_corrupt.csv`:
   - **Description**: Corrupted CSV containing `NaN` floating point entries, incomplete rows, and missing column values.

## Supported Input Limits

- **Maximum File Size**: 50 MiB (enforced for file and browser uploads; parser rejects inputs over this cap)
- **Interactive Node Limit**: 200,000 nodes; larger point clouds are rejected with an actionable message. The byte cap is covered by an automated over-limit rejection check; full-size rendering throughput is not benchmarked.
- **Validation Rules**:
  - File must contain a header with `node_id`, `x_m`, `y_m`, `z_m`, `temperature_C`.
  - Coordinates and temperatures must be strictly finite real numbers (no `NaN`, `Inf`).
  - Node IDs must be unique positive integers.


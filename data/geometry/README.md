# Geometry Datasets & Provenance (Task B)

This directory contains Stereolithography (STL) surface geometry datasets for CAD visualization in the Unified Engineering Workbench.

## Files

1. `sample_heatsink.stl`:
   - **Description**: 5-fin extruded heat sink geometry with a base plate and 5 cooling fins.
   - **Format**: ASCII STL (`solid ... facet normal ... vertex ... endfacet ... endsolid`).
   - **Provenance**: Procedurally synthesized engineering benchmark model created for this assessment. Openly licensed under CC0 1.0 Universal (Public Domain).
   - **Units**: Meters ($m$). Dimensions: $0.100\text{ m} \times 0.100\text{ m} \times 0.050\text{ m}$ ($100\text{ mm} \times 100\text{ mm} \times 50\text{ mm}$).
   - **Triangles**: 72 triangular planar facets.
   - **Export Notes**: Planar normals are outward-facing and unit-normalized. Vertices obey standard right-hand rule winding.

2. `malformed_empty.stl`:
   - **Description**: Zero-byte file used to test negative validation, empty-file explanation, and graceful error reporting.

3. `malformed_corrupt.stl`:
   - **Description**: Corrupted ASCII STL containing non-finite coordinates (`NaN`, `Inf`) and premature EOF without required `endfacet`/`endsolid` closing tags.

## Supported Input Limits

- **Maximum File Size**: 150 MiB (enforced for file and browser uploads)
- **Interactive Triangle Limits**: ASCII and binary STL are parsed and validated in C++ and support up to 200,000 triangles in the interactive viewer.
- **Units**: STL does not store units. The sample coordinates are metres; for user files, select mm, m, or inches in the Geometry panel. Reported dimensions are converted to millimetres.
- **Validation Rules**:
  - File must start with `solid [name]`.
  - Normal coordinates and vertex coordinates must be finite real numbers.
  - Every `outer loop` must contain exactly 3 vertices.
  - Every facet must close with `endloop` and `endfacet`.
  - The model must terminate with `endsolid`.


#!/usr/bin/env python3
"""Render the compact six-page submission report in docs/REPORT.pdf."""
from pathlib import Path
import re

from reportlab import rl_config
from reportlab.lib import colors
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.units import mm
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, PageBreak

rl_config.invariant = 1

ROOT = Path(__file__).resolve().parent.parent
OUTPUT = ROOT / "docs" / "REPORT.pdf"
SOURCE_REPORT = ROOT / "docs" / "REPORT.md"
source_text = SOURCE_REPORT.read_text(encoding="utf-8")
candidate_match = re.search(r"\*\*Author:\*\* Candidate ID: \*([^*]+)\*", source_text)
effort_match = re.search(r"\*\*Actual effort:\*\* \*([^*]+)\*", source_text)
candidate_id = candidate_match.group(1).strip() if candidate_match else "Not recorded"
actual_effort = effort_match.group(1).strip() if effort_match else "Not recorded"
PAGE_W, PAGE_H = A4
NAVY = colors.HexColor("#243957")
PLUM = colors.HexColor("#493B5D")
MUTED = colors.HexColor("#586579")
LIGHT = colors.HexColor("#EEF1F5")

styles = getSampleStyleSheet()
styles.add(ParagraphStyle(name="ReportTitle", parent=styles["Title"], fontName="Helvetica-Bold", fontSize=19, leading=23, textColor=NAVY, alignment=0, spaceAfter=4))
styles.add(ParagraphStyle(name="ReportSub", parent=styles["Normal"], fontName="Helvetica", fontSize=8.6, leading=11, textColor=MUTED, spaceAfter=11))
styles.add(ParagraphStyle(name="Section", parent=styles["Heading1"], fontName="Helvetica-Bold", fontSize=14, leading=17, textColor=NAVY, spaceBefore=2, spaceAfter=7, keepWithNext=True))
styles.add(ParagraphStyle(name="Subsection", parent=styles["Heading2"], fontName="Helvetica-Bold", fontSize=10.3, leading=13, textColor=PLUM, spaceBefore=6, spaceAfter=3, keepWithNext=True))
styles.add(ParagraphStyle(name="BodySmall", parent=styles["BodyText"], fontName="Helvetica", fontSize=8.7, leading=11.4, textColor=colors.HexColor("#20242A"), spaceAfter=5))
styles.add(ParagraphStyle(name="TableHead", parent=styles["BodyText"], fontName="Helvetica-Bold", fontSize=7.7, leading=9.2, textColor=colors.white))
styles.add(ParagraphStyle(name="TableCell", parent=styles["BodyText"], fontName="Helvetica", fontSize=7.6, leading=9.2, textColor=colors.HexColor("#20242A")))
styles.add(ParagraphStyle(name="Callout", parent=styles["BodyText"], fontName="Helvetica-Bold", fontSize=8.8, leading=11.5, textColor=colors.HexColor("#743D2A"), backColor=colors.HexColor("#F8EEE8"), borderColor=colors.HexColor("#D9B8A7"), borderWidth=0.6, borderPadding=7, spaceBefore=3, spaceAfter=7))


def para(text, style="BodySmall"):
    return Paragraph(text, styles[style])


def make_table(rows, widths):
    data = [[Paragraph(str(value), styles["TableHead" if row == 0 else "TableCell"]) for value in values] for row, values in enumerate(rows)]
    result = Table(data, colWidths=widths, repeatRows=1, hAlign="LEFT")
    rules = [
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("LEFTPADDING", (0, 0), (-1, -1), 5), ("RIGHTPADDING", (0, 0), (-1, -1), 5),
        ("TOPPADDING", (0, 0), (-1, -1), 4), ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
        ("GRID", (0, 0), (-1, -1), 0.35, colors.HexColor("#D3D8E0")),
        ("BACKGROUND", (0, 0), (-1, 0), NAVY),
    ]
    for row in range(2, len(rows), 2):
        rules.append(("BACKGROUND", (0, row), (-1, row), LIGHT))
    result.setStyle(TableStyle(rules))
    return result


def page_chrome(canvas, doc):
    canvas.saveState()
    canvas.setStrokeColor(colors.HexColor("#D7DDE6"))
    canvas.setLineWidth(0.5)
    canvas.line(18 * mm, PAGE_H - 15 * mm, PAGE_W - 18 * mm, PAGE_H - 15 * mm)
    canvas.setFont("Helvetica-Bold", 7.5)
    canvas.setFillColor(MUTED)
    canvas.drawString(18 * mm, PAGE_H - 11.5 * mm, "UNIFIED ENGINEERING WORKBENCH  |  TECHNICAL REPORT")
    canvas.line(18 * mm, 13 * mm, PAGE_W - 18 * mm, 13 * mm)
    canvas.setFont("Helvetica", 7.5)
    canvas.drawString(18 * mm, 8.5 * mm, "PASS Science Works | R&D Internship Assessment 2026-2027")
    canvas.drawRightString(PAGE_W - 18 * mm, 8.5 * mm, f"{doc.page} / 6")
    canvas.restoreState()


story = []
# Page 1: executive summary and architecture.
story += [para("Unified Engineering Workbench", "ReportTitle"), para(f"PASS Science Works | R&D Internship Assessment 2026-2027 | Candidate ID: {candidate_id} | Actual effort: {actual_effort} | November 2026", "ReportSub")]
story += [para("Executive summary", "Section"), para("The Unified Engineering Workbench (UEW) combines geometry inspection, thermal analysis, multicore benchmarking, and embedded-device simulation in one browser interface. Numerical computation, file parsing, and concurrency are implemented in C++17. The interface is plain HTML, CSS, and JavaScript with no npm runtime dependencies. This report records the implemented engineering methods, checked-in measurements, tests, and limits.")]
story += [para("Delivery status", "Subsection"), para("The restructured source builds and its 14 backend checks pass on macOS / Apple silicon. The Windows x64 standalone package is not yet built or verified. Root-level Launch.exe and dist/windows-x64/workbench_server.exe must be produced and smoke-checked on Windows before this report can accompany a final submission.", "Callout")]
story += [para("Architecture and design choices", "Section"), make_table([
    ["Layer", "Implementation"],
    ["C++ core", "src/backend separates CAD and CSV parsers, 1D FEA, bounded thread pool, embedded mocks, backpropagation, system information, and HTTP transport."],
    ["Interface", "src/frontend provides Geometry, Thermal Analysis, Performance, and Hardware views using WebGL and Canvas without third-party browser packages."],
    ["Delivery", "CMake builds the backend. The Windows launcher starts the packaged server. Data, tests, results, docs, and packaging sources use separate top-level folders."],
], [29 * mm, 135 * mm]), Spacer(1, 6)]
story += [para("The HTTP server serves the static interface and REST endpoints. Request handlers complete independently of the accept loop; solver outputs are returned only after a solve completes. This provides safe completion for an in-flight operation without a cancellation control."), para("Source build prerequisites: CMake 3.20+ and a C++17 compiler. The standalone package is intended to require no first-run build, administrator rights, package installation, or internet access. Standalone Windows behavior remains unverified until the runtime is built on the target platform.")]
story.append(PageBreak())

# Page 2: CAD and imported data.
story += [para("Task B - CAD and thermal data visualization", "Section"), para("The application imports ASCII STL geometry and ANSYS-compatible nodal temperature CSV data. Native ANSYS project files and native CAD solids are outside scope. Parsing and validation occur in C++ before the browser renders the data."), make_table([
    ["Dataset", "Provenance and schema", "Observed sample"],
    ["Geometry", "Self-created CC0 1.0 ASCII STL. STL carries no units; sample coordinates are metres.", "sample_heatsink.stl: 72 triangles; dimensions 0.100 x 0.100 x 0.050 m."],
    ["Thermal point cloud", "Synthetic data, not an ANSYS solver export. UTF-8 CSV columns: node_id,x_m,y_m,z_m,temperature_C.", "ansys_nodal_temperature.csv: 1,440 nodes; 55.62 to 134.93 C."],
], [31 * mm, 75 * mm, 58 * mm]), para("Interaction and validation", "Subsection"), para("The STL viewer supports rotate, pan, zoom, fit-to-view, shaded mode, and wireframe. The CSV viewer renders temperature-colored 3D points with a Celsius legend and node inspection. Mesh-to-result registration is not required. Users can also select a local file."), para("The STL parser accepts ASCII and binary STL, validates triangle records, finite coordinates, and required ASCII terminators, and recomputes missing or zero-length normals. The CSV parser checks required columns, positive unique node IDs, finite numeric fields, and malformed rows. Empty files receive an explanatory error."), make_table([
    ["Input", "Documented enforced limits", "Negative coverage"],
    ["STL", "150 MiB; 200,000 triangles.", "Empty, malformed, non-finite, incomplete and over-limit inputs are rejected."],
    ["CSV", "50 MiB; 200,000 nodes.", "Empty, malformed, duplicate/non-positive IDs, non-finite and over-limit inputs are rejected."],
], [34 * mm, 60 * mm, 70 * mm]), Spacer(1, 6), para("Provenance notes and malformed fixtures are included under data/geometry and data/thermal. The 3D CSV point cloud contains no element connectivity and is not registered to the separate heat sink STL or 1D rod model.")]
story.append(PageBreak())

# Page 3: thermal FEA.
story += [para("Task C - 1D steady-state thermal FEA", "Section"), para("Model and assumptions", "Subsection"), para("A uniform rod of length L=1 m and area A=1e-4 m^2 has constant conductivity k=10 W/(m K), uniform volumetric heat generation q=10,000 W/m^3, prescribed end temperatures T(0)=100 C and T(L)=20 C, and insulated lateral surfaces. The governing equation is -d/dx(kA dT/dx)=qA. Material properties and cross-section are constant."), para("The solver uses two-node linear elements. Each element contributes Ke=(kA/h)[[1,-1],[-1,1]] and Fe=(qAh/2)[1,1]. Both prescribed temperatures are imposed in the assembled system. The tridiagonal system is solved with the O(N) Thomas algorithm."), para("Analytical verification and convergence", "Subsection"), para("The exact field is T_exact(x)=100-80x/L+[q/(2k)]x(L-x). With the specified values, the peak is 188.2 C near x=0.42 m. Linear interpolation is compared to the exact field at element midpoints, where the quadratic solution exposes the discretization error. The theoretical maximum midpoint error is q*h^2/(8k)=125*h^2 C."), make_table([
    ["Elements N", "h (m)", "Max midpoint error (C)", "Observed rate"],
    ["10", "0.1000", "1.25000000", "base"], ["20", "0.0500", "0.31250000", "2.0000"], ["40", "0.0250", "0.07812500", "2.0000"], ["80", "0.0125", "0.01953125", "2.0000"],
], [30 * mm, 32 * mm, 67 * mm, 35 * mm]), Spacer(1, 6), para("Nodal errors are near floating-point precision because this constant-load 1D case has nodal superconvergence. Measuring only at nodes would hide the true O(h^2) interpolation error. The report therefore checks element midpoints and exports the underlying mesh and convergence data to results/fea_convergence.csv. Labelled plots are in results/plots/."), para("Debugging insight", "Subsection"), para("An early node-only comparison appeared to show no mesh error. Comparing the FE midpoint interpolation against the exact quadratic field revealed the expected fourfold error reduction whenever the mesh width was halved. This distinction is essential for evaluating this problem correctly.")]
story.append(PageBreak())

# Page 4: benchmark.
story += [para("Task D - Multicore execution and benchmark", "Section"), para("The benchmark generates independent rods with a fixed mt19937_64 seed (42), varied positive k and q, and configurable case count and mesh size. The serial and parallel paths use identical input cases and numerical routines. The bounded C++ worker pool processes contiguous chunks and writes results at deterministic input indices."), para("Measurement setup", "Subsection"), para("Checked-in run: Apple M2, 8 physical / 8 logical cores, 16 GiB RAM, Apple Clang 21.0, -O3; 2,000 cases, N=100, seed 42. One untimed warm-up was performed for each worker count. Five measured repetitions were taken. Case generation, file I/O, rendering, and compilation are excluded; parallel call timing includes worker-pool startup and dispatch. Launch-to-ready is reported separately in the raw CSV (0.1 ms for the recorded run), from main entry to the server listening; OS process loading and browser loading are excluded."), make_table([
    ["Workers", "Five timings (ms)", "Median (ms)", "Speedup", "Efficiency"],
    ["1", "9.73, 5.57, 5.22, 4.70, 4.39", "5.22", "1.00x", "100.0%"],
    ["2", "2.73, 2.71, 2.52, 2.39, 2.36", "2.52", "2.07x", "103.6%"],
    ["4", "1.46, 1.40, 1.32, 1.33, 1.32", "1.33", "3.92x", "98.1%"],
], [18 * mm, 69 * mm, 26 * mm, 24 * mm, 27 * mm]), Spacer(1, 6), para("Serial/parallel maximum absolute temperature difference was 0 C, passing the 1e-8 C tolerance. The slightly above-100% two-worker efficiency comes from timing/cache variation in this short workload and is not treated as sustained superlinear scaling. Thread creation, dispatch, synchronization, and result allocation can dominate smaller jobs. Results vary with CPU load, power state, compiler, and hardware; no minimum speedup is assumed."), para("The raw repetitions and metadata are in results/benchmark_serial_parallel.csv. On a one-logical-CPU host, the parallel path is checked for correctness without claiming a measurable speedup.")]
story.append(PageBreak())

# Page 5: embedded-system design.
story += [para("Task E - Embedded-system integration design", "Section"), para("The design separates host transport, target compute/acceleration devices, and the dedicated debug probe. The C++ adapters are software mocks; they do not control physical hardware."), para("Host PC -> USB-CDC -> STM32F401 MCU (3.3 V logic)<br/>Host PC -> FTDI USB FIFO -> Artix-7 FPGA (1.0 V core, 1.8 V auxiliary, 3.3 V I/O)<br/>Host PC -> USB debug probe -> target SWD/JTAG (target-reference voltage sensed)"), make_table([
    ["Device", "Power / voltage assumptions", "Host link and signal role"],
    ["STM32F401", "USB 5 V to 3.3 V regulator; 3.3 V LVCMOS; common ground.", "USB-CDC UART, 115200 8N1. Target is SWD slave; adapter owns SWCLK/SWDIO signaling."],
    ["Artix-7", "5 V input; 1.0 V core, 1.8 V auxiliary, 3.3 V I/O banks; translation depends on bank and signal direction.", "FT2232H FIFO for telemetry. Target TAP is JTAG slave; probe drives TCK/TMS/TDI and samples TDO/VTREF."],
    ["JTAG probe", "USB-powered probe; senses target VTREF and uses compatible buffers. VTREF is not target power.", "Dedicated debug master, not an interchangeable compute board. Connector and protocol must match the board revision."],
], [29 * mm, 65 * mm, 70 * mm]), para("The pin-voltage table and labelled block diagram are in docs/EMBEDDED_SYSTEM_SPEC.md and the Hardware screenshot. Board connector and pin mappings are assumptions that must be verified against exact hardware revisions before wiring. The safe integration plan checks grounds, supplies, voltage domains, level translation, pin-one orientation, and signal ownership before connecting a link."), para("Mock behavior", "Subsection"), para("McuAdapter, FpgaAdapter, and JtagAdapter report capabilities, discover, connect/disconnect, and emit telemetry. A simulated 250 ms acknowledgement timeout transitions a connected adapter to ERROR_TIMEOUT; recovery resets simulated link state and returns to CONNECTED, after which telemetry resumes. No physical USB device, firmware, PCB, or FPGA programming is claimed.")]
story.append(PageBreak())

# Page 6: backpropagation, tests, and limitations.
story += [para("Task F - Backpropagation, tests, and limitations", "Section"), para("For x=1, y=2, z=wx+b, h=tanh(z), y_hat=vh+c, and J=0.5(y_hat-y)^2, the initial parameters are w=0.5, b=0, v=1, c=0, with learning rate 0.1. At step 0: z=0.5, h=0.46211716, y_hat=0.46211716, and J=1.18254182."), make_table([
    ["Parameter", "Analytical gradient", "Central FD gradient", "Absolute error"],
    ["w", "-1.20946448", "-1.20946448", "1.13e-10"], ["b", "-1.20946448", "-1.20946448", "1.13e-10"], ["v", "-0.71068205", "-0.71068205", "5.25e-11"], ["c", "-1.53788284", "-1.53788284", "1.12e-10"],
], [28 * mm, 43 * mm, 43 * mm, 50 * mm]), Spacer(1, 5), para("Finite differences use epsilon=1e-6 and all errors are below the 1e-5 tolerance. One simultaneous update from the original gradients gives w=0.62094645, b=0.12094645, v=1.07106820, c=0.15378828. Since the initial prediction underestimates the target and each parameter locally increases it, all four gradients are negative and gradient descent increases each parameter. Loss decreases monotonically from 1.18254182 to about 1.70e-20 by step 100. The two handwritten pages and training CSV/plot are included separately."), para("Verification", "Subsection"), para("The C++ suite has 14 checks covering numerical FEA, parser positive/negative cases, JSON upload decoding, deterministic batch execution and agreement, benchmark metadata, embedded timeout recovery, and analytical/finite-difference gradients. The restructured macOS build and CTest pass; frontend JavaScript syntax checks pass. Sample UI assets and both bundled data imports returned HTTP 200 / valid results in a local smoke check. Windows compilation and clean extracted-folder launch remain unverified."), para("Known limitations", "Subsection"), para("The solver is 1D with constant properties and cross-section. STL does not encode units. The point cloud is synthetic and has no element connectivity. Embedded devices are mocks only. Benchmark timings are machine-specific. The Windows zero-install gate is not complete until Launch.exe and the packaged server are tested on Windows 11 x64."), para("Evidence and disclosure", "Subsection"), para("Raw CSVs, labelled plots, test suite, sample provenance, screenshots, handwritten pages, AI-use log, ownership statement, and Git bundle are organized under results/, tests/, data/, and docs/. Candidate ID and actual effort hours must be supplied before packaging.")]

report = SimpleDocTemplate(str(OUTPUT), pagesize=A4, rightMargin=18 * mm, leftMargin=18 * mm, topMargin=21 * mm, bottomMargin=18 * mm, title="Unified Engineering Workbench - Technical Report", author=f"Candidate ID {candidate_id}")
report.build(story, onFirstPage=page_chrome, onLaterPages=page_chrome)
print(f"Wrote {OUTPUT}")

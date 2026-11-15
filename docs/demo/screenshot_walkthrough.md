# Timestamped Screenshot Walkthrough

**Candidate ID:** SanthoshU  
**Source recording:** [`demo.mov`](demo.mov), supplied by the candidate  
**Capture date:** 2026-11-15 (Asia/Kolkata)

The screenshots below are frames extracted from the supplied QuickTime recording. Their wall-clock timestamps use the recording's embedded creation time (`2026-11-15 09:23:06 UTC`, or `14:53:06 IST`) plus each frame's decoded presentation timestamp. The original recording is 72.875 seconds; it is included as supporting source evidence, not described as a 3–5 minute video.

| Timestamp (IST) | Screenshot | Observed screen state |
|---|---|---|
| 2026-11-15 14:53:06.000 | [Geometry shaded model](screenshots/01_145306_geometry.png) | Geometry view with the sample heat sink loaded. |
| 2026-11-15 14:53:14.000 | [Geometry wireframe](screenshots/02_145314_wireframe.png) | Wireframe mode selected for the loaded model. |
| 2026-11-15 14:53:22.000 | [Geometry detail](screenshots/03_145322_geometry_detail.png) | Closer shaded view of the heat-sink fins. |
| 2026-11-15 14:53:30.133 | [Thermal FEA results](screenshots/04_145330_thermal_fea.png) | Temperature profile and mesh/error summary are visible. |
| 2026-11-15 14:53:46.333 | [3D thermal visualization](screenshots/05_145346_thermal_3d.png) | The solved rod temperature field is rendered in 3D. |
| 2026-11-15 14:53:54.000 | [Performance setup](screenshots/06_145354_performance_setup.png) | Benchmark configuration is visible before execution. |
| 2026-11-15 14:53:58.000 | [Performance results](screenshots/07_145358_performance_results.png) | Completed 2,000-case run, worker timings, and numerical agreement status are visible. |
| 2026-11-15 14:54:10.000 | [Hardware workbench](screenshots/08_145410_hardware.png) | MCU, FPGA, JTAG adapter diagram and device panels are visible. |
| 2026-11-15 14:54:18.000 | [Geometry closing view](screenshots/09_145418_geometry_end.png) | The recording returns to the loaded geometry view. |

The performance frame shows a separate AMD EPYC 7R13 run (4 physical / 8 logical CPUs): medians of 2.58 ms, 7.92 ms, and 3.57 ms for 1, 2, and 4 workers, with numerical agreement marked PASS. This cloud-host measurement is distinct from the Apple M2 benchmark CSV and report; the two runs are not combined.

## Verification log

The separate local HTTP smoke check is recorded in [`verification.log`](verification.log). It was run against the macOS source build after the screen recording; it is not represented as console output captured during the recording. It records successful page/module responses, backend readiness, parsing of the 72-triangle sample STL, and parsing of the 1,440-node synthetic CSV.

```text
2026-11-15T16:35:21+05:30 GET / -> HTTP 200 HTML served
2026-11-15T16:35:21+05:30 GET /styles.css -> HTTP 200
2026-11-15T16:35:21+05:30 GET /src/app.js -> HTTP 200
2026-11-15T16:35:21+05:30 GET /api/system/info -> HTTP 200 status=READY port=18080
2026-11-15T16:35:21+05:30 POST /api/cad/stl/parse -> HTTP 200 valid=True triangles=72
2026-11-15T16:35:21+05:30 POST /api/cad/ansys/parse -> HTTP 200 valid=True nodes=1440
```

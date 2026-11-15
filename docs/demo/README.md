# Demo evidence capture

Save either a 3–5 minute video as `demo.mp4` in this folder, or a real timestamped screenshot sequence as `screenshot_walkthrough.md` with images in `screenshots/` and a log transcript in this folder.

## Windows 11 capture sequence

1. Start a PowerShell transcript and run `scripts/build_windows_runtime.ps1`. Keep the complete build output and SHA-256 values in the transcript.
2. Copy the project to a fresh folder, double-click `Launch.exe`, and confirm that the browser opens the workbench without a terminal, administrator prompt, first-run build, or internet connection.
3. Capture the Geometry, Thermal, Performance, and Hardware views, plus one input-validation result. Use the operating-system screenshot time when recording each image; do not fill timestamps in advance.
4. In PowerShell, record the running `workbench_server.exe` process and a successful local `/api/system/info` response. Include those outputs in the transcript.
5. Stop the transcript and write each screenshot's actual timestamp, filename, action, and observed result in `screenshot_walkthrough.md`.

Use an actual capture date and actual command output. The existing images under `docs/screenshots/` are helpful static illustrations, but they do not satisfy this evidence item by themselves.

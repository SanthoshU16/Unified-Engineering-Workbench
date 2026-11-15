# Version-Control Evidence & Development Log

**Repository:** `Unified-Engineering-Workbench`
**Branch:** `main`
**Audit date:** 2026-11-15

## Recent committed history at audit start

```text
26aa0b1 | 2026-11-15 | docs: add live Render deployment link and update handwritten evidence
f6639ab | 2026-11-15 | results: update benchmark CSV and speedup SVG plot to match live 2000-case/N=100 run data
a7a5fde | 2026-11-15 | docs(screenshots): add 8 live application screenshots covering all 4 workbench views
7514553 | 2026-11-15 | feat(ui): add Reset buttons for Performance Lab and Thermal FEA; improve empty-state chart UX
cbb7db3 | 2026-11-15 | perf: optimize batch solver memory and tune default benchmark workload for cloud containers
98bfe68 | 2026-11-15 | docs: replace ssh remote references with https across documentation
b5fcd08 | 2026-11-15 | feat(deploy): add multi-stage Dockerfile and render.yaml for Render deployment
5132661 | 2026-11-15 | docs: synchronize git log and update verified git bundle
```

The checked-in `docs/workbench_repo.bundle` is historical evidence. It contains old commit metadata and is excluded from the submission ZIP. The package process creates `docs/submission_history.bundle` from committed `main`, anonymizes author/committer email fields, strips any historical email addresses in text blobs, and excludes temporary refs. Inspect the packaged history with `git clone docs/submission_history.bundle <folder>` followed by `git log --oneline --decorate`.

## Final audit changes

This audit aligned the project to `src/`, `data/`, and `tests/`; updated CMake, local launchers, Docker paths, sample data routes, editor configuration, and package contents; made desktop HTTP binding loopback-only; added a native Windows launcher source and runtime build helper; synchronized the report with the checked-in benchmark CSV; and added a six-page report PDF, dependency record, and requirement audit.

## Final-check update (2026-11-15)

Candidate ID `SanthoshU` is confirmed and recorded in the report. At the candidate's direction, the requested self-report field is omitted; the submission audit records that rubric item as unmet. The six-page report was regenerated and visually checked after this metadata update. The Windows runtime is absent and the supplied demo clip is below the required duration, so the submission archive has not been produced.

The candidate later supplied `docs/demo/demo.mov`. Its QuickTime metadata contains a video track and reports 72.875 seconds, below the video length requirement. Nine actual timestamped frames and a separate local HTTP verification log were added as the screenshot walkthrough alternative. The README architecture tree now includes the omitted configuration, evidence, and helper files, and the package preflight can inspect MP4/MOV containers or validate the screenshot alternative without FFmpeg.

## Repository and package state

- At final-check start, local `main` was ahead of the stored `origin/main` tracking ref by two commits (`f2a6534`, `709a5bb`). This final-check update is recorded in the following commit; no push was made.
- A live remote check was unavailable during the audit because the host could not resolve GitHub.
- The sanitized archive bundle is generated from committed `main` at packaging time. Final audit changes must be committed first so the archive history reflects them. The packaging helper anonymizes author/committer email metadata and email addresses in historical text blobs; the older tracked bundle is excluded from the ZIP because it predates this redaction.
- No submission ZIP was created. `scripts/package_submission.sh` stops until the Windows runtime and required demonstration evidence are present; the Candidate ID is now confirmed.

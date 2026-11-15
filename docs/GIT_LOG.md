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

This audit aligned the project to `src/`, `data/`, and `tests/`; updated CMake, local launchers, Docker paths, sample data routes, editor configuration, and package contents; made desktop HTTP binding loopback-only; added a native Windows launcher source and runtime build helper; synchronized the report with the checked-in benchmark CSV; and added a six-page report PDF, dependency record, and requirement audit. These changes are prepared in the working tree and have not been pushed.

## Repository and package state

- Local `HEAD` and the stored `origin/main` tracking ref matched at `26aa0b1` when checked.
- A live remote check was unavailable during the audit because the host could not resolve GitHub.
- The sanitized archive bundle is generated from committed `main` at packaging time. Final audit changes must be committed first so the archive history reflects them. The packaging helper anonymizes author/committer email metadata and email addresses in historical text blobs; the older tracked bundle is excluded from the ZIP because it predates this redaction.
- No submission ZIP was created. `scripts/package_submission.sh` now stops until the candidate ID and both Windows runtime files are present.

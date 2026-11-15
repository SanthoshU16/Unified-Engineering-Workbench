#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$SCRIPT_DIR"

CANDIDATE_ID="${1:-CandidateID}"
ARCHIVE_NAME="${CANDIDATE_ID}_Workbench.zip"

echo "========================================================"
echo "    Packaging Submission Archive: ${ARCHIVE_NAME}       "
echo "========================================================"

# Keep submitted results and report in sync. Regenerate explicitly with
# REGENERATE_RESULTS=1 when the report is updated for a new measurement.
if [ "${REGENERATE_RESULTS:-0}" = "1" ]; then
    ./scripts/run_benchmarks.sh
fi

# Preserve actual project history when a committed repository is available.
ZIP_EXCLUDES=("build/*" ".git/*" "*.DS_Store" "__pycache__/*")
if git rev-parse --verify HEAD >/dev/null 2>&1; then
    echo "Creating Git bundle for version-control evidence..."
    git bundle create docs/workbench_repo.bundle --all
else
    echo "[Submission gap] No committed Git history is available; excluding the stale checkpoint bundle."
    ZIP_EXCLUDES+=("docs/workbench_repo.bundle")
fi

if [ -f "dist/windows-x64/Launch.exe" ] && [ -f "dist/windows-x64/workbench_server.exe" ]; then
    echo "Including the Windows x64 packaged runtime..."
else
    echo "[Submission gap] Windows runtime missing: add dist/windows-x64/Launch.exe and workbench_server.exe."
fi

# Remove old zip if present
rm -f "$ARCHIVE_NAME"

# Zip required components using relative paths
PACKAGE_ITEMS=(
    Launch.bat \
    launch.sh \
    CMakeLists.txt \
    README.md \
    .gitignore \
    .vscode/ \
    backend/ \
    frontend/ \
    samples/ \
    results/ \
    docs/ \
    scripts/
)

if [ -d "dist/windows-x64" ]; then
    PACKAGE_ITEMS+=(dist/windows-x64/)
fi

zip -r "$ARCHIVE_NAME" "${PACKAGE_ITEMS[@]}" -x "${ZIP_EXCLUDES[@]}"

echo ""
echo "Archive created successfully: ${ARCHIVE_NAME}"
ls -lh "$ARCHIVE_NAME"


#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$SCRIPT_DIR"

if [ -z "${1:-}" ]; then
    echo "Usage: $0 <CandidateID>"
    exit 2
fi

CANDIDATE_ID="$1"
candidate_id_pattern='^[A-Za-z0-9 ._&-]+$'
if [[ ! "$CANDIDATE_ID" =~ $candidate_id_pattern ]]; then
    echo "CandidateID may contain only letters, numbers, spaces, dots, underscores, ampersands, and hyphens."
    exit 2
fi
ARCHIVE_NAME="${CANDIDATE_ID}_Workbench.zip"

echo "========================================================"
echo "    Packaging Submission Archive: ${ARCHIVE_NAME}       "
echo "========================================================"

if [ -e "$ARCHIVE_NAME" ]; then
    echo "[Packaging stopped] Archive already exists: $ARCHIVE_NAME"
    exit 1
fi

if grep -Eiq 'Candidate ID:.*pending' docs/REPORT.md; then
    echo "[Submission blocked] Fill the candidate ID in docs/REPORT.md before packaging."
    exit 1
fi

REPORT_CANDIDATE="$(sed -n 's/^\*\*Author:\*\* Candidate ID: \*\([^*]*\)\*.*/\1/p' docs/REPORT.md)"
if [ "$REPORT_CANDIDATE" != "$CANDIDATE_ID" ]; then
    echo "[Submission blocked] Archive CandidateID '$CANDIDATE_ID' does not match docs/REPORT.md Candidate ID '$REPORT_CANDIDATE'."
    exit 1
fi
VIDEO_FILE=""
if [ -f "docs/demo/demo.mp4" ]; then
    VIDEO_FILE="docs/demo/demo.mp4"
elif [ -f "docs/demo/demo.mov" ]; then
    VIDEO_FILE="docs/demo/demo.mov"
fi

VIDEO_VALID=0
VIDEO_SECONDS=""
if [ -n "$VIDEO_FILE" ]; then
    if [ ! -s "$VIDEO_FILE" ]; then
        echo "[Submission blocked] $VIDEO_FILE is empty; replace or remove the invalid video file."
        exit 1
    fi
    if ! VIDEO_SECONDS="$(python3 scripts/probe_movie.py "$VIDEO_FILE")"; then
        echo "[Submission blocked] $VIDEO_FILE must be a readable MP4 or QuickTime video with a video track."
        exit 1
    fi
    if awk -v duration="$VIDEO_SECONDS" 'BEGIN { exit !(duration >= 180 && duration <= 300) }'; then
        VIDEO_VALID=1
    fi
fi

WALKTHROUGH_VALID=0
if [ -s "docs/demo/screenshot_walkthrough.md" ] && [ -s "docs/demo/verification.log" ] && [ -d "docs/demo/screenshots" ]; then
    SCREENSHOT_COUNT="$(find docs/demo/screenshots -maxdepth 1 -type f -name '*.png' | wc -l | tr -d '[:space:]')"
    if [[ "$SCREENSHOT_COUNT" =~ ^[0-9]+$ ]] && [ "$SCREENSHOT_COUNT" -ge 4 ] && grep -Eq '(^|[^0-9])[0-9]{1,2}:[0-5][0-9](:[0-5][0-9])?([^0-9]|$)' docs/demo/screenshot_walkthrough.md && grep -Eiq 'log|command|HTTP|output|response' docs/demo/screenshot_walkthrough.md; then
        WALKTHROUGH_VALID=1
    fi
fi

if [ "$VIDEO_VALID" -ne 1 ] && [ "$WALKTHROUGH_VALID" -ne 1 ]; then
    if [ -n "$VIDEO_SECONDS" ]; then
        echo "[Submission blocked] $VIDEO_FILE is ${VIDEO_SECONDS} seconds; provide a 180-300 second video or a timestamped screenshot walkthrough with logs."
    elif [ -n "$VIDEO_FILE" ]; then
        echo "[Submission blocked] Add a valid 3-5 minute MP4/MOV or a timestamped screenshot walkthrough with logs."
    else
        echo "[Submission blocked] Add a 3-5 minute demo at docs/demo/demo.mp4 or docs/demo/demo.mov, or a timestamped walkthrough with logs at docs/demo/screenshot_walkthrough.md."
    fi
    exit 1
fi

if [ "$VIDEO_VALID" -eq 1 ]; then
    echo "Using the verified 3-5 minute demo video."
else
    echo "Using the timestamped screenshot walkthrough with logs ($SCREENSHOT_COUNT screenshots)."
    if [ -n "$VIDEO_FILE" ]; then
        echo "The shorter demo video is included as supporting evidence only (${VIDEO_SECONDS} seconds)."
    fi
fi

if [ -f "Launch.exe" ] && [ -f "dist/windows-x64/workbench_server.exe" ]; then
    echo "Including the Windows x64 packaged runtime..."
else
    echo "[Submission blocked] Build and verify root Launch.exe and dist/windows-x64/workbench_server.exe on Windows first."
    exit 1
fi

python3 scripts/render_report.py
if ! command -v pdfinfo >/dev/null 2>&1; then
    echo "[Submission blocked] Install Poppler so pdfinfo can verify the report page limit."
    exit 1
fi
REPORT_PAGES="$(pdfinfo docs/REPORT.pdf | awk '/^Pages:/ {print $2}')"
if [[ ! "$REPORT_PAGES" =~ ^[0-9]+$ ]] || [ "$REPORT_PAGES" -gt 6 ]; then
    echo "[Submission blocked] docs/REPORT.pdf must contain six pages or fewer."
    exit 1
fi

if [ -n "$(git status --porcelain)" ]; then
    echo "[Submission blocked] Commit the reviewed source, docs, and generated report before packaging the Git history."
    exit 1
fi

echo "Creating anonymized main-branch Git history bundle..."
python3 scripts/create_sanitized_bundle.py

ZIP_EXCLUDES=("build/*" "build-windows-x64/*" ".git/*" "docs/workbench_repo.bundle" "*.DS_Store" "__pycache__/*")

# Zip required components using relative paths
PACKAGE_ITEMS=(
    Launch.exe \
    Launch.bat \
    launch.sh \
    CMakeLists.txt \
    dependencies.lock \
    README.md \
    .gitignore \
    Dockerfile \
    render.yaml \
    .vscode/ \
    src/ \
    data/ \
    tests/ \
    results/ \
    docs/ \
    scripts/ \
    packaging/ \
    dist/windows-x64/
)

zip -r "$ARCHIVE_NAME" "${PACKAGE_ITEMS[@]}" -x "${ZIP_EXCLUDES[@]}"

echo ""
echo "Archive created successfully: ${ARCHIVE_NAME}"
ls -lh "$ARCHIVE_NAME"


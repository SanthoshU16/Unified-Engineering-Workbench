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

if [ -f "Launch.exe" ] && [ -f "dist/windows-x64/workbench_server.exe" ]; then
    echo "Including the Windows x64 packaged runtime..."
else
    echo "[Submission blocked] Build and verify root Launch.exe and dist/windows-x64/workbench_server.exe on Windows first."
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
if [ -f "docs/demo/demo.mp4" ]; then
    if [ ! -s "docs/demo/demo.mp4" ] || ! command -v ffprobe >/dev/null 2>&1; then
        echo "[Submission blocked] Install ffprobe and provide a readable 3-5 minute video at docs/demo/demo.mp4, or provide the timestamped screenshot walkthrough."
        exit 1
    fi
    VIDEO_SECONDS="$(ffprobe -v error -select_streams v:0 -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 docs/demo/demo.mp4)"
    VIDEO_STREAM="$(ffprobe -v error -select_streams v:0 -show_entries stream=codec_type -of csv=p=0 docs/demo/demo.mp4)"
    if [ "$VIDEO_STREAM" != "video" ] || ! awk -v duration="$VIDEO_SECONDS" 'BEGIN { exit !(duration >= 180 && duration <= 300) }'; then
        echo "[Submission blocked] docs/demo/demo.mp4 must contain a 3-5 minute video stream."
        exit 1
    fi
elif [ -s "docs/demo/screenshot_walkthrough.md" ]; then
    if ! grep -Eq '(^|[^0-9])[0-9]{1,2}:[0-5][0-9](:[0-5][0-9])?([^0-9]|$)' docs/demo/screenshot_walkthrough.md || ! grep -Eiq 'log|command|HTTP|output|response' docs/demo/screenshot_walkthrough.md; then
        echo "[Submission blocked] The screenshot walkthrough must include timestamps and logs."
        exit 1
    fi
else
    echo "[Submission blocked] Add a 3-5 minute demo at docs/demo/demo.mp4 or a timestamped walkthrough with logs at docs/demo/screenshot_walkthrough.md."
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


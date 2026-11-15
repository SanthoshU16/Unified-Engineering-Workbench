#!/usr/bin/env python3
"""Create a main-branch Git bundle with commit identities anonymized."""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
OUTPUT = ROOT / "docs" / "submission_history.bundle"
REF = "refs/heads/main"
EMAIL = re.compile(rb"[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}")
IDENTITY = re.compile(rb"^(author|committer|tagger) .+ <[^>]+> ([0-9]+ [+-][0-9]{4})$")
PLACEHOLDER = b"candidate@local.invalid"


def run(args, *, cwd=None, input_data=None, capture=False):
    return subprocess.run(
        args,
        cwd=cwd,
        input=input_data,
        check=True,
        stdout=subprocess.PIPE if capture else None,
        stderr=subprocess.PIPE if capture else None,
    )


def sanitize_export(stream: bytes) -> tuple[bytes, int]:
    output = bytearray()
    cursor = 0
    redactions = 0
    while cursor < len(stream):
        end = stream.find(b"\n", cursor)
        if end < 0:
            output.extend(stream[cursor:])
            break
        line = stream[cursor:end]
        newline = stream[end : end + 1]
        cursor = end + 1

        identity = IDENTITY.fullmatch(line)
        if identity:
            line = identity.group(1) + b" Candidate <" + PLACEHOLDER + b"> " + identity.group(2)
            redactions += 1
            output.extend(line + newline)
            continue

        if line.startswith(b"data "):
            try:
                size = int(line[5:])
            except ValueError:
                output.extend(line + newline)
                continue
            payload = stream[cursor : cursor + size]
            if len(payload) != size:
                raise ValueError("Git fast-export data block ended unexpectedly")
            cursor += size
            has_terminal_newline = payload.endswith(b"\n")
            if not has_terminal_newline:
                if stream[cursor : cursor + 1] != b"\n":
                    raise ValueError("Git fast-export data block has no separator")
                cursor += 1
            try:
                payload.decode("utf-8")
            except UnicodeDecodeError:
                pass
            else:
                payload, count = EMAIL.subn(PLACEHOLDER, payload)
                redactions += count
            output.extend(b"data " + str(len(payload)).encode("ascii") + b"\n")
            output.extend(payload)
            if not payload.endswith(b"\n"):
                output.extend(b"\n")
            continue

        output.extend(line + newline)
    return bytes(output), redactions


def assert_no_unexpected_emails(repo: Path) -> None:
    objects = run(["git", "rev-list", "--objects", REF], cwd=repo, capture=True).stdout.splitlines()
    ids = [line.split(b" ", 1)[0] for line in objects]
    if not ids:
        raise RuntimeError("Sanitized branch contains no objects")
    batch = run(["git", "cat-file", "--batch"], cwd=repo, input_data=b"\n".join(ids) + b"\n", capture=True).stdout
    cursor = 0
    for _ in ids:
        end = batch.find(b"\n", cursor)
        if end < 0:
            raise RuntimeError("Could not parse Git object batch")
        size = int(batch[cursor:end].split()[-1])
        cursor = end + 1
        body = batch[cursor : cursor + size]
        cursor += size + 1
        for address in EMAIL.findall(body):
            if address != PLACEHOLDER:
                raise RuntimeError("A non-placeholder email address remains in the sanitized history")


def main() -> None:
    if not (ROOT / ".git").exists():
        raise SystemExit("A Git worktree is required to create the history bundle")
    exported = run(["git", "fast-export", "--signed-tags=strip", REF], cwd=ROOT, capture=True).stdout
    sanitized, redactions = sanitize_export(exported)

    with tempfile.TemporaryDirectory(prefix="uew-history-") as temp_name:
        temp_repo = Path(temp_name) / "history.git"
        run(["git", "init", "--bare", str(temp_repo)], capture=True)
        run(["git", "fast-import", "--quiet"], cwd=temp_repo, input_data=sanitized, capture=True)
        refs = run(["git", "show-ref", "--verify", REF], cwd=temp_repo, capture=True).stdout
        if not refs:
            raise RuntimeError("The sanitized import did not create refs/heads/main")
        assert_no_unexpected_emails(temp_repo)
        temp_bundle = Path(temp_name) / "submission_history.bundle"
        run(["git", "bundle", "create", str(temp_bundle), REF], cwd=temp_repo, capture=True)
        run(["git", "bundle", "verify", str(temp_bundle)], cwd=temp_repo, capture=True)
        OUTPUT.write_bytes(temp_bundle.read_bytes())

    print(f"Wrote {OUTPUT.relative_to(ROOT)} with anonymized commit identities; {redactions} email fields redacted.")


if __name__ == "__main__":
    main()

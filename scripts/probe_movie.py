#!/usr/bin/env python3
"""Read video-track presence and duration from an MP4/QuickTime movie."""
import struct
import sys
from pathlib import Path


def boxes(data: bytes, start: int, end: int):
    position = start
    while position + 8 <= end:
        size, kind = struct.unpack_from(">I4s", data, position)
        header_size = 8
        if size == 1:
            if position + 16 > end:
                raise ValueError("truncated extended-size atom")
            size = struct.unpack_from(">Q", data, position + 8)[0]
            header_size = 16
        elif size == 0:
            size = end - position
        if size < header_size or position + size > end:
            raise ValueError(f"invalid {kind!r} atom size")
        yield kind, position + header_size, position + size
        position += size


def movie_duration_seconds(path: Path) -> float:
    data = path.read_bytes()
    top_level = list(boxes(data, 0, len(data)))
    moov = next(((start, end) for kind, start, end in top_level if kind == b"moov"), None)
    if moov is None:
        raise ValueError("movie metadata atom is missing")

    moov_start, moov_end = moov
    movie_header = next(
        ((start, end) for kind, start, end in boxes(data, moov_start, moov_end) if kind == b"mvhd"),
        None,
    )
    if movie_header is None:
        raise ValueError("movie duration metadata is missing")

    header_start, header_end = movie_header
    version = data[header_start]
    if version == 0 and header_end - header_start >= 20:
        timescale = struct.unpack_from(">I", data, header_start + 12)[0]
        duration = struct.unpack_from(">I", data, header_start + 16)[0]
    elif version == 1 and header_end - header_start >= 32:
        timescale = struct.unpack_from(">I", data, header_start + 20)[0]
        duration = struct.unpack_from(">Q", data, header_start + 24)[0]
    else:
        raise ValueError("unsupported or truncated movie header")
    if timescale == 0 or duration == 0:
        raise ValueError("movie duration is invalid")

    has_video = False
    for kind, track_start, track_end in boxes(data, moov_start, moov_end):
        if kind != b"trak":
            continue
        for child, media_start, media_end in boxes(data, track_start, track_end):
            if child != b"mdia":
                continue
            for media_child, handler_start, handler_end in boxes(data, media_start, media_end):
                if media_child == b"hdlr" and handler_end - handler_start >= 12:
                    has_video |= data[handler_start + 8 : handler_start + 12] == b"vide"
    if not has_video:
        raise ValueError("movie contains no video track")
    return duration / timescale


def main() -> int:
    if len(sys.argv) != 2:
        print("Usage: probe_movie.py <movie.mp4|movie.mov>", file=sys.stderr)
        return 2
    try:
        seconds = movie_duration_seconds(Path(sys.argv[1]))
    except (OSError, ValueError, struct.error) as error:
        print(f"Movie inspection failed: {error}", file=sys.stderr)
        return 1
    print(f"{seconds:.3f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

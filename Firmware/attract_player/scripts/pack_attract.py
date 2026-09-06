#!/usr/bin/env python3
"""Pack preview attract MP4 into SCMJ (simple concatenated MJPEG) for the badge player."""

from __future__ import annotations

import argparse
import shutil
import struct
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SRC = ROOT.parent.parent / "preview" / "saintcon2025_attract.mp4"
DEFAULT_OUT = ROOT / "spiffs" / "attract.scmj"


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", type=Path, default=DEFAULT_SRC)
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT)
    ap.add_argument("--fps", type=int, default=12)
    ap.add_argument("--q", type=int, default=10, help="ffmpeg mjpeg quality (2=best, 31=worst)")
    ap.add_argument("--w", type=int, default=240)
    ap.add_argument("--h", type=int, default=320)
    args = ap.parse_args()

    tmp = Path(tempfile.mkdtemp(prefix="scmj_"))
    try:
        subprocess.check_call(
            [
                "ffmpeg",
                "-y",
                "-i",
                str(args.src),
                "-an",
                "-vf",
                f"scale={args.w}:{args.h}",
                "-r",
                str(args.fps),
                "-q:v",
                str(args.q),
                str(tmp / "f%04d.jpg"),
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        frames = sorted(tmp.glob("f*.jpg"))
        if not frames:
            raise SystemExit("no frames extracted")
        args.out.parent.mkdir(parents=True, exist_ok=True)
        with args.out.open("wb") as f:
            f.write(b"SCMJ")
            f.write(struct.pack("<HHHI", args.w, args.h, args.fps, len(frames)))
            for p in frames:
                data = p.read_bytes()
                f.write(struct.pack("<I", len(data)))
                f.write(data)
        print(f"wrote {args.out} frames={len(frames)} size={args.out.stat().st_size}")
    finally:
        shutil.rmtree(tmp)


if __name__ == "__main__":
    main()

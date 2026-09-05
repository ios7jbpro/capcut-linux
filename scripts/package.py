#!/usr/bin/env python3
"""Package supplied shim binaries with portable docs, source, and checksums."""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]
NAME = "CapCut-Wine-Compat-v0.4"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary_dir", type=Path, help="directory with version.dll and libcapcut_xcopy.so")
    parser.add_argument("--output", type=Path, default=ROOT / "packages" / (NAME + ".zip"))
    args = parser.parse_args()
    files = {}
    for name in ("version.dll", "libcapcut_xcopy.so"):
        path = args.binary_dir / name
        if not path.is_file():
            parser.error(f"missing binary: {path}")
        files[name] = (path.read_bytes(), 0o644)
    for name in ("README.md", "build.sh"):
        path = ROOT / name
        files[name] = (path.read_bytes(), path.stat().st_mode & 0o777)
    for directory in ("src", "scripts", "docs", "extras"):
        for path in sorted((ROOT / directory).rglob("*")):
            if not path.is_file() or "build" in path.relative_to(ROOT).parts or "__pycache__" in path.parts:
                continue
            files[path.relative_to(ROOT).as_posix()] = (path.read_bytes(), path.stat().st_mode & 0o777)
    manifest = {
        "version": "0.4-experimental",
        "capcut": "1.5.0.230",
        "architecture": "x86_64",
        "windows_proxy_version": "0.1",
        "tested_runner": "GE-Proton11-6",
        "tested_dxvk": "3.1",
        "original_VECreator_sha256": "c94175f5348a68d506a5f73d5b2249ce4ebc715c83bd16886848d5ebfae0d64d",
        "artifacts": {name: hashlib.sha256(files[name][0]).hexdigest()
                      for name in ("version.dll", "libcapcut_xcopy.so")},
    }
    files["manifest.json"] = ((json.dumps(manifest, indent=2) + "\n").encode(), 0o644)
    sums = "".join(f"{hashlib.sha256(data).hexdigest()}  {name}\n"
                   for name, (data, _) in sorted(files.items()))
    files["SHA256SUMS"] = (sums.encode(), 0o644)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.output, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for name, (data, mode) in sorted(files.items()):
            info = zipfile.ZipInfo(f"{NAME}/{name}", date_time=(2026, 9, 5, 0, 0, 0))
            info.create_system = 3
            info.external_attr = (0o100000 | mode) << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, data)
    checksum = hashlib.sha256(args.output.read_bytes()).hexdigest()
    args.output.with_suffix(".zip.sha256").write_text(f"{checksum}  {args.output.name}\n")
    print(f"Packaged {len(files)} files: {args.output}")
    print(f"SHA-256: {checksum}")


if __name__ == "__main__":
    main()

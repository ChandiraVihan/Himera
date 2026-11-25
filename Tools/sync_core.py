#!/usr/bin/env python3
"""Copy or verify the vendored HIMERA core against a local AI-repo checkout.

  python Tools/sync_core.py <ai-repo>           copy files, rewrite VENDORED_FROM.txt
  python Tools/sync_core.py --check <ai-repo>   exit 1 if any vendored file differs
"""
import argparse, filecmp, shutil, subprocess, sys
from pathlib import Path

CORE = Path(__file__).resolve().parent.parent / "Plugins/HimeraAI/Source/HimeraAI/Core"
GLOBS = [("include/himera", "*.hpp"), ("src", "*.cpp")]


def commit_of(repo: Path) -> str:
    out = subprocess.run(["git", "-C", str(repo), "rev-parse", "HEAD"],
                         capture_output=True, text=True)
    return out.stdout.strip() if out.returncode == 0 else "unknown"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("repo", type=Path)
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()

    drift = []
    for sub, pattern in GLOBS:
        for src in sorted((args.repo / sub).glob(pattern)):
            dst = CORE / sub / src.name
            same = dst.exists() and filecmp.cmp(src, dst, shallow=False)
            if same:
                continue
            drift.append(f"{sub}/{src.name}")
            if not args.check:
                dst.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(src, dst)

    if args.check:
        if drift:
            print("DRIFT:", *drift, sep="\n  ")
            return 1
        print("Vendored core matches", args.repo)
        return 0

    (CORE / "VENDORED_FROM.txt").write_text(
        f"repo: HIMERA-Autonomous-AI\ncommit: {commit_of(args.repo)}\n")
    print("Updated:", *(drift or ["(nothing changed)"]), sep="\n  ")
    return 0


if __name__ == "__main__":
    sys.exit(main())

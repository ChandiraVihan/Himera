#!/usr/bin/env python3
"""CTest wrapper for Tools/sync_core.py --check.

Set HIMERA_AI_REPO to a local checkout of HIMERA-Autonomous-AI to run it.
Without it the test is SKIPPED (exit code 77), not failed.
"""
import os, subprocess, sys
from pathlib import Path

repo = os.environ.get("HIMERA_AI_REPO", "").strip()
if not repo:
    print("SKIP: set HIMERA_AI_REPO to the AI repo path to check for drift")
    sys.exit(77)
if not Path(repo).is_dir():
    print(f"FAIL: HIMERA_AI_REPO does not exist: {repo}")
    sys.exit(1)
script = Path(__file__).resolve().parent.parent / "sync_core.py"
sys.exit(subprocess.run([sys.executable, str(script), "--check", repo]).returncode)

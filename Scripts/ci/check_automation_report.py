"""Falla (código 1) si el informe de Automation tiene tests en rojo o se han ejecutado menos de --min-tests.

    uv run python Scripts/ci/check_automation_report.py Saved/Automation/CI/index.json --min-tests 87

El informe es el index.json que deja UnrealEditor-Cmd con -ReportExportPath (Scripts/ci/ci_local.bat).
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def summarize(report: dict) -> tuple[int, int, list[str]]:
    """Tests ejecutados, cuántos en rojo y las rutas de los que fallan."""
    failed = int(report.get("failed", 0))
    total = int(report.get("succeeded", 0)) + int(report.get("succeededWithWarnings", 0)) + failed
    paths = [t.get("fullTestPath", "?") for t in report.get("tests", []) if t.get("state") == "Fail"]
    return total, failed, paths


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path)
    parser.add_argument("--min-tests", type=int, default=87)
    args = parser.parse_args(argv)
    if not args.report.exists():
        print(f"sin informe: {args.report}")
        return 1
    total, failed, paths = summarize(json.loads(args.report.read_text(encoding="utf-8-sig")))
    for path in paths:
        print(f"FALLA {path}")
    print(f"{total} tests, {failed} en rojo (mínimo {args.min_tests})")
    return 0 if failed == 0 and total >= args.min_tests else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

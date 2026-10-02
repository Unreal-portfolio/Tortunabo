"""Lectura del informe de Automation (index.json de -ReportExportPath) para la CI local."""

from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Scripts" / "ci"))

from check_automation_report import main, summarize  # noqa: E402

REPORT = {
    "succeeded": 3,
    "succeededWithWarnings": 1,
    "failed": 1,
    "notRun": 0,
    "tests": [
        {"fullTestPath": "Tortunabo.Coop.Bake", "state": "Success"},
        {"fullTestPath": "Tortunabo.Coop.Watch", "state": "Fail"},
    ],
}


def test_summarize_counts_and_lists_failures():
    total, failed, paths = summarize(REPORT)
    assert (total, failed, paths) == (5, 1, ["Tortunabo.Coop.Watch"])


def test_main_fails_with_a_failed_test(tmp_path):
    path = tmp_path / "index.json"
    path.write_text(json.dumps(REPORT), encoding="utf-8-sig")
    assert main([str(path), "--min-tests", "1"]) == 1


def test_main_fails_when_too_few_tests_ran(tmp_path):
    path = tmp_path / "index.json"
    path.write_text(json.dumps({**REPORT, "failed": 0, "tests": []}), encoding="utf-8")
    assert main([str(path), "--min-tests", "87"]) == 1


def test_main_passes_when_green(tmp_path):
    path = tmp_path / "index.json"
    path.write_text(json.dumps({**REPORT, "failed": 0, "tests": []}), encoding="utf-8")
    assert main([str(path), "--min-tests", "4"]) == 0


def test_main_fails_without_report(tmp_path):
    assert main([str(tmp_path / "no-existe.json")]) == 1

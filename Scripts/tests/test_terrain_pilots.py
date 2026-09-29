"""Los tres pilotos del catalogo de mapas (Docs/Catalogo-Mapas-2026-09-29.md, lote 0): L10_japon_fuji (Rally, geo),
I01_filipinas_rara (TcT, isla) y A01_diana (TcT, arena). Estan en index.json, pasan los validadores sobre los
TNTM2 guardados, tienen CREDITS y lamina, y se regeneran igual (mismo trozo byte a byte).

    uv run --with pytest --with numpy --with scipy --with pillow --with scikit-image --with pyproj \
        python -m pytest Scripts/tests/test_terrain_pilots.py
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from terrain_vol.export import write_chunk  # noqa: E402
from terrain_vol.mesh import build_chunk  # noqa: E402
from terrain_vol.validate import check_budget, check_seams  # noqa: E402

SCRIPTS = Path(__file__).resolve().parent.parent
VARIANTS = SCRIPTS / "terrain_volumes" / "Variants"
SHEETS = SCRIPTS.parent / "Docs" / "Mapas"
PILOTS = {"L10_japon_fuji": ("rally", 1_200_000), "I01_filipinas_rara": ("tct", 350_000), "A01_diana": ("tct", 350_000)}


def index() -> dict[str, dict]:
    return {e["name"]: e for e in json.loads((VARIANTS / "index.json").read_text(encoding="utf-8"))}


@pytest.mark.parametrize("name", sorted(PILOTS))
def test_pilot_is_registered_valid_and_documented(name):
    mode, max_tris = PILOTS[name]
    entry = index()[name]
    assert entry["recorrible"] and entry["mode"] == mode and entry["description"]
    manifest = json.loads((VARIANTS / name / "manifest.json").read_text(encoding="utf-8"))
    assert manifest["recorrible"] and manifest["mode"] == mode and manifest["format"] == "TNTM2"
    budget = check_budget(VARIANTS / name)
    assert budget["ok"] and budget["triangles"] <= max_tris
    assert check_seams(VARIANTS / name)["ok"]
    assert (VARIANTS / name / "CREDITS.txt").read_text(encoding="utf-8").strip()
    sheet = SHEETS / f"{name}.png"
    assert sheet.exists() and sheet.stat().st_size < 1_000_000


def test_private_maps_stay_out_of_the_index():
    assert not [n for n in index() if n.startswith("F")]


def _same_chunk(model, col: int, row: int, name: str, tmp_path: Path) -> None:
    out = tmp_path / "chunk.bin"
    write_chunk(out, build_chunk(model, col, row))
    assert out.read_bytes() == (VARIANTS / name / "Chunks" / f"r{row}c{col}.bin").read_bytes()


def test_l10_regenerates_the_same_from_the_stored_rasters(tmp_path):
    pytest.importorskip("pyproj")
    from gen_terrain_geo import build_model
    from terrain_geo.presets import PRESETS
    model, info = build_model(PRESETS["L10_japon_fuji"])
    assert 0.15 <= info["exaggeration"] <= 6.0 and info["projection"] == "laea"
    _same_chunk(model, 2, 4, "L10_japon_fuji", tmp_path)                                  # el Fuji


def test_i01_and_a01_regenerate_the_same(tmp_path):
    from terrain_shapes.archipelago import build_archipelago, philippines_rare
    from terrain_shapes.arena import build_diana, diana_a01
    _same_chunk(build_archipelago(philippines_rare()).model, 1, 1, "I01_filipinas_rara", tmp_path)
    _same_chunk(build_diana(diana_a01()).model, 0, 0, "A01_diana", tmp_path)

"""Criterios de aceptación de E01B_espana_rally (Rally punto a punto por España) sobre la variante YA GENERADA
(manifest y trozos de Scripts/terrain_volumes/Variants/E01B_espana_rally): Docs/Rally_E01B_y_Biplaza.md §2.

    uv run pytest Scripts/tests/test_terrain_rally_spain.py

La variante se regenera con `uv run --with pyfqmr python Scripts/gen_terrain_rally_spain.py`.
"""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path

import numpy as np
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from terrain_geo.build import VARIANTS  # noqa: E402
from terrain_geo.rally_corridor import MeshSampler, corridor_report, flat_widths  # noqa: E402
from terrain_vol.layout import UU_PER_M, WATER_M  # noqa: E402

NAME = "E01B_espana_rally"
OUT = VARIANTS / NAME
E01 = VARIANTS / "E01_espana"

pytestmark = pytest.mark.skipif(not (OUT / "manifest.json").exists(), reason=f"{NAME} sin generar")


def chunk_bytes(variant: Path) -> int:
    manifest = json.loads((variant / "manifest.json").read_text(encoding="utf-8"))
    return sum((variant / c["file"]).stat().st_size for c in manifest["cells"])


@pytest.fixture(scope="module")
def manifest() -> dict:
    return json.loads((OUT / "manifest.json").read_text(encoding="utf-8"))


@pytest.fixture(scope="module")
def sampler(manifest) -> MeshSampler:
    return MeshSampler(OUT, manifest)


@pytest.fixture(scope="module")
def report(sampler) -> dict:
    return corridor_report(OUT, sampler)


# ── Formato y registro ───────────────────────────────────────────────────────────
def test_registrada_en_el_indice_con_preview(manifest):
    index = json.loads((VARIANTS / "index.json").read_text(encoding="utf-8"))
    entry = next(e for e in index if e["name"] == NAME)
    assert entry["mode"] == "rally" and entry["recorrible"] is True
    assert (OUT / "preview.png").stat().st_size > 10_000
    assert manifest["format"] == "TNTM2" and manifest["mode"] == "rally" and manifest["closed"] is False
    assert all((OUT / c["file"]).exists() for c in manifest["cells"])


def test_agua_y_cajas_de_muerte_como_e01(manifest):
    e01 = json.loads((E01 / "manifest.json").read_text(encoding="utf-8"))
    assert manifest["water_uu"] == e01["water_uu"]
    (box,), (ref,) = manifest["kill_boxes_uu"], e01["kill_boxes_uu"]
    assert box["yaw"] == ref["yaw"] == 0.0
    top, ref_top = box["center"][2] + box["extent"][2], ref["center"][2] + ref["extent"][2]
    assert top == pytest.approx(ref_top) and box["center"][2] - box["extent"][2] == pytest.approx(ref["center"][2] - ref["extent"][2])
    for cell in manifest["cells"]:                     # la caja cubre todos los trozos
        for axis in (0, 1):
            assert abs(cell["center_uu"][axis] - box["center"][axis]) + 5000.0 <= box["extent"][axis] + 1e-3


def test_trozos_no_pesan_mas_que_e01():
    assert chunk_bytes(OUT) <= chunk_bytes(E01)


def test_exageracion_reducida_y_relieve_de_pirineo_a_playa(manifest, sampler):
    geo = manifest["geo"]
    assert 3.0 <= geo["exaggeration"] <= 6.0
    assert geo["exaggeration"] == pytest.approx(geo["k"] * geo["ground_m_per_game_m"], rel=1e-3)
    assert geo["ground_m_per_game_m"] == pytest.approx(400.0, rel=0.01)
    start_z, end_z = manifest["start_uu"][2] / UU_PER_M, manifest["end_uu"][2] / UU_PER_M
    assert start_z - WATER_M >= 8.0, "la salida está en el Pirineo"
    assert end_z - WATER_M <= 4.0, "la meta está junto al mar"
    hitos = manifest["markers_uu"]
    pyrenees = hitos["hito_canfranc"][0][2] / UU_PER_M
    meseta = hitos["hito_guadalajara"][0][2] / UU_PER_M
    assert pyrenees > meseta > end_z


# ── Calzada ──────────────────────────────────────────────────────────────────────
def test_longitud_de_calzada(report):
    assert 1800.0 <= report["road_m"] <= 2600.0
    assert 1800.0 <= report["checkpoints"]["course_m"] <= 2600.0
    assert report["missing_samples"] == 0
    assert report["min_above_water_m"] > 0.3


def test_pendientes_y_escalon(report):
    assert report["sustained_grade_deg"] <= 12.0          # ventanas de 30 m
    assert report["short_grade_deg"] <= 20.0              # ventanas de 4 m (< 30 m)
    assert report["max_step_m"] <= 0.4                    # entre muestras de 1 m


def test_ancho_y_radio(report):
    assert report["min_width_m"] >= 12.0
    assert report["min_radius_m"] >= 25.0


def test_el_ancho_detecta_un_corte():
    """Caso negativo del validador de ancho: un escalón de 1 m a 4 m a cada lado del eje deja 8 m de llano, no 12."""
    class Step:
        def top(self, xy):
            xy = np.asarray(xy).reshape(-1, 2)
            return np.where(np.abs(xy[:, 1]) > 4.0, 1.0, 0.0)
    pts = np.column_stack([np.arange(20.0), np.zeros(20)])
    widths = flat_widths(Step(), pts, np.zeros(20))
    assert widths.max() < 12.0


# ── Checkpoints ──────────────────────────────────────────────────────────────────
def test_checkpoints_sobre_la_calzada(manifest, report):
    cp = report["checkpoints"]
    assert cp["count"] >= 8 and all(len(c) == 4 for c in manifest["checkpoints_uu"])
    assert cp["ordered"]
    assert 150.0 <= cp["gap_min_m"] and cp["gap_max_m"] <= 250.0
    assert cp["max_offset_m"] <= 1.0 and cp["max_dz_m"] <= 0.3
    assert cp["max_yaw_err_deg"] <= 10.0
    assert cp["covered"] == 0, "ningún checkpoint en túnel ni en puente"


def test_salida_y_meta_en_los_extremos(manifest, report):
    cp = report["checkpoints"]
    assert cp["start_arc_m"] <= 80.0
    assert report["road_m"] - cp["end_arc_m"] <= 50.0
    road = np.asarray(manifest["road_uu"]) / UU_PER_M
    start, end = np.array(manifest["start_uu"][:2]) / UU_PER_M, np.array(manifest["end_uu"][:2]) / UU_PER_M
    assert np.hypot(*(start - road[0, :2])) < np.hypot(*(start - road[-1, :2]))
    assert math.hypot(*(end - road[-1, :2])) < math.hypot(*(end - road[0, :2]))

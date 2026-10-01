"""Genera E01B_espana_rally (Rally punto a punto por España, Docs/Rally_E01B_y_Biplaza.md §2) en
Scripts/terrain_volumes/Variants/E01B_espana_rally/ (trozos TNTM2, manifest, vistas) y lo añade a index.json.

    uv run --with pyfqmr python Scripts/gen_terrain_rally_spain.py [--no-decimate]

El trazado y el terreno están en terrain_geo/rally_spain.py; la validación del corredor, sobre la variante ya
escrita, en terrain_geo/rally_corridor.py (la repite Scripts/tests/test_terrain_rally_spain.py).

Manifest (además de lo común de write_map): mode "rally", closed false, road_uu (eje cada 1 m, [x, y, z] en uu),
road_width_m, checkpoints_uu ([x, y, z, yaw], yaw = rumbo de la marcha en grados de Unreal) entre start_uu (línea
de salida, tras la recta de parrilla) y end_uu (meta, antes de la escapatoria), markers_uu (parrilla e hitos),
kill_boxes_uu (el agua de todo el volumen), geo (proyección, escala y exageración) y checks (el validador).
"""

from __future__ import annotations

import argparse
import json
import math
import time

import numpy as np
from PIL import Image
from scipy import ndimage
from terrain_geo import rally_spain as rs
from terrain_geo.build import VARIANTS, dir_size_mb, kill_boxes_uu, update_index, write_credits
from terrain_geo.heightfield import ZONES
from terrain_geo.layout import SPAIN_REGION
from terrain_geo.rally_corridor import corridor_report
from terrain_vol.export import global_top, write_map
from terrain_vol.layout import CELL_SAMPLES, MAP_MIN_M, UU_PER_M, WATER_M
from terrain_vol.mesh import build_chunk

CHECKPOINT_EVERY_M = 200.0
DECIMATE_ERROR_M = 0.08            # trozos con la calzada o su talud
DECIMATE_FAR_M = 0.25              # trozos de fondo (a mas de NEAR_M del eje)
NEAR_M = 20.0

# Criterios de aceptación (los mismos que comprueba el test).
LIMITS = {"road_m": (1800.0, 2600.0), "sustained_grade_deg": 12.0, "short_grade_deg": 20.0, "max_step_m": 0.4,
          "min_width_m": 12.0, "min_radius_m": 25.0, "checkpoint_gap_m": (150.0, 250.0), "exaggeration": (3.0, 6.0)}


def sample_top(top: np.ndarray, pts: np.ndarray) -> np.ndarray:
    coords = [pts[:, 0] - MAP_MIN_M, pts[:, 1] - MAP_MIN_M]
    return ndimage.map_coordinates(top, coords, order=1, mode="nearest")


def checkpoint_indices(arc: np.ndarray, start: int, end: int) -> list[int]:
    """Índices del eje repartidos a partes iguales entre salida y meta, a unos CHECKPOINT_EVERY_M."""
    course = arc[end] - arc[start]
    parts = max(1, int(round(course / CHECKPOINT_EVERY_M)))
    marks = arc[start] + course * np.arange(1, parts) / parts
    return [int(np.searchsorted(arc, m)) for m in marks]


def yaw_deg(pts: np.ndarray, k: int) -> float:
    t = pts[min(k + 2, len(pts) - 1)] - pts[max(k - 2, 0)]
    return math.degrees(math.atan2(t[1], t[0]))


def uu(p, z: float, yaw: float | None = None) -> list[float]:
    out = [round(float(p[0]) * UU_PER_M, 1), round(float(p[1]) * UU_PER_M, 1), round(float(z) * UU_PER_M, 1)]
    return out + [round(yaw, 1)] if yaw is not None else out


def render_preview(out, top: np.ndarray, frame: rs.Frame, built: set, road: np.ndarray, cps: list[int]) -> None:
    """Vista cenital (Norte arriba) del rectángulo del mapa: relieve sombreado, agua, trozos no generados en gris
    oscuro; preview_debug.png añade la calzada (rojo) y los checkpoints (amarillo)."""
    h, w = frame.rows * 100 + 1, frame.cols * 100 + 1
    t = top[:h, :w]
    gx, gy = np.gradient(t)
    light = np.clip((gx * 0.5 - gy * 0.35 + 1.0) / np.sqrt(gx * gx + gy * gy + 1.0) * 0.8, 0.0, 1.0)
    height = np.clip((t - WATER_M) / 40.0, 0.0, 1.0)[..., None]
    base = np.array([0.93, 0.84, 0.60]) * (1 - height) + np.array([0.62, 0.45, 0.30]) * height
    rgb = base * (0.35 + 0.65 * light)[..., None]
    rgb = np.where((t < WATER_M)[..., None], rgb * 0.25 + np.array([0.12, 0.35, 0.65]) * 0.75, rgb)
    mask = np.zeros((h, w), dtype=bool)
    for col, row in built:
        mask[row * 100:row * 100 + 101, col * 100:col * 100 + 101] = True
    rgb = np.where(mask[..., None], rgb, np.array([0.18, 0.18, 0.2]))
    Image.fromarray((np.clip(rgb, 0, 1) * 255).astype(np.uint8)[::-1]).save(out / "preview.png")
    debug = rgb.copy()
    for k, (x, y) in enumerate(road):
        i, j = int(round(x - MAP_MIN_M)), int(round(y - MAP_MIN_M))
        if 0 <= i < h and 0 <= j < w:
            debug[max(0, i - 2):i + 3, max(0, j - 2):j + 3] = (0.9, 0.1, 0.1)
    for k in cps:
        i, j = int(round(road[k, 0] - MAP_MIN_M)), int(round(road[k, 1] - MAP_MIN_M))
        debug[max(0, i - 6):i + 7, max(0, j - 6):j + 7] = (1.0, 0.9, 0.1)
    Image.fromarray((np.clip(debug, 0, 1) * 255).astype(np.uint8)[::-1]).save(out / "preview_debug.png")


def verdict(report: dict, exaggeration: float) -> dict[str, bool]:
    cp = report["checkpoints"]
    return {"road_m": LIMITS["road_m"][0] <= report["road_m"] <= LIMITS["road_m"][1],
            "sustained_grade": report["sustained_grade_deg"] <= LIMITS["sustained_grade_deg"],
            "short_grade": report["short_grade_deg"] <= LIMITS["short_grade_deg"],
            "step": report["max_step_m"] <= LIMITS["max_step_m"],
            "width": report["min_width_m"] >= LIMITS["min_width_m"],
            "radius": report["min_radius_m"] >= LIMITS["min_radius_m"],
            "on_mesh": report["missing_samples"] == 0 and report["min_above_water_m"] > 0.3,
            "checkpoints": (LIMITS["checkpoint_gap_m"][0] <= cp["gap_min_m"] and cp["gap_max_m"] <= LIMITS["checkpoint_gap_m"][1]
                            and cp["ordered"] and cp["covered"] == 0),
            "exaggeration": LIMITS["exaggeration"][0] <= exaggeration <= LIMITS["exaggeration"][1]}


def build(decimate: bool = True) -> dict:
    t0 = time.time()
    frame = rs.make_frame()
    model = rs.RallySpainModel(frame)
    gaps = model.cell_gaps()
    chunks = {cell: build_chunk(model, *cell) for cell in gaps}
    if decimate:
        from terrain_vol.decimate import decimate_chunks
        near = {c: m for c, m in chunks.items() if gaps[c] <= NEAR_M}
        far = {c: m for c, m in chunks.items() if gaps[c] > NEAR_M}
        chunks = {**decimate_chunks(near, DECIMATE_ERROR_M), **decimate_chunks(far, DECIMATE_FAR_M)}
    grid = frame.grid
    top = global_top(chunks, grid=grid)
    road = model.road
    arc = rs.arc_length(road)
    z = sample_top(top, road)
    start = int(np.searchsorted(arc, rs.GRID_M))
    end = int(np.searchsorted(arc, arc[-1] - rs.RUNOFF_M))
    cps = checkpoint_indices(arc, start, end)
    hitos = {name: frame.projection.to_game(lon, lat) for name, lat, lon in rs.HITOS}
    hito_pts = {name: np.array([float(v) for v in xy]) for name, xy in hitos.items()}
    exaggeration = model.k * frame.projection.ground_m_per_game_m()
    extra = {
        "description": rs.DESCRIPTION, "mode": "rally", "closed": False, "laps": 1, "kill_boxes_uu": kill_boxes_uu(grid),
        "z_range": [model.z_range.z_min_m, model.z_range.levels, model.z_range.step_m],
        "generator": {"generator": "rally_spain", "seed": rs.SEED, "hitos": [list(h) for h in rs.HITOS],
                      "bends": [list(map(list, b)) for b in rs.BENDS], "road_w_m": rs.ROAD_W_M,
                      "shoulder_m": rs.SHOULDER_M, "max_grade_deg": rs.MAX_GRADE_DEG, "band_m": rs.BAND_M,
                      "decimate_m": [DECIMATE_ERROR_M, DECIMATE_FAR_M] if decimate else 0.0,
                      "near_m": NEAR_M},
        "geo": {"source": "ES_dem.png (E01)", "projection": "merc", "center_lonlat": list(frame.projection.center),
                "ground_m_per_game_m": round(frame.projection.ground_m_per_game_m(), 1),
                "exaggeration": round(exaggeration, 3), "k": model.k, "rows": frame.rows, "cols": frame.cols},
        "road_width_m": rs.ROAD_W_M, "road_uu": [uu(p, zz) for p, zz in zip(road, z)],
        "checkpoints_uu": [uu(road[k], z[k], yaw_deg(road, k)) for k in cps],
        "start_yaw": round(yaw_deg(road, start), 1),
        "tunnels": [], "decks": [], "bridges": [],
        "markers_uu": {"parrilla": [uu(road[0], z[0]), uu(road[start], z[start])],
                       **{f"hito_{name}": [uu(p, float(sample_top(top, p[None, :])[0]))] for name, p in hito_pts.items()}},
    }
    out = VARIANTS / rs.NAME
    size = grid * (CELL_SAMPLES - 1) + 1
    zones = {zone: (np.ones((size, size)) if zone == "cliffs" else np.zeros((size, size))) for zone in ZONES}
    write_map(out, rs.NAME, rs.SEED, chunks, (*road[start], z[start]), (*road[end], z[end]), zones, road,
              extra_manifest=extra, grid=grid)
    write_credits(out, SPAIN_REGION.credits(("Trazado de Rally E01B: Scripts/gen_terrain_rally_spain.py.",)))
    render_preview(out, top, frame, set(chunks), road, cps)
    report = corridor_report(out)
    checks = verdict(report, exaggeration)
    ok = all(checks.values())
    data = json.loads((out / "manifest.json").read_text(encoding="utf-8"))
    data["checks"] = {"corridor": report, "verdict": checks}
    data["recorrible"] = ok
    (out / "manifest.json").write_text(json.dumps(data, indent=1), encoding="utf-8")
    size_mb = dir_size_mb(out)
    update_index(rs.NAME, rs.SEED, ok, size_mb, rs.DESCRIPTION, {"mode": "rally"})
    chunk_bytes = sum(f.stat().st_size for f in (out / "Chunks").iterdir())
    return {"ok": ok, "time_s": round(time.time() - t0, 1), "cells": len(chunks), "size_mb": size_mb,
            "chunk_bytes": chunk_bytes, "exaggeration": round(exaggeration, 2), "report": report, "checks": checks}


def main() -> None:
    parser = argparse.ArgumentParser(description="Genera E01B_espana_rally (Rally punto a punto por España).")
    parser.add_argument("--no-decimate", action="store_true", help="no decimar los trozos (no necesita pyfqmr)")
    args = parser.parse_args()
    r = build(decimate=not args.no_decimate)
    print(json.dumps({k: v for k, v in r.items()}, indent=1, ensure_ascii=False))


if __name__ == "__main__":
    main()

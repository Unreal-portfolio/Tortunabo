"""Genera el mapa de plataformas P01 (rios, mesetas de arena y puentes colgantes) a partir del plano
de Scripts/terrain_platforms/P01_layout.png y lo deja en Scripts/terrain_volumes/Variants/P01_plataformas/
(trozos TNTM2, manifest y vistas), listo para el desplegable de ATN_MapVariantLoader.

    uv run --with numpy --with scipy --with pillow --with scikit-image python Scripts/gen_terrain_platforms.py
    ... gen_terrain_platforms.py --layout otro_layout.png --name P02_otro

El layout sale de un plano dibujado con Scripts/terrain_platforms/extract_layout.py. Cada mapa se
comprueba sobre la malla: el inicio llega a pie al final por los puentes y a todas las plataformas.
"""

from __future__ import annotations

import argparse
import json
import time
from pathlib import Path

import numpy as np
from scipy import ndimage

from gen_terrain_volume import build_all, global_standable, ground_level, walk, world_index, zone_map
from terrain_platforms.layout import (GRID, LAYOUT_FILE, LAYOUT_ORIGIN_M, MAP_MAX_M, MAP_MIN_M, PLATFORM_ABOVE_WATER_M, PX_M,
                                      RIVER_DEPTH_M, UU_PER_M, WATER_M, load_layout)
from terrain_platforms.model import SEED, PlatformModel
from terrain_vol.export import global_top, write_map

VARIANTS = Path(__file__).resolve().parent / "terrain_volumes" / "Variants"
DEFAULT_NAME = "P01_plataformas"
DESCRIPTION = ("Plataformas de arena a +20, +25 y +30 m sobre un rio a 0 m, unidas por puentes colgantes; "
               "caer al rio es morir.")
KILL_TOP_M = WATER_M + 2.0              # cara de arriba de la caja de muerte del rio
MIN_PLATFORM_M2 = 30.0                  # menos que esto no cuenta como plataforma en las comprobaciones


def interior_point(mask: np.ndarray) -> tuple[float, float]:
    """Punto (X, Y del mundo, m) mas lejos del contorno de una mascara del layout."""
    depth = ndimage.distance_transform_edt(mask)
    i, j = np.unravel_index(int(np.argmax(depth)), depth.shape)
    return LAYOUT_ORIGIN_M + (i + 0.5) * PX_M, LAYOUT_ORIGIN_M + (j + 0.5) * PX_M


def platform_masks(layout: np.ndarray) -> list[tuple[int, np.ndarray]]:
    """(clase, mascara) de cada plataforma: componente conexa de una clase de plataforma."""
    out = []
    for cls in PLATFORM_ABOVE_WATER_M:
        comp, count = ndimage.label(layout == cls)
        out += [(cls, comp == k) for k in range(1, count + 1) if (comp == k).sum() * PX_M ** 2 >= MIN_PLATFORM_M2]
    return out


def pick_start_end(layout: np.ndarray) -> tuple[tuple[float, float], tuple[float, float]]:
    """Inicio en la plataforma mas baja de mas superficie y final en la mas alta enlazada por puentes."""
    platforms = platform_masks(layout)
    lowest = min(cls for cls, _ in platforms)
    start_mask = max((m for cls, m in platforms if cls == lowest), key=lambda m: m.sum())
    # La plataforma de mas altura del plano con mas superficie; su parte mas al sur (el lobulo de abajo).
    highest = max(cls for cls, _ in platforms)
    end_mask = max((m for cls, m in platforms if cls == highest), key=lambda m: m.sum()).copy()
    rows = np.nonzero(end_mask.any(axis=1))[0]
    end_mask[rows.min() + int(0.45 * (rows.max() - rows.min())):] = False      # deja el 45 % sur
    return interior_point(start_mask), interior_point(end_mask)


def check(model: PlatformModel, chunks, start_xy, end_xy) -> dict:
    standable = global_standable(chunks, grid=GRID)
    s_ij, e_ij = world_index(start_xy), world_index(end_xy)
    start = (*s_ij, ground_level(standable, *s_ij))
    seen = walk(standable, start) if start[2] >= 0 else np.zeros_like(standable)
    flat = seen.any(axis=2)
    reached, missed = [], []
    for cls, mask in platform_masks(model.layout):
        px, py = interior_point(mask)
        i, j = world_index((px, py))
        (reached if flat[i, j] else missed).append(f"+{int(PLATFORM_ABOVE_WATER_M[cls])} m en ({px:.0f}, {py:.0f})")
    bridges_ok = 0
    for bridge in model.bridges:
        ends = [world_index(bridge.points[k]) for k in (len(bridge.points) // 2,)]
        bridges_ok += all(flat[i - 1:i + 2, j - 1:j + 2].any() for i, j in ends)
    return {"end_reached": bool(seen[e_ij].any()), "reached": reached, "missed": missed,
            "bridges_walked": bridges_ok, "bridges": len(model.bridges), "start_ground": start[2] >= 0}


def kill_boxes_uu() -> list[dict]:
    """Una caja sobre todo el rio: el agua no se nada, caer es morir (0,5 s dentro bastan)."""
    z0, z1 = WATER_M - RIVER_DEPTH_M - 1.0, KILL_TOP_M
    half = (MAP_MAX_M - MAP_MIN_M) / 2.0
    center = (MAP_MIN_M + MAP_MAX_M) / 2.0
    return [{"center": [center * UU_PER_M, center * UU_PER_M, 0.5 * (z0 + z1) * UU_PER_M],
             "extent": [half * UU_PER_M, half * UU_PER_M, 0.5 * (z1 - z0) * UU_PER_M], "yaw": 0.0}]


def update_index(name: str, seed: int, ok: bool, size_mb: float) -> None:
    index_path = VARIANTS / "index.json"
    index = json.loads(index_path.read_text(encoding="utf-8")) if index_path.exists() else []
    entry = {"name": name, "seed": seed, "description": DESCRIPTION, "recorrible": ok, "size_mb": size_mb}
    index = [e for e in index if e["name"] != name] + [entry]
    index_path.write_text(json.dumps(index, indent=1, ensure_ascii=False), encoding="utf-8")


def build(name: str, layout_path: Path, seed: int) -> dict:
    t0 = time.time()
    model = PlatformModel(seed, load_layout(layout_path))
    chunks = build_all(model, grid=GRID)
    start_xy, end_xy = pick_start_end(model.layout)
    result = check(model, chunks, start_xy, end_xy)
    top = global_top(chunks, grid=GRID)
    s_ij, e_ij = world_index(start_xy), world_index(end_xy)
    out = VARIANTS / name
    route = np.concatenate([b.points for b in model.bridges])
    ok = result["end_reached"] and result["start_ground"]
    write_map(out, name, seed, chunks, (*start_xy, float(top[s_ij])), (*end_xy, float(top[e_ij])),
              zone_map(model, chunks, grid=GRID), route,
              extra_manifest={"description": DESCRIPTION, "recorrible": ok, "kill_boxes_uu": kill_boxes_uu()}, grid=GRID)
    size_mb = round(sum(f.stat().st_size for f in out.rglob("*") if f.is_file()) / (1024 * 1024), 2)
    update_index(name, seed, ok, size_mb)
    return {"name": name, "ok": ok, "size_mb": size_mb, "time_s": round(time.time() - t0, 1), "start": start_xy,
            "end": end_xy, **result}


def main() -> None:
    parser = argparse.ArgumentParser(description="Genera el mapa de plataformas y puentes colgantes.")
    parser.add_argument("--layout", type=Path, default=LAYOUT_FILE)
    parser.add_argument("--name", default=DEFAULT_NAME)
    parser.add_argument("--seed", type=int, default=SEED)
    args = parser.parse_args()
    r = build(args.name, args.layout, args.seed)
    print(f"{r['name']}: {'OK' if r['ok'] else 'NO VALIDO'} {r['time_s']} s, {r['size_mb']} MB; "
          f"inicio {tuple(round(v) for v in r['start'])} -> final {tuple(round(v) for v in r['end'])}: "
          f"{'SI' if r['end_reached'] else 'NO'} a pie; puentes recorridos {r['bridges_walked']}/{r['bridges']}")
    print("plataformas alcanzadas:", r["reached"])
    print("plataformas NO alcanzadas:", r["missed"] or "ninguna")


if __name__ == "__main__":
    main()

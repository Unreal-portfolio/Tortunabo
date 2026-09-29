"""Genera el mapa de España (relieve real y frontera como costa) en
Scripts/terrain_volumes/Variants/E01_espana/ (trozos TNTM2, manifest y vistas) y lo añade a index.json,
listo para el desplegable de ATN_MapVariantLoader.

    uv run --with numpy --with scipy --with pillow --with scikit-image python Scripts/gen_terrain_spain.py
    ... gen_terrain_spain.py --start 43.009,-1.320 --end 42.881,-8.545 --peak 28 --name E01_espana

Los datos salen de Scripts/terrain_geo/data (ES_dem.png y ES_mask.png, ver terrain_geo/fetch_spain.py).
Por defecto el inicio es Roncesvalles y el final Santiago de Compostela (Camino Francés); se
comprueba sobre la malla que se llega a pie de uno a otro.
"""

from __future__ import annotations

import argparse
import json
import time
from pathlib import Path

import numpy as np

from gen_terrain_volume import build_all, world_index, zone_map
from terrain_geo.layout import GRID, MAP_MAX_M, MAP_MIN_M, UU_PER_M, WATER_M, lonlat_to_game
from terrain_geo.model import PEAK_ABOVE_WATER_M, SEED, SpainModel
from terrain_vol.export import global_top, write_map
from terrain_vol.layout import Z_MIN_M

VARIANTS = Path(__file__).resolve().parent / "terrain_volumes" / "Variants"
DEFAULT_NAME = "E01_espana"
DESCRIPTION = ("España con el relieve real (1 m de juego = 2,1 km de suelo, cimas a unos 29 m sobre el agua): la frontera es la costa y "
               "el mar es la muerte. Del Pirineo a Santiago por la Meseta.")
DEFAULT_START = (43.009, -1.320)         # Roncesvalles
DEFAULT_END = (42.881, -8.545)           # Santiago de Compostela
CLIMB_M = 1.0                            # desnivel maximo entre muestras vecinas (1 m de juego): 45 grados
DRY_M = WATER_M + 0.1                    # por debajo de esto es agua: no se anda
KILL_TOP_M = WATER_M + 0.2               # cara de arriba de la caja de muerte del mar


def flood(top: np.ndarray, start: tuple[int, int]) -> np.ndarray:
    """Muestras alcanzables a pie desde start sobre el mapa de alturas: en seco y con desnivel <= CLIMB_M."""
    dry = top > DRY_M
    seen = np.zeros(top.shape, dtype=bool)
    if not dry[start]:
        return seen
    seen[start] = True
    frontier = seen.copy()
    while frontier.any():
        grow = np.zeros_like(seen)
        for axis, sign in ((0, 1), (0, -1), (1, 1), (1, -1)):
            moved = np.roll(frontier, sign, axis=axis)
            other = np.roll(top, sign, axis=axis)
            edge = [slice(None), slice(None)]
            edge[axis] = slice(0, 1) if sign == 1 else slice(-1, None)
            moved[tuple(edge)] = False                   # np.roll da la vuelta: el borde no se conecta con el opuesto
            grow |= moved & dry & (np.abs(top - other) <= CLIMB_M)
        frontier = grow & ~seen
        seen |= frontier
    return seen


def check(model: SpainModel, top: np.ndarray, start_xy, end_xy) -> dict:
    s_ij, e_ij = world_index(start_xy), world_index(end_xy)
    seen = flood(top, s_ij)
    land = model.coverage > 0.9
    px = model.coverage.shape[0] // top.shape[0]
    land_1m = land[::px, ::px][: top.shape[0], : top.shape[1]]
    return {"end_reached": bool(seen[e_ij]), "start_dry": bool(top[s_ij] > DRY_M), "end_dry": bool(top[e_ij] > DRY_M),
            "walkable_share": float(seen[land_1m].sum() / max(int(land_1m.sum()), 1)),
            "land_km2_share": float(land_1m.mean())}


def kill_boxes_uu() -> list[dict]:
    """Una caja sobre todo el mar: no se nada, caer al agua es morir (0,5 s dentro bastan)."""
    z0, z1 = Z_MIN_M, KILL_TOP_M
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


def parse_point(text: str) -> tuple[float, float]:
    lat, lon = (float(v) for v in text.split(","))
    return lat, lon


def build(name: str, seed: int, peak: float, start: tuple[float, float], end: tuple[float, float]) -> dict:
    t0 = time.time()
    model = SpainModel(seed, peak)
    chunks = build_all(model)
    top = global_top(chunks)
    start_xy = tuple(float(v) for v in lonlat_to_game(start[1], start[0]))
    end_xy = tuple(float(v) for v in lonlat_to_game(end[1], end[0]))
    result = check(model, top, start_xy, end_xy)
    ok = result["end_reached"]
    s_ij, e_ij = world_index(start_xy), world_index(end_xy)
    line = np.linspace(start_xy, end_xy, 300)
    out = VARIANTS / name
    write_map(out, name, seed, chunks, (*start_xy, float(top[s_ij])), (*end_xy, float(top[e_ij])),
              zone_map(model, chunks), line, style=None,
              extra_manifest={"description": DESCRIPTION, "recorrible": ok, "kill_boxes_uu": kill_boxes_uu(),
                              "peak_above_water_m": peak}, grid=GRID)
    size_mb = round(sum(f.stat().st_size for f in out.rglob("*") if f.is_file()) / (1024 * 1024), 2)
    update_index(name, seed, ok, size_mb)
    return {"name": name, "ok": ok, "size_mb": size_mb, "time_s": round(time.time() - t0, 1), "start": start_xy,
            "end": end_xy, "top_max": float(top.max()), **result}


def main() -> None:
    parser = argparse.ArgumentParser(description="Genera el mapa de España con relieve real.")
    parser.add_argument("--name", default=DEFAULT_NAME)
    parser.add_argument("--seed", type=int, default=SEED)
    parser.add_argument("--peak", type=float, default=PEAK_ABOVE_WATER_M, help="cima (3480 m reales) sobre el agua, en m de juego")
    parser.add_argument("--start", type=parse_point, default=DEFAULT_START, help="lat,lon del inicio")
    parser.add_argument("--end", type=parse_point, default=DEFAULT_END, help="lat,lon del final")
    args = parser.parse_args()
    r = build(args.name, args.seed, args.peak, args.start, args.end)
    print(f"{r['name']}: {'OK' if r['ok'] else 'NO VALIDO'} {r['time_s']} s, {r['size_mb']} MB; cima {r['top_max']:.1f} m; "
          f"inicio {tuple(round(v) for v in r['start'])} -> final {tuple(round(v) for v in r['end'])}: "
          f"{'SI' if r['end_reached'] else 'NO'} a pie; {r['walkable_share'] * 100:.0f} % de España alcanzable desde el inicio")


if __name__ == "__main__":
    main()

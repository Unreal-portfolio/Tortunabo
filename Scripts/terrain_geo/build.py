"""Piezas comunes de los generadores de mapas sobre un campo de alturas (España, regiones reales, islas, arenas):
recorrido a pie sobre la cota superior, caja de muerte del agua, CREDITS, index.json y rasters guardados.
"""

from __future__ import annotations

import json
from pathlib import Path

import numpy as np
from PIL import Image
from scipy import ndimage

from terrain_vol.layout import CELL_M, MAP_MIN_M, UU_PER_M, WATER_M, Z_MIN_M

VARIANTS = Path(__file__).resolve().parents[1] / "terrain_volumes" / "Variants"
DATA_DIR = Path(__file__).resolve().parent / "data"
CLIMB_M = 1.0                            # desnivel maximo entre muestras vecinas (1 m de juego): 45 grados
DRY_M = WATER_M + 0.1                    # por debajo de esto es agua: no se anda
KILL_TOP_M = WATER_M + 0.2               # cara de arriba de la caja de muerte del agua
ELEVATION_OFFSET_M = 5000                # los PNG de 16 bits guardan elevacion + este desfase
PRIVATE_PREFIXES = ("F",)                # mapas privados (retratos): el indice no se toca para ellos


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


def kill_boxes_uu(grid: int = 6, top_m: float = KILL_TOP_M, bottom_m: float = Z_MIN_M) -> list[dict]:
    """Una caja sobre toda el agua del volumen: no se nada, caer al agua es morir (0,5 s dentro bastan)."""
    half = grid * CELL_M / 2.0
    center = MAP_MIN_M + half
    return [{"center": [center * UU_PER_M, center * UU_PER_M, 0.5 * (bottom_m + top_m) * UU_PER_M],
             "extent": [half * UU_PER_M, half * UU_PER_M, 0.5 * (top_m - bottom_m) * UU_PER_M], "yaw": 0.0}]


def update_index(name: str, seed: int, ok: bool, size_mb: float, description: str, extra: dict | None = None,
                 allow_private: bool = False) -> None:
    """Añade o sustituye la entrada `name` de Variants/index.json; las demas quedan tal cual."""
    if name.startswith(PRIVATE_PREFIXES) and not allow_private:
        raise ValueError(f"{name}: los mapas F* son privados y no van al indice")
    index_path = VARIANTS / "index.json"
    index = json.loads(index_path.read_text(encoding="utf-8")) if index_path.exists() else []
    entry = {"name": name, "seed": seed, "description": description, "recorrible": ok, "size_mb": size_mb, **(extra or {})}
    index = [e for e in index if e["name"] != name] + [entry]
    index_path.write_text(json.dumps(index, indent=1, ensure_ascii=False), encoding="utf-8")


def dir_size_mb(path: Path) -> float:
    return round(sum(f.stat().st_size for f in path.rglob("*") if f.is_file()) / (1024 * 1024), 2)


def write_credits(out: Path, text: str) -> None:
    (out / "CREDITS.txt").write_text(text, encoding="utf-8")


def world_index(p, grid_origin: float = MAP_MIN_M) -> tuple[int, int]:
    return int(round(p[0] - grid_origin)), int(round(p[1] - grid_origin))


def world_point(ij: tuple[int, int]) -> tuple[float, float]:
    return MAP_MIN_M + ij[0], MAP_MIN_M + ij[1]


def deepest_dry(top: np.ndarray) -> tuple[int, int]:
    """Muestra seca mas lejos del agua (el corazon de la mayor masa de tierra)."""
    depth = ndimage.distance_transform_edt(top > DRY_M)
    i, j = np.unravel_index(int(np.argmax(depth)), depth.shape)
    return int(i), int(j)


def farthest_reachable(top: np.ndarray, seen: np.ndarray, start: tuple[int, int]) -> tuple[int, int]:
    """Muestra alcanzada mas lejos (en linea recta) de start y a mas de 3 m del agua."""
    inner = seen & (ndimage.distance_transform_edt(top > DRY_M) > 3.0)
    candidates = np.argwhere(inner if inner.any() else seen)
    k = int(np.argmax(((candidates - np.array(start)) ** 2).sum(axis=1)))
    return int(candidates[k][0]), int(candidates[k][1])


# ── Rasters guardados (PNG de 16 bits el MDE, de 8 la cobertura; Norte arriba) ───
def raster_paths(name: str) -> tuple[Path, Path]:
    return DATA_DIR / f"{name}_dem.png", DATA_DIR / f"{name}_mask.png"


def save_rasters(name: str, dem: np.ndarray, coverage: np.ndarray) -> None:
    dem_path, mask_path = raster_paths(name)
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    packed = np.clip(np.rint(dem + ELEVATION_OFFSET_M), 0, 65535).astype(np.uint16)
    Image.fromarray(np.ascontiguousarray(packed[::-1])).save(dem_path, optimize=True)
    Image.fromarray(np.ascontiguousarray(np.rint(np.clip(coverage, 0, 1) * 255).astype(np.uint8)[::-1])).save(
        mask_path, optimize=True)


def load_rasters(name: str) -> tuple[np.ndarray, np.ndarray] | None:
    dem_path, mask_path = raster_paths(name)
    if not (dem_path.exists() and mask_path.exists()):
        return None
    with Image.open(dem_path) as image:
        dem = np.asarray(image, dtype=np.float64)[::-1] - ELEVATION_OFFSET_M
    with Image.open(mask_path) as image:
        coverage = np.asarray(image, dtype=np.float64)[::-1] / 255.0
    return np.ascontiguousarray(dem), np.ascontiguousarray(coverage)

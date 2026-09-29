"""Validadores comunes de los mapas volumetricos (los usan los generadores y los tests):

  - transitabilidad sobre la cota superior (`top`, una muestra por metro): se anda por lo seco con desnivel
    <= tan(pendiente maxima) entre muestras vecinas; una pasarela o calzada vale si une sus extremos con un ancho
    minimo (el suelo transitable erosionado por un disco de ese diametro sigue conectado);
  - islas: toda masa de tierra seca de mas de min_area_m2 se alcanza a pie desde el inicio, salvo las marcadas;
  - presupuesto de malla: triangulos y MB de StaticMesh por mapa (Catalogo de mapas §4.1 por modo; si no hay
    modo, Plan maestro §2.5);
  - sin grietas: los vertices del borde de cada trozo coinciden con los del vecino (lo que lee el juego).

Todo en 2,5D sobre `top` (lo alto de cada columna): los puentes naturales cuentan por su tablero. Lo que haya
debajo de un voladizo no se valida aqui (para eso esta gen_terrain_volume.walk sobre el volumen).
"""

from __future__ import annotations

import json
import math
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from scipy import ndimage, sparse
from scipy.sparse import csgraph
from scipy.spatial import cKDTree

from .layout import CELL_M, UU_PER_M, WATER_M

DRY_M = WATER_M + 0.1                    # por debajo es agua: no se anda
WALK_SLOPE_DEG = 45.0                    # la tortuga sube 1 m por metro (CLIMB_M de gen_terrain_spain)
TRI_PER_KM2_TARGET = 1_000_000           # objetivo del Plan §2.5 (a validar en F6b; pide decimado)
MIN_TRIANGLES = 1_100_000                # tope de un mapa de 600 m (aceptacion de CP01; C01 = 1,06 M)
EDITOR_MB_PER_MTRI = 17.0                # StaticMesh de editor [A §3]
SEAM_TOLERANCE_UU = 0.5                  # la cuantizacion de TNTM2 mueve un vertice como mucho ~0,15 uu


# ── Grafo de lo transitable ──────────────────────────────────────────────────────
def _edges(top: np.ndarray, ok: np.ndarray, max_step_m: float) -> sparse.csr_matrix:
    n = top.size
    index = np.arange(n).reshape(top.shape)
    rows, cols = [], []
    for a, b, ta, tb, oa, ob in ((index[:-1, :], index[1:, :], top[:-1, :], top[1:, :], ok[:-1, :], ok[1:, :]),
                                 (index[:, :-1], index[:, 1:], top[:, :-1], top[:, 1:], ok[:, :-1], ok[:, 1:])):
        keep = oa & ob & (np.abs(ta - tb) <= max_step_m)
        rows.append(a[keep])
        cols.append(b[keep])
    r, c = np.concatenate(rows), np.concatenate(cols)
    return sparse.coo_matrix((np.ones(len(r), dtype=np.int8), (r, c)), shape=(n, n)).tocsr()


def components(top: np.ndarray, ok: np.ndarray, max_slope_deg: float = WALK_SLOPE_DEG) -> np.ndarray:
    """Etiqueta de componente a pie de cada muestra (-1 donde no se puede estar)."""
    graph = _edges(top, ok, math.tan(math.radians(max_slope_deg)))
    _, labels = csgraph.connected_components(graph, directed=False)
    labels = labels.reshape(top.shape)
    return np.where(ok, labels, -1)


def reachable(top: np.ndarray, start: tuple[int, int], max_slope_deg: float = WALK_SLOPE_DEG,
              dry_m: float = DRY_M) -> np.ndarray:
    """Muestras alcanzables a pie desde start (seco, desnivel acotado)."""
    dry = top > dry_m
    if not dry[start]:
        return np.zeros(top.shape, dtype=bool)
    labels = components(top, dry, max_slope_deg)
    return labels == labels[start]


def local_slope_deg(top: np.ndarray, valid: np.ndarray | None = None) -> np.ndarray:
    """Pendiente de cada muestra: el mayor desnivel con sus 4 vecinas (m por m), en grados. Con `valid`, solo
    cuenta el desnivel entre muestras validas (el borde de una meseta sobre el agua no es pendiente: es orilla)."""
    valid = np.ones(top.shape, dtype=bool) if valid is None else valid
    step = np.zeros(top.shape)
    for axis in (0, 1):
        diff = np.abs(np.diff(top, axis=axis))
        both = np.logical_and(np.take(valid, range(0, top.shape[axis] - 1), axis=axis),
                              np.take(valid, range(1, top.shape[axis]), axis=axis))
        diff = np.where(both, diff, 0.0)
        lo = [slice(None), slice(None)]
        hi = [slice(None), slice(None)]
        lo[axis], hi[axis] = slice(0, -1), slice(1, None)
        step[tuple(lo)] = np.maximum(step[tuple(lo)], diff)
        step[tuple(hi)] = np.maximum(step[tuple(hi)], diff)
    return np.degrees(np.arctan(step))


def gradient_slope_deg(top: np.ndarray, valid: np.ndarray | None = None) -> np.ndarray:
    """Pendiente de cada muestra por diferencias centradas entre muestras validas (una sola si la otra no lo
    es), en grados: la pendiente del terreno, sin el salto de una orilla o de un borde."""
    valid = np.ones(top.shape, dtype=bool) if valid is None else valid
    grads = []
    for axis in (0, 1):
        fwd = np.zeros(top.shape)
        bwd = np.zeros(top.shape)
        ok_f = np.zeros(top.shape, dtype=bool)
        ok_b = np.zeros(top.shape, dtype=bool)
        lo = [slice(None), slice(None)]
        hi = [slice(None), slice(None)]
        lo[axis], hi[axis] = slice(0, -1), slice(1, None)
        diff = np.diff(top, axis=axis)
        fwd[tuple(lo)], ok_f[tuple(lo)] = diff, valid[tuple(hi)]
        bwd[tuple(hi)], ok_b[tuple(hi)] = diff, valid[tuple(lo)]
        both = ok_f & ok_b
        g = np.where(both, 0.5 * (fwd + bwd), np.where(ok_f, fwd, np.where(ok_b, bwd, 0.0)))
        grads.append(g)
    return np.degrees(np.arctan(np.hypot(*grads)))


def slope_stats(top: np.ndarray, mask: np.ndarray) -> dict[str, float]:
    slopes = gradient_slope_deg(top, mask)[mask]
    if len(slopes) == 0:
        return {"p50": 0.0, "p90": 0.0, "max": 0.0}
    return {"p50": float(np.percentile(slopes, 50)), "p90": float(np.percentile(slopes, 90)), "max": float(slopes.max())}


def disk(radius: float) -> np.ndarray:
    r = int(math.floor(radius))
    y, x = np.mgrid[-r:r + 1, -r:r + 1]
    return x * x + y * y <= radius * radius + 1e-9


def wide_ground(top: np.ndarray, min_width_m: float, max_slope_deg: float = WALK_SLOPE_DEG,
                dry_m: float = DRY_M) -> np.ndarray:
    """Muestras donde cabe un disco de min_width_m de diametro de suelo seco sin mas pendiente que max_slope_deg
    respecto al centro (cada muestra del disco, a distancia d, no se separa del centro mas de tan * d)."""
    dry = top > dry_m
    radius = max(0.0, (min_width_m - 1.0) / 2.0)
    tan = math.tan(math.radians(max_slope_deg)) + 1e-9
    ok = dry.copy()
    offsets = np.argwhere(disk(radius)) - int(math.floor(radius))
    ni, nj = top.shape
    for di, dj in offsets:
        if di == 0 and dj == 0:
            continue
        shifted_top = np.full(top.shape, np.nan)
        src_i = slice(max(0, di), ni + min(0, di))
        src_j = slice(max(0, dj), nj + min(0, dj))
        dst_i = slice(max(0, -di), ni + min(0, -di))
        dst_j = slice(max(0, -dj), nj + min(0, -dj))
        shifted_top[dst_i, dst_j] = np.where(dry[src_i, src_j], top[src_i, src_j], np.nan)
        with np.errstate(invalid="ignore"):
            ok &= np.abs(shifted_top - top) <= tan * math.hypot(di, dj)
    return ok


def _nearest(mask: np.ndarray, point: tuple[int, int], search_m: int) -> tuple[int, int] | None:
    i, j = point
    i0, j0 = max(0, i - search_m), max(0, j - search_m)
    window = mask[i0:i + search_m + 1, j0:j + search_m + 1]
    hits = np.argwhere(window)
    if len(hits) == 0:
        return None
    k = int(np.argmin(((hits + [i0, j0] - [i, j]) ** 2).sum(axis=1)))
    return int(hits[k][0] + i0), int(hits[k][1] + j0)


def corridor_ok(top: np.ndarray, a: tuple[int, int], b: tuple[int, int], min_width_m: float,
                max_slope_deg: float = WALK_SLOPE_DEG, search_m: int = 6) -> bool:
    """a y b se unen por suelo transitable de min_width_m de ancho (pasarela, puente o calzada). Los extremos
    pueden caer hasta search_m del suelo ancho (el borde de una meseta)."""
    wide = wide_ground(top, min_width_m, max_slope_deg)
    pa, pb = _nearest(wide, a, search_m), _nearest(wide, b, search_m)
    if pa is None or pb is None:
        return False
    labels = components(top, wide, max_slope_deg)
    return labels[pa] == labels[pb]


# ── Islas ────────────────────────────────────────────────────────────────────────
def unreachable_islands(top: np.ndarray, start: tuple[int, int], marked: tuple[tuple[int, int], ...] = (),
                        min_area_m2: float = 25.0, max_slope_deg: float = WALK_SLOPE_DEG) -> list[dict]:
    """Masas de tierra seca (>= min_area_m2) a las que no se llega a pie desde start y que no contienen ninguna
    muestra marcada. Cada una como {"center": (i, j), "area_m2": ...}."""
    dry = top > DRY_M
    seen = reachable(top, start, max_slope_deg)
    land, count = ndimage.label(dry)
    out = []
    marked_labels = {int(land[p]) for p in marked if 0 <= p[0] < top.shape[0] and 0 <= p[1] < top.shape[1]}
    for k in range(1, count + 1):
        mask = land == k
        area = float(mask.sum())
        if area < min_area_m2 or seen[mask].any() or k in marked_labels:
            continue
        i, j = (int(v) for v in np.argwhere(mask).mean(axis=0))
        out.append({"center": (i, j), "area_m2": area})
    return out


# ── Malla ────────────────────────────────────────────────────────────────────────
@dataclass(frozen=True)
class MeshBudget:
    """Tope de triangulos y de MB de StaticMesh de editor (EDITOR_MB_PER_MTRI) por mapa."""
    max_triangles: int
    max_editor_mb: float

    @classmethod
    def for_grid(cls, grid: int, cell_m: float = CELL_M) -> "MeshBudget":
        """Plan §2.5: 1,1 M triangulos por mapa de hasta 600 m (CP01) y 1 M por km2 por encima."""
        area_km2 = (grid * cell_m / 1000.0) ** 2
        tris = int(max(MIN_TRIANGLES, TRI_PER_KM2_TARGET * area_km2))
        return cls(tris, round(tris / 1e6 * EDITOR_MB_PER_MTRI, 1))

    @classmethod
    def for_mode(cls, mode: str | None, grid: int) -> "MeshBudget":
        """Catalogo de mapas §4.1: Rally <= 1,2 M triangulos y 21 MB; TcT <= 0,35 M y 6 MB; si no, el del Plan."""
        return MODE_BUDGETS.get(mode or "", None) or cls.for_grid(grid)


MODE_BUDGETS = {"rally": MeshBudget(1_200_000, 21.0), "tct": MeshBudget(350_000, 6.0)}


def map_stats(map_dir: Path) -> dict:
    manifest = json.loads((map_dir / "manifest.json").read_text(encoding="utf-8"))
    tris = sum(int(c["triangles"]) for c in manifest["cells"])
    chunk_mb = sum((map_dir / c["file"]).stat().st_size for c in manifest["cells"]) / (1024 * 1024)
    total_mb = sum(f.stat().st_size for f in map_dir.rglob("*") if f.is_file()) / (1024 * 1024)
    grid = int(manifest.get("grid", 6))
    area_km2 = (grid * manifest["cell_uu"] / UU_PER_M / 1000.0) ** 2
    return {"grid": grid, "mode": manifest.get("mode"), "triangles": tris, "chunk_mb": round(chunk_mb, 2), "total_mb": round(total_mb, 2),
            "tri_per_km2": round(tris / area_km2), "editor_mb_est": round(tris / 1e6 * EDITOR_MB_PER_MTRI, 1)}


def check_budget(map_dir: Path, budget: MeshBudget | None = None) -> dict:
    stats = map_stats(map_dir)
    budget = budget or MeshBudget.for_mode(stats.get("mode"), stats["grid"])
    ok = stats["triangles"] <= budget.max_triangles and stats["editor_mb_est"] <= budget.max_editor_mb
    return {**stats, "budget_triangles": budget.max_triangles, "budget_editor_mb": budget.max_editor_mb, "ok": ok}


def _world_vertices(map_dir: Path, cell: dict) -> np.ndarray:
    from .export import read_chunk
    verts = read_chunk(map_dir / cell["file"])["vertices"].astype(np.float64)
    return verts + np.array([cell["center_uu"][0], cell["center_uu"][1], 0.0])


def check_seams(map_dir: Path, tolerance_uu: float = SEAM_TOLERANCE_UU) -> dict:
    """Mayor distancia (uu) entre un vertice del borde de un trozo y el mas cercano del borde del vecino, en
    las dos direcciones; un borde con vertices solo en un lado es una grieta."""
    manifest = json.loads((map_dir / "manifest.json").read_text(encoding="utf-8"))
    # Solo los trozos de la rejilla del mapa: la corona de fondo (sin col/row) no se suelda con ellos.
    cells = {(c["col"], c["row"]): c for c in manifest["cells"] if "col" in c and "row" in c}
    half = manifest["cell_uu"] / 2.0
    world = {key: _world_vertices(map_dir, c) for key, c in cells.items()}
    worst, cracks = 0.0, []
    for (col, row), cell in cells.items():
        for axis, neighbour in ((1, (col + 1, row)), (0, (col, row + 1))):      # Este (Y) y Norte (X)
            if neighbour not in cells:
                continue
            edge = cell["center_uu"][axis] + half
            a, b = world[(col, row)], world[neighbour]
            ea = a[np.abs(a[:, axis] - edge) < tolerance_uu][:, [1 - axis, 2]]
            eb = b[np.abs(b[:, axis] - edge) < tolerance_uu][:, [1 - axis, 2]]
            if len(ea) == 0 and len(eb) == 0:
                continue
            if len(ea) == 0 or len(eb) == 0:
                cracks.append(((col, row), neighbour))
                continue
            gap = max(float(cKDTree(eb).query(ea)[0].max()), float(cKDTree(ea).query(eb)[0].max()))
            worst = max(worst, gap)
            if gap > tolerance_uu:
                cracks.append(((col, row), neighbour))
    return {"max_gap_uu": worst, "cracks": cracks, "ok": not cracks}


# ── Informe comun ────────────────────────────────────────────────────────────────
def evaluate_map(top: np.ndarray, map_dir: Path | None, start: tuple[int, int], end: tuple[int, int] | None = None,
                 required: tuple[tuple[int, int], ...] = (), corridors: tuple[tuple[tuple[int, int], tuple[int, int]], ...] = (),
                 marked: tuple[tuple[int, int], ...] = (), min_width_m: float = 3.0,
                 max_slope_deg: float = WALK_SLOPE_DEG, min_island_m2: float = 25.0,
                 budget: MeshBudget | None = None) -> dict:
    """Todas las comprobaciones de un mapa: final y puntos requeridos a pie desde el inicio, pasarelas
    (corridors) con min_width_m de ancho, sin islas inalcanzables salvo las marcadas y, si hay map_dir,
    presupuesto de malla y costuras. `ok` solo si todo pasa."""
    seen = reachable(top, start, max_slope_deg)
    report: dict = {"start_dry": bool(top[start] > DRY_M), "walkable_share": float(seen.sum() / max((top > DRY_M).sum(), 1))}
    report["end_reached"] = bool(seen[end]) if end is not None else True
    report["required_missed"] = [p for p in required if not seen[p]]
    report["corridors_failed"] = [c for c in corridors if not corridor_ok(top, c[0], c[1], min_width_m, max_slope_deg)]
    report["unreachable_islands"] = unreachable_islands(top, start, marked, min_island_m2, max_slope_deg)
    report["slope_deg"] = slope_stats(top, seen)
    ok = report["start_dry"] and report["end_reached"] and not report["required_missed"] \
        and not report["corridors_failed"] and not report["unreachable_islands"]
    if map_dir is not None:
        report["budget"] = check_budget(map_dir, budget)
        report["seams"] = check_seams(map_dir)
        ok = ok and report["budget"]["ok"] and report["seams"]["ok"]
    report["ok"] = bool(ok)
    return report

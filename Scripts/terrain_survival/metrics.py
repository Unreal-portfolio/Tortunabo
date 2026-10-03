"""Métricas comunes de un mapa de Supervivencia. Usa los validadores de terrain_vol (mismo criterio de
transitabilidad que los mapas de Coop) y mide si un mapa cumple la especificación: que se pueda recorrer, cuánto
cuesta generarlo y si la dificultad se nota."""

from __future__ import annotations

import math

import numpy as np
from scipy import ndimage, sparse, stats
from scipy.sparse import csgraph

from terrain_vol.validate import DRY_M, WALK_SLOPE_DEG, components, slope_stats, wide_ground

from . import spec
from .mapa import SurvivalMap, expected_shape

STEEP_DEG = 25.0                      # a partir de aquí una pendiente cuenta como reto en el camino
OFF_ROUTE_M = 10.0                    # más lejos que esto del camino más corto, el suelo es «fuera del camino»
_NEIGHBOURS = ((0, 1), (1, 0), (1, 1), (1, -1))


Jump = tuple[tuple[int, int], tuple[int, int]]


def usable_jumps(m: SurvivalMap) -> list[Jump]:
    """Huecos de salto que la tortuga cruza: su salto más largo cabe en el dive (spec.MAX_JUMP_M)."""
    h, w = m.top.shape
    out = []
    for r0, c0, r1, c1, leap in np.asarray(m.jumps).reshape(-1, 5):
        a, b = (int(r0), int(c0)), (int(r1), int(c1))
        if leap <= spec.MAX_JUMP_M and all(0 <= p[0] < h and 0 <= p[1] < w for p in (a, b)):
            out.append((a, b))
    return out


def reachable(top: np.ndarray, start: tuple[int, int], jumps: list[Jump] = (), max_slope_deg: float = WALK_SLOPE_DEG,
              dry_m: float = DRY_M) -> np.ndarray:
    """Muestras alcanzables desde start a pie (seco, desnivel acotado) o saltando los huecos `jumps`."""
    dry = top > dry_m
    if not dry[start]:
        return np.zeros(top.shape, dtype=bool)
    labels = components(top, dry, max_slope_deg)
    links: dict[int, set[int]] = {}
    for a, b in jumps:
        if dry[a] and dry[b]:
            links.setdefault(int(labels[a]), set()).add(int(labels[b]))
            links.setdefault(int(labels[b]), set()).add(int(labels[a]))
    seen, queue = {int(labels[start])}, [int(labels[start])]
    while queue:
        for nxt in links.get(queue.pop(), ()):
            if nxt not in seen:
                seen.add(nxt)
                queue.append(nxt)
    return np.isin(labels, list(seen))


def route(top: np.ndarray, ok: np.ndarray, start: tuple[int, int], goal: tuple[int, int],
          max_slope_deg: float = WALK_SLOPE_DEG, jumps: list[Jump] = ()) -> list[tuple[int, int]] | None:
    """Camino más corto (longitud 3D) de start a goal por las muestras `ok`, a pie o saltando `jumps`; None si no hay."""
    h, w = top.shape
    if not (ok[start] and ok[goal]):
        return None
    index = np.arange(h * w).reshape(h, w)
    tan = math.tan(math.radians(max_slope_deg))
    rows, cols, dist = [], [], []
    for di, dj in _NEIGHBOURS:
        a = (slice(0, h - di), slice(max(0, -dj), w - max(0, dj)))
        b = (slice(di, h), slice(max(0, dj), w - max(0, -dj)))
        run = math.hypot(di, dj)
        dz = np.abs(top[a] - top[b])
        keep = ok[a] & ok[b] & (dz <= tan * run)
        rows.append(index[a][keep])
        cols.append(index[b][keep])
        dist.append(np.hypot(run, dz[keep]))
    for a, b in jumps:
        if ok[a] and ok[b]:
            rows.append(np.array([index[a]]))
            cols.append(np.array([index[b]]))
            dist.append(np.array([math.dist((*a, top[a]), (*b, top[b]))]))
    r, c, d = np.concatenate(rows), np.concatenate(cols), np.concatenate(dist)
    graph = sparse.coo_matrix((d, (r, c)), shape=(h * w, h * w)).tocsr()
    s, g = start[0] * w + start[1], goal[0] * w + goal[1]
    cost, pred = csgraph.dijkstra(graph, directed=False, indices=s, return_predecessors=True)
    if not np.isfinite(cost[g]):
        return None
    path, k = [], g
    while k != s:
        path.append((int(k // w), int(k % w)))
        k = pred[k]
    path.append(start)
    return path[::-1]


def route_length(top: np.ndarray, path: list[tuple[int, int]]) -> float:
    pts = np.array([(i, j, top[i, j]) for i, j in path], dtype=float)
    return float(np.linalg.norm(np.diff(pts, axis=0), axis=1).sum())


def _is_jump(p: tuple[int, int], q: tuple[int, int]) -> bool:
    return max(abs(p[0] - q[0]), abs(p[1] - q[1])) > 1


def _path_slopes(top: np.ndarray, path: list[tuple[int, int]]) -> list[float]:
    """Pendiente de cada paso a pie (los saltos no son pendiente)."""
    return [math.degrees(math.atan(abs(top[i1, j1] - top[i0, j0]) / math.hypot(i1 - i0, j1 - j0)))
            for (i0, j0), (i1, j1) in zip(path, path[1:]) if not _is_jump((i0, j0), (i1, j1))]


def off_route_share(seen: np.ndarray, path: list[tuple[int, int]]) -> float:
    """Parte del suelo alcanzable a más de OFF_ROUTE_M del camino más corto: 0 = lineal (todo es camino), cerca de
    1 = laberinto (ramas, lazos y zonas que no llevan a la meta). Informativa, y solo vale con el borde cerrado:
    un campo abierto también la sube."""
    on_path = np.zeros(seen.shape, dtype=bool)
    on_path[tuple(np.array(path).T)] = True
    far = ndimage.distance_transform_edt(~on_path) > OFF_ROUTE_M
    return float((far & seen).sum() / max(seen.sum(), 1))


def evaluate(m: SurvivalMap) -> dict:
    """Informe de un mapa. `valid` solo si cumple toda la especificación; `challenge` (informativo) es lo que
    se compara entre dificultades."""
    top = m.top
    r: dict = {"algorithm": m.algorithm, "seed": m.seed, "difficulty": m.difficulty, "gen_seconds": m.gen_seconds}
    r["shape_ok"] = tuple(top.shape) == expected_shape() and spec.LENGTH_M / spec.WIDTH_M >= spec.MIN_ASPECT
    r["triangles"] = m.triangle_count()
    r["triangles_ok"] = r["triangles"] <= spec.MAX_TRIANGLES
    r["time_ok"] = m.gen_seconds <= spec.MAX_GEN_SECONDS
    h, w = top.shape
    inside = all(0 <= p[0] < h and 0 <= p[1] < w for p in (m.start, m.goal))
    r["ends_ok"] = bool(inside and m.start[1] <= spec.START_MARGIN_M + 2 and m.goal[1] >= w - 1 - spec.START_MARGIN_M - 2
                        and top[m.start] > DRY_M and top[m.goal] > DRY_M)
    jumps = usable_jumps(m)
    seen = reachable(top, m.start, jumps) if r["ends_ok"] else np.zeros(top.shape, dtype=bool)
    r["reached"] = bool(r["ends_ok"] and seen[m.goal])
    path = route(top, wide_ground(top, spec.MIN_PATH_WIDTH_M), m.start, m.goal, jumps=jumps) if r["ends_ok"] else None
    r["wide_path"] = path is not None
    if path:
        length = route_length(top, path)
        straight = float(np.hypot(m.goal[0] - m.start[0], m.goal[1] - m.start[1]))
        steep = float(np.mean([s > STEEP_DEG for s in _path_slopes(top, path)] or [0.0]))
        leaps = sum(_is_jump(p, q) for p, q in zip(path, path[1:]))
        r.update(route_m=length, route_ratio=length / max(straight, 1.0), steep_share=steep, jumps_on_route=leaps)
        r["challenge"] = (length / max(straight, 1.0) - 1.0) + 2.0 * steep + spec.JUMP_WEIGHT * leaps * 100.0 / length
        r["off_route_share"] = off_route_share(seen, path)
    else:
        r.update(route_m=None, route_ratio=None, steep_share=None, jumps_on_route=None, challenge=None,
                 off_route_share=None)
    r["walkable_share"] = float(seen.sum() / max((top > DRY_M).sum(), 1))
    r["slope_deg"] = slope_stats(top, seen) if seen.any() else None
    r["valid"] = bool(r["shape_ok"] and r["ends_ok"] and r["reached"] and r["wide_path"] and r["triangles_ok"]
                      and r["time_ok"])
    return r


def variety(tops: list[np.ndarray]) -> float:
    """Diferencia RMS media entre pares de mapas de la misma dificultad, dividida por su rango de cotas: 0 = todos
    iguales («nuevo cada vez» pide más de MIN_VARIETY)."""
    if len(tops) < 2:
        return 0.0
    scale = max(float(np.ptp(np.stack(tops))), 1e-6)
    diffs = [math.sqrt(float(np.mean((a - b) ** 2))) / scale for k, a in enumerate(tops) for b in tops[k + 1:]]
    return float(np.mean(diffs))


def difficulty_rank(reports: list[dict]) -> float | None:
    """Spearman entre la dificultad pedida y el reto medido (None si no se puede calcular)."""
    pairs = [(r["difficulty"], r["challenge"]) for r in reports if r.get("challenge") is not None]
    if len({d for d, _ in pairs}) < 2 or len({c for _, c in pairs}) < 2:
        return None
    return float(stats.spearmanr([d for d, _ in pairs], [c for _, c in pairs]).statistic)

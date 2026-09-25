"""Redes de caminos organicas para las zonas laberinticas (acantilados, dunas, algas).

Nodos repartidos con Poisson-disk y unidos por la triangulacion de Delaunay; de ahi sale un
arbol aleatorio que cuelga de la CADENA (los nodos sobre la ruta principal) mas algunos lazos.
Si los lazos se quedan dentro de su rama, la unica forma de cruzar la zona es la cadena: las
demas ramas son callejones o vueltas que devuelven al mismo punto.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field

import numpy as np
from scipy.spatial import Delaunay, cKDTree

from terrain_gen.features import chaikin

from .layout import MAP_MAX_M, MAP_MIN_M
from .route import ZONES, ZoneField

NET_EDGE_MARGIN_M = 25.0        # los nodos no se acercan mas al borde del mapa


@dataclass
class Network:
    zone: str
    nodes: np.ndarray                               # (N, 2)
    chain: list[int]                                # nodos de la ruta, en orden
    edges: list[tuple[int, int]]
    curves: list[np.ndarray]                        # polilinea de cada arista
    attach: dict[int, int] = field(default_factory=dict)   # nodo -> nodo de la cadena del que cuelga

    def degree(self) -> np.ndarray:
        deg = np.zeros(len(self.nodes), dtype=int)
        for a, b in self.edges:
            deg[a] += 1
            deg[b] += 1
        return deg

    def dead_ends(self) -> list[int]:
        """Hojas fuera de la cadena: callejones sin salida."""
        chain = set(self.chain)
        return [k for k, d in enumerate(self.degree()) if d == 1 and k not in chain]

    def connected(self, a: int, b: int, without: tuple[int, int] | None = None) -> bool:
        adjacency: dict[int, list[int]] = {}
        for e in self.edges:
            if without is not None and {e[0], e[1]} == set(without):
                continue
            adjacency.setdefault(e[0], []).append(e[1])
            adjacency.setdefault(e[1], []).append(e[0])
        seen, stack = {a}, [a]
        while stack:
            n = stack.pop()
            for m in adjacency.get(n, []):
                if m not in seen:
                    seen.add(m)
                    stack.append(m)
        return b in seen

    def sample(self, step: float = 0.5) -> np.ndarray:
        """Puntos a lo largo de todas las aristas (para medir distancias con un KD-tree)."""
        out = []
        for curve in self.curves:
            seg = np.linalg.norm(np.diff(curve, axis=0), axis=1)
            arc = np.concatenate(([0.0], np.cumsum(seg)))
            s = np.arange(0.0, arc[-1] + step, step)
            out.append(np.stack([np.interp(s, arc, curve[:, 0]), np.interp(s, arc, curve[:, 1])], axis=1))
        return np.vstack(out) if out else np.zeros((0, 2))


def poisson_disk(rng: np.random.Generator, bounds, radius: float, accept, seeds: np.ndarray, tries: int = 24) -> np.ndarray:
    """Bridson: puntos a distancia >= radius dentro de bounds (x0, x1, y0, y1) que cumplen accept."""
    x0, x1, y0, y1 = bounds
    points = [p for p in seeds]
    active = list(range(len(points)))
    if not active:
        start = np.array([rng.uniform(x0, x1), rng.uniform(y0, y1)])
        points.append(start)
        active.append(0)
    while active:
        k = active[int(rng.integers(len(active)))]
        base = points[k]
        placed = False
        tree = cKDTree(np.array(points))
        for _ in range(tries):
            angle = rng.uniform(0.0, 2.0 * math.pi)
            dist = rng.uniform(radius, 2.0 * radius)
            cand = base + dist * np.array([math.cos(angle), math.sin(angle)])
            if not (x0 <= cand[0] <= x1 and y0 <= cand[1] <= y1):
                continue
            if tree.query(cand)[0] < radius or not accept(cand):
                continue
            points.append(cand)
            active.append(len(points) - 1)
            tree = cKDTree(np.array(points))
            placed = True
            break
        if not placed:
            active.remove(k)
    return np.array(points[len(seeds):]) if len(points) > len(seeds) else np.zeros((0, 2))


def bent_curve(rng: np.random.Generator, a: np.ndarray, b: np.ndarray, bend: float) -> np.ndarray:
    d = b - a
    length = float(np.linalg.norm(d))
    if length < 1e-6:
        return np.stack([a, b])
    perp = np.array([-d[1], d[0]]) / length
    mid = (a + b) / 2.0 + perp * rng.uniform(-bend, bend) * length
    pts = chaikin((tuple(a), tuple(a), tuple(mid), tuple(b), tuple(b)), 3)
    return np.array(pts)


def build_network(rng: np.random.Generator, zones: ZoneField, zone: str, spacing: float,
                  loop_share: float, loops_anywhere: bool = False, reach_share: float = 0.85,
                  bend: float = 0.22) -> Network:
    """Red de la zona. loops_anywhere=False: los lazos no saltan de una rama a otra, asi que la
    cadena es el unico paso de la entrada a la salida de la zona."""
    route = zones.route
    reach = next(r for name, _, r in ZONES if name == zone) * reach_share
    s0, s1 = route.zone_range(zone)
    chain_s = np.arange(max(0.0, s0 - spacing * 0.5), min(route.length, s1 + spacing * 0.5), spacing)
    chain_s = np.append(chain_s, min(route.length, s1 + spacing * 0.5))
    chain_pts = np.array([route.point_at(s) for s in chain_s])

    inside = route.points[(route.arc >= s0 - reach) & (route.arc <= s1 + reach)]
    lo, hi = MAP_MIN_M + NET_EDGE_MARGIN_M, MAP_MAX_M - NET_EDGE_MARGIN_M
    bounds = (max(lo, inside[:, 0].min() - reach), min(hi, inside[:, 0].max() + reach),
              max(lo, inside[:, 1].min() - reach), min(hi, inside[:, 1].max() + reach))

    def accept(p):
        d, s = zones.nearest(np.array([p[0]]), np.array([p[1]]))
        x, y = np.array([p[0]]), np.array([p[1]])
        w = zones.weights(x, y, s, d)[zone][0]
        return w > 0.55 and d[0] < min(reach, 0.9 * zones.reach(zones.weights(x, y, s, d), x, y)[0])

    extra = poisson_disk(rng, bounds, spacing, accept, chain_pts)
    nodes = np.vstack([chain_pts, extra]) if len(extra) else chain_pts
    chain = list(range(len(chain_pts)))
    chain_set = set(chain)

    candidates = set()
    if len(nodes) >= 3:
        tri = Delaunay(nodes)
        for simplex in tri.simplices:
            for u in range(3):
                a, b = sorted((int(simplex[u]), int(simplex[(u + 1) % 3])))
                if np.linalg.norm(nodes[a] - nodes[b]) < 1.8 * spacing:
                    candidates.add((a, b))
    edges = [(chain[k], chain[k + 1]) for k in range(len(chain) - 1)]
    attach = {c: c for c in chain}
    # Arbol aleatorio (Prim con eleccion al azar) que crece desde la cadena.
    while True:
        frontier = [(a, b) for a, b in candidates if (a in attach) != (b in attach)]
        if not frontier:
            break
        a, b = frontier[int(rng.integers(len(frontier)))]
        new, old = (b, a) if a in attach else (a, b)
        attach[new] = attach[old]
        edges.append((old, new))
    in_tree = {tuple(sorted(e)) for e in edges}
    spare = [(a, b) for a, b in candidates if (a, b) not in in_tree and a in attach and b in attach
             and not (a in chain_set and b in chain_set)
             and (loops_anywhere or attach[a] == attach[b])]
    rng.shuffle(spare)
    edges += spare[:int(round(loop_share * len(attach)))]

    # Nodos sin conectar (fuera de la triangulacion util): fuera, y se reindexa.
    keep = sorted(attach)
    remap = {old: new for new, old in enumerate(keep)}
    nodes = nodes[keep]
    edges = [(remap[a], remap[b]) for a, b in edges]
    chain = [remap[c] for c in chain]
    attach = {remap[k]: remap[v] for k, v in attach.items()}

    curves = []
    chain_pairs = {(chain[k], chain[k + 1]) for k in range(len(chain) - 1)}
    for a, b in edges:
        if (a, b) in chain_pairs:
            # Tramo de la cadena: sigue la ruta exacta entre sus dos arcos.
            ka, kb = chain.index(a), chain.index(b)
            sa, sb = chain_s[ka], chain_s[kb]
            mask = (route.arc >= sa) & (route.arc <= sb)
            curve = np.vstack([nodes[a], route.points[mask], nodes[b]])
        else:
            curve = bent_curve(rng, nodes[a], nodes[b], bend)
        curves.append(curve)
    return Network(zone, nodes, chain, edges, curves, attach)

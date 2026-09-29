"""Trazado Rally de un pais: calzada principal, tuneles bajo montaña real que forman lazos con la calzada vieja,
y puentes sobre valles (tableros de terrain_path.bridge sobre un rio tallado). Validacion en 3D de los tuneles.

  - Tunel + lazo: un atajo recto entre dos puntos de la calzada que pasa bajo montaña (cobertura >= ROOF_M sobre el
    galibo); la calzada vieja se conserva, asi que atajo y calzada vieja cierran un circuito.
  - Puente de valle: en un minimo del terreno a lo largo de la calzada se talla un rio perpendicular (agua de
    muerte) y la calzada lo cruza por un tablero Deck de terrain_path (losa con estribos, aire debajo).
Coordenadas: rejilla de 1 m de global_top (indices i Norte, j Este desde MAP_MIN_M) y metros de juego (X, Y).
"""

from __future__ import annotations

import math
from dataclasses import dataclass

import numpy as np
from scipy import ndimage

from terrain_path.bridge import Deck, DeckSet
from terrain_path.graph import PathLine
from terrain_vol.density import smooth
from terrain_vol.layout import MAP_MIN_M, WATER_M
from terrain_vol.noise import ValueNoise3D

from .country import CountryModel, Road, lipschitz
from .region import RASTER_PX_M

CLEAR_M = 6.0                   # galibo del tunel
TUNNEL_HALF_M = 7.0             # semiancho del tunel (14 m)
ROOF_M = 2.0                    # roca minima sobre el galibo para que sea tunel y no trinchera
DECK_HALF_M = 6.0               # tablero de 12 m (DeckSet busca hasta 6 m del eje)
RIVER_HALF_M = 8.0
GRADE = math.tan(math.radians(9.0))


@dataclass
class Tunnel:
    pts: np.ndarray             # (n, 2) (X, Y) cada 1 m
    floor: np.ndarray           # cota del suelo (m)
    covered: np.ndarray         # bool: bajo roca (tunel) o a cielo abierto (boca en trinchera)

    @property
    def length(self) -> float:
        return float(self.covered.sum())

    def void(self, X: np.ndarray, Y: np.ndarray, Z: np.ndarray) -> np.ndarray | None:
        """> 0 dentro del hueco del tunel (solo en el tramo cubierto y las bocas)."""
        from scipy.spatial import cKDTree
        tree = cKDTree(self.pts)
        d, k = tree.query(np.stack([X.ravel(), Y.ravel()], axis=1), distance_upper_bound=TUNNEL_HALF_M + 1.0)
        near = np.isfinite(d)
        if not near.any():
            return None
        k = np.where(near, k, 0)
        floor = self.floor[k].reshape(X.shape)[..., None]
        side = (TUNNEL_HALF_M - np.where(near, d, 99.0)).reshape(X.shape)[..., None]
        Z3 = Z[None, None, :]
        arch = floor + CLEAR_M - 0.5 + 1.5 * np.clip(side / TUNNEL_HALF_M, 0.0, 1.0)      # boveda: 5,5 m en los lados
        return np.minimum(np.minimum(side, Z3 - floor), arch - Z3)


@dataclass
class RallyPlan:
    main: Road
    branches: list[Road]
    tunnels: list[Tunnel]
    decks: list[Deck]
    loops: list[tuple[int, int]]            # indices en main.path donde cada lazo sale y vuelve
    rivers: list[tuple[np.ndarray, np.ndarray]]   # (centro (X, Y), direccion) de cada rio tallado


def _arc(path: np.ndarray) -> np.ndarray:
    return np.concatenate([[0.0], np.cumsum(np.hypot(*np.diff(path, axis=0).T))])


def find_shortcuts(h: np.ndarray, road: Road, count: int = 2, step: int = 6) -> list[tuple[int, int, float]]:
    """(i, j, metros cubiertos) de los atajos rectos bajo montaña, sin solaparse."""
    path, prof = road.path, road.profile
    arc = _arc(path)
    found = []
    idx = np.arange(0, len(path), step)
    for a in idx:
        for b in idx[(arc[idx] - arc[a] >= 60.0) & (arc[idx] - arc[a] <= 420.0)]:
            L = float(np.hypot(*(path[b] - path[a])))
            if not 25.0 <= L <= 200.0 or arc[b] - arc[a] < 1.3 * L or abs(prof[b] - prof[a]) > GRADE * L:
                continue
            t = np.linspace(0.0, 1.0, int(L) + 1)
            line = path[a][None, :] + (path[b] - path[a])[None, :] * t[:, None]
            terrain = ndimage.map_coordinates(h, line.T, order=1)
            if (terrain < WATER_M + 0.5).any():
                continue
            floor = prof[a] + (prof[b] - prof[a]) * t
            covered = float((terrain - floor >= CLEAR_M + ROOF_M).sum())
            if covered >= 12.0:
                found.append((covered, int(a), int(b)))
    out: list[tuple[int, int, float]] = []
    for covered, a, b in sorted(found, reverse=True):
        if all(b < a2 - 30 or a > b2 + 30 for a2, b2, _ in out):
            out.append((a, b, covered))
        if len(out) >= count:
            break
    return sorted(out)


def build_tunnels(h: np.ndarray, road: Road, shortcuts) -> tuple[Road, list[Road], list[Tunnel], list[tuple[int, int]]]:
    """Calzada principal con los atajos (tuneles) y la calzada vieja de cada atajo como rama del lazo."""
    parts_p, parts_z, branches, tunnels, loops = [], [], [], [], []
    last = 0
    for a, b, _ in shortcuts:
        parts_p.append(road.path[last:a])
        parts_z.append(road.profile[last:a])
        L = int(np.hypot(*(road.path[b] - road.path[a])))
        t = np.linspace(0.0, 1.0, L + 1)
        line = road.path[a][None, :] + (road.path[b] - road.path[a])[None, :] * t[:, None]
        floor = road.profile[a] + (road.profile[b] - road.profile[a]) * t
        terrain = ndimage.map_coordinates(h, line.T, order=1)
        covered = terrain - floor >= CLEAR_M + ROOF_M
        start = sum(len(p) for p in parts_p)
        parts_p.append(np.rint(line).astype(int))
        parts_z.append(floor)
        tunnels.append(Tunnel(line + MAP_MIN_M, floor, covered))
        branches.append(Road(road.path[a:b + 1], road.profile[a:b + 1]))
        loops.append((start, start + len(line) - 1))
        last = b + 1
    parts_p.append(road.path[last:])
    parts_z.append(road.profile[last:])
    main = Road(np.concatenate(parts_p), np.concatenate(parts_z))
    return main, branches, tunnels, loops


def valley_spots(h: np.ndarray, road: Road, avoid: list[tuple[int, int]], count: int = 2, spacing: float = 150.0) -> list[int]:
    """Indices de la calzada en los minimos del terreno (valles), separados spacing m y fuera de tuneles y bordes."""
    arc = _arc(road.path)
    terrain = h[road.path[:, 0], road.path[:, 1]]
    low = ndimage.gaussian_filter1d(terrain, 8.0) - ndimage.gaussian_filter1d(terrain, 60.0)
    order = np.argsort(low)
    chosen: list[int] = []
    for k in order:
        if arc[k] < 60.0 or arc[-1] - arc[k] < 60.0 or any(a - 30 <= k <= b + 30 for a, b in avoid):
            continue
        if all(abs(arc[k] - arc[c]) >= spacing for c in chosen):
            chosen.append(int(k))
        if len(chosen) >= count:
            break
    return chosen


def valley_deck(road: Road, k: int) -> tuple[Deck, np.ndarray, np.ndarray]:
    """Tablero Deck sobre el rio tallado en road.path[k] y el eje (centro, direccion) del rio."""
    arc = _arc(road.path)
    sel = np.abs(arc - arc[k]) <= RIVER_HALF_M + 10.0
    pts = road.path[sel] + MAP_MIN_M
    along = arc[sel] - arc[k]
    tangent = road.path[min(k + 3, len(road.path) - 1)] - road.path[max(k - 3, 0)]
    tangent = tangent / max(float(np.linalg.norm(tangent)), 1e-9)
    deck = Deck(pts.astype(np.float64), along, road.profile[sel], RIVER_HALF_M + 1.0, WATER_M - 1.5, half=DECK_HALF_M)
    return deck, road.path[k] + MAP_MIN_M, tangent


def carve_river(model: CountryModel, centre: np.ndarray, tangent: np.ndarray, reach_m: float = 45.0) -> None:
    """Rio perpendicular a la calzada: un canal de RIVER_HALF_M de semiancho hasta bajo el agua."""
    axis = MAP_MIN_M + (np.arange(model.height.shape[0]) + 0.5) * RASTER_PX_M
    X, Y = axis[:, None], axis[None, :]
    dX, dY = X - centre[0], Y - centre[1]
    across = np.abs(dX * tangent[0] + dY * tangent[1])            # distancia al eje del rio (a lo largo de la calzada)
    along = np.abs(-dX * tangent[1] + dY * tangent[0])
    depth = (1.0 - smooth(RIVER_HALF_M - 2.0, RIVER_HALF_M + 2.0, across)) * (1.0 - smooth(reach_m - 10.0, reach_m, along))
    bed = WATER_M - 1.5
    model.height = np.where(model.height > bed, model.height + (bed - model.height) * depth, model.height)


class RallyDensity:
    """Densidad del pais con tableros de valle, puentes de estrechos y tuneles."""

    def __init__(self, model: CountryModel, plan: RallyPlan, seed: int):
        self.model, self.plan = model, plan
        self.decks = DeckSet(plan.decks, ValueNoise3D(np.random.default_rng(seed), 3.0)) if plan.decks else None

    def __call__(self, X: np.ndarray, Y: np.ndarray, Z: np.ndarray, fields=None) -> np.ndarray:
        out = CountryModel.density(self.model, X, Y, Z)
        if self.decks is not None:
            deck = self.decks.density(X, Y, Z[None, None, :])
            if deck is not None:                  # maximo exacto: el redondeo de fuse() deja un escalon de 0,4 m
                out = np.maximum(out, deck)
        for tunnel in self.plan.tunnels:
            void = tunnel.void(X, Y, Z)
            if void is not None:
                out = np.minimum(out, -void)
        return out


def plan_rally(model: CountryModel, road: Road, want_tunnels: int = 2, want_valleys: int = 2) -> RallyPlan:
    h = model.ground_height(*np.meshgrid(MAP_MIN_M + np.arange(model.region.grid * 100 + 1.0),
                                         MAP_MIN_M + np.arange(model.region.grid * 100 + 1.0), indexing="ij"))
    shortcuts = find_shortcuts(h, road, want_tunnels)
    main, branches, tunnels, loops = build_tunnels(h, road, shortcuts)
    avoid = list(loops)
    spots = valley_spots(h, main, avoid, want_valleys)
    profile = main.profile.copy()
    for k in spots:                                           # rellano a la cota del puente: sin escalon al subir
        profile[max(0, k - 32):k + 33] = profile[k]
    main = Road(main.path, smooth_profile(profile, main.path), main.carve)
    decks, rivers = [], []
    for k in spots:
        deck, centre, tangent = valley_deck(main, k)
        decks.append(deck)
        rivers.append((centre, tangent))
    return RallyPlan(main, branches, tunnels, decks, loops, rivers)


def as_pathline(road: Road, ident: int = 0) -> PathLine:
    """La calzada como PathLine de terrain_path (puntos en m de juego y longitud de arco)."""
    pts = road.path.astype(np.float64) + MAP_MIN_M
    return PathLine(ident, None, pts, _arc(road.path))


def check_tunnel(density, tunnel: Tunnel) -> dict:
    """Galibo, ancho y pendiente del tunel sobre la densidad final (lo que se voxeliza): aire del suelo al galibo en
    el eje y a +-(ancho/2 - 1) m, suelo solido debajo, pendiente <= 12 grados."""
    idx = np.nonzero(tunnel.covered)[0]
    if len(idx) == 0:
        return {"ok": False, "length_m": 0.0}
    failures = 0
    sample = idx[:: max(1, len(idx) // 12)]
    d = tunnel.pts[-1] - tunnel.pts[0]
    normal = np.array([-d[1], d[0]]) / max(float(np.linalg.norm(d)), 1e-9)
    ok = True
    for k in sample:
        for off in (0.0, TUNNEL_HALF_M - 1.5, -(TUNNEL_HALF_M - 1.5)):
            X = np.array([[tunnel.pts[k, 0] + normal[0] * off]])
            Y = np.array([[tunnel.pts[k, 1] + normal[1] * off]])
            Z = tunnel.floor[k] + np.array([-0.6, 0.6, 2.5, 5.0])                     # galibo >= 5 m
            dens = density(X, Y, Z)[0, 0]
            good = bool(dens[0] > 0 and (dens[1:] < 0).all())
            failures += not good
            ok &= good
    grade = abs(float(tunnel.floor[-1] - tunnel.floor[0])) / max(len(tunnel.floor) - 1, 1)
    return {"ok": bool(ok and grade <= math.tan(math.radians(12.0))), "length_m": tunnel.length, "failures": failures,
            "clearance_m": CLEAR_M, "width_m": 2 * TUNNEL_HALF_M, "grade_deg": round(math.degrees(math.atan(grade)), 1)}


def smooth_profile(values: np.ndarray, path: np.ndarray) -> np.ndarray:
    steps = np.concatenate([[0.0], np.hypot(*np.diff(path, axis=0).T)])
    return lipschitz(values, steps, GRADE)

"""Trazado Rally de un pais: calzada principal, tuneles bajo montaña real que forman lazos con la calzada vieja,
y puentes sobre valles (tableros de terrain_path.bridge sobre un rio tallado). Validacion en 3D de los tuneles.

  - Tunel + lazo: un atajo recto entre dos puntos de la calzada que pasa bajo montaña (cobertura >= ROOF_M sobre el
    galibo); la calzada vieja se conserva, asi que atajo y calzada vieja cierran un circuito.
  - Puente de valle: en un minimo del terreno, en un tramo recto y llano de la calzada, se talla un rio
    perpendicular (agua de muerte) y la calzada lo cruza por un puente natural de VALLEY_BRIDGE_W_M de ancho (el
    mismo NaturalBridge que los estrechos: el Deck de terrain_path no pasa de 12 m y la calzada exige 12 libres).
Coordenadas: rejilla de 1 m de global_top (indices i Norte, j Este desde MAP_MIN_M) y metros de juego (X, Y).
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field

import numpy as np
from scipy import ndimage

from terrain_path.graph import PathLine
from terrain_shapes.bridges import NaturalBridge
from terrain_vol.density import smooth
from terrain_vol.layout import MAP_MIN_M, WATER_M

from .country import CountryModel, Road, lipschitz
from .region import RASTER_PX_M
from .validity import Discard

CLEAR_M = 6.0                   # galibo del tunel
TUNNEL_HALF_M = 7.0             # semiancho del tunel (14 m)
ROOF_M = 2.0                    # roca minima sobre el galibo para que sea tunel y no trinchera
RIVER_HALF_M = 8.0
VALLEY_BRIDGE_W_M = 16.0        # ancho del puente de valle (el de los estrechos en Rally)
VALLEY_SPAN_M = RIVER_HALF_M + 8.0      # medio largo del puente de valle, a lo largo de la calzada
DECK_FLAT_M = 2.0               # desnivel maximo del perfil original en el rellano de un puente de valle
GRADE = math.tan(math.radians(9.0))
LOOP_CLEAR = 10                 # puntos de calzada de mas entre un lazo y el rellano de un puente de valle
DECK_DRY_PTS = 40               # puntos de calzada sin agua debajo a cada lado de un puente de valle
RELLANO_PTS = 32                # puntos de calzada a cada lado del puente de valle a su cota
MAX_DECK_TURN_DEG = 12.0        # giro maximo de la calzada donde se pone un puente de valle
ROAD_GUARD_M = 6.0              # el rio tallado no se acerca a menos de esto de otro tramo de la calzada


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
    decks: list[NaturalBridge]              # puentes de valle (tambien en model.bridges)
    loops: list[tuple[int, int]]            # indices en main.path donde cada tunel sale y vuelve
    rivers: list[tuple[np.ndarray, np.ndarray]]   # (centro (X, Y), direccion) de cada rio tallado
    tunnel_ids: list[int] = field(default_factory=list)   # atajo (indice en find_shortcuts) de cada tunel
    loop_ids: list[int] = field(default_factory=list)     # atajo de cada rama tallada (branches)
    discarded_loops: int = 0                # lazos rotos: su rama no se talla
    discarded_tunnels: int = 0              # tuneles que no pasaron check_tunnel: el atajo no se talla


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


def build_tunnels(h: np.ndarray, road: Road, shortcuts, no_branch: frozenset[int] = frozenset()
                  ) -> tuple[Road, list[Road], list[Tunnel], list[tuple[int, int]]]:
    """Calzada principal con los atajos (tuneles) y la calzada vieja de cada atajo como rama del lazo, salvo las
    de los atajos de posicion no_branch (lazo descartado: queda el tunel, sin rama)."""
    parts_p, parts_z, branches, tunnels, loops = [], [], [], [], []
    last = 0
    for n, (a, b, _) in enumerate(shortcuts):
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
        if n not in no_branch:
            branches.append(Road(road.path[a:b + 1], road.profile[a:b + 1]))
        loops.append((start, start + len(line) - 1))
        last = b + 1
    parts_p.append(road.path[last:])
    parts_z.append(road.profile[last:])
    main = Road(np.concatenate(parts_p), np.concatenate(parts_z))
    return main, branches, tunnels, loops


def turn_deg(path: np.ndarray, reach: int = 20) -> np.ndarray:
    """Giro (grados) de la calzada en cada punto: angulo entre la direccion de los reach puntos anteriores y la de
    los reach siguientes."""
    n = len(path)
    k = np.arange(n)
    before = path[k] - path[np.maximum(k - reach, 0)]
    after = path[np.minimum(k + reach, n - 1)] - path[k]
    norm = np.maximum(np.hypot(*before.T) * np.hypot(*after.T), 1e-9)
    cos = np.clip((before * after).sum(axis=1) / norm, -1.0, 1.0)
    return np.degrees(np.arccos(cos))


def valley_spots(h: np.ndarray, road: Road, avoid: list[tuple[int, int]], count: int = 2, spacing: float = 150.0,
                 max_turn_deg: float = MAX_DECK_TURN_DEG, flat_m: float = DECK_FLAT_M) -> list[int]:
    """Indices de la calzada en los minimos del terreno (valles), separados spacing m, fuera de tuneles y bordes y
    en tramo recto (el rio tallado es recto: en una curva cortaria la calzada fuera del tablero), lejos del agua
    (un puente de estrecho no lleva encima otro de valle)."""
    arc = _arc(road.path)
    terrain = h[road.path[:, 0], road.path[:, 1]]
    wet = ndimage.maximum_filter1d((terrain < WATER_M + 0.5).astype(np.uint8), size=2 * DECK_DRY_PTS + 1) > 0
    span = 2 * RELLANO_PTS + 1
    rough = ndimage.maximum_filter1d(road.profile, span) - ndimage.minimum_filter1d(road.profile, span) > flat_m
    curved = (turn_deg(road.path) > max_turn_deg) | wet | rough
    low = ndimage.gaussian_filter1d(terrain, 8.0) - ndimage.gaussian_filter1d(terrain, 60.0)
    order = np.argsort(low)
    chosen: list[int] = []
    for k in order:
        if arc[k] < 60.0 or arc[-1] - arc[k] < 60.0 or curved[k] or any(a - 30 <= k <= b + 30 for a, b in avoid):
            continue
        if all(abs(arc[k] - arc[c]) >= spacing for c in chosen):
            chosen.append(int(k))
        if len(chosen) >= count:
            break
    return chosen


def pick_spots(h: np.ndarray, road: Road, avoid: list[tuple[int, int]], count: int) -> list[int]:
    """Puntos de los puentes de valle: primero con las condiciones estrictas (150 m entre puentes, tramo recto y
    llano); si la calzada es corta o montañosa, se relajan por pasos (separacion, desnivel y giro)."""
    best: list[int] = []
    for flat_m, turn in ((DECK_FLAT_M, MAX_DECK_TURN_DEG), (2.0 * DECK_FLAT_M, 20.0)):
        for spacing in (150.0, 100.0, 60.0):
            spots = valley_spots(h, road, avoid, count, spacing, turn, flat_m)
            if len(spots) >= count:
                return spots
            best = max(best, spots, key=len)
    return best


def valley_bridge(road: Road, k: int) -> tuple[NaturalBridge, np.ndarray, np.ndarray]:
    """Puente natural sobre el rio tallado en road.path[k] (de VALLEY_SPAN_M antes a VALLEY_SPAN_M despues, a la
    cota de la calzada) y el eje (centro, direccion) del rio."""
    arc = _arc(road.path)
    ia = int(np.searchsorted(arc, arc[k] - VALLEY_SPAN_M))
    ib = min(int(np.searchsorted(arc, arc[k] + VALLEY_SPAN_M)), len(arc) - 1)
    (xa, ya), (xb, yb) = road.path[ia] + MAP_MIN_M, road.path[ib] + MAP_MIN_M
    tangent = np.array([xb - xa, yb - ya], dtype=np.float64)
    tangent = tangent / max(float(np.linalg.norm(tangent)), 1e-9)
    bridge = NaturalBridge((float(ya), float(xa)), (float(yb), float(xb)), float(road.profile[ia]), float(road.profile[ib]),
                           width_m=VALLEY_BRIDGE_W_M, rise_m=0.3, pier_m=3.0)
    return bridge, (road.path[k] + MAP_MIN_M).astype(np.float64), tangent


def carve_river(model: CountryModel, centre: np.ndarray, tangent: np.ndarray, reach_m: float = 45.0) -> None:
    """Rio perpendicular a la calzada: un canal de RIVER_HALF_M de semiancho hasta bajo el agua. Solo corta la
    calzada bajo su tablero: si la calzada vuelve a pasar cerca (curvas), el rio se para antes de ese tramo."""
    axis = MAP_MIN_M + (np.arange(model.height.shape[0]) + 0.5) * RASTER_PX_M
    X, Y = axis[:, None], axis[None, :]
    dX, dY = X - centre[0], Y - centre[1]
    across = np.abs(dX * tangent[0] + dY * tangent[1])            # distancia al eje del rio (a lo largo de la calzada)
    along = np.abs(-dX * tangent[1] + dY * tangent[0])
    depth = (1.0 - smooth(RIVER_HALF_M - 2.0, RIVER_HALF_M + 2.0, across)) * (1.0 - smooth(reach_m - 10.0, reach_m, along))
    size = 2 * int(ROAD_GUARD_M / RASTER_PX_M) + 1
    guard = ndimage.maximum_filter(np.asarray(model.road_mask, dtype=np.float64), size=size)
    depth = depth * np.where(np.hypot(dX, dY) < RIVER_HALF_M + 2.0 * ROAD_GUARD_M, 1.0, 1.0 - guard)
    bed = WATER_M - 1.5
    model.height = np.where(model.height > bed, model.height + (bed - model.height) * depth, model.height)


class RallyDensity:
    """Densidad del pais (puentes de estrechos y de valle incluidos, en model.bridges) con los tuneles."""

    def __init__(self, model: CountryModel, plan: RallyPlan, seed: int):
        self.model, self.plan, self.seed = model, plan, seed

    def __call__(self, X: np.ndarray, Y: np.ndarray, Z: np.ndarray, fields=None) -> np.ndarray:
        out = CountryModel.density(self.model, X, Y, Z)
        for tunnel in self.plan.tunnels:
            void = tunnel.void(X, Y, Z)
            if void is not None:
                out = np.minimum(out, -void)
        return out


def plan_rally(model: CountryModel, road: Road, want_tunnels: int = 2, want_valleys: int = 2,
               discard: Discard = Discard()) -> RallyPlan:
    """Tuneles y lazos opcionales (los atajos bajo montaña que haya, menos los descartados) y puentes de valle."""
    h = model.ground_height(*np.meshgrid(MAP_MIN_M + np.arange(model.region.grid * 100 + 1.0),
                                         MAP_MIN_M + np.arange(model.region.grid * 100 + 1.0), indexing="ij"))
    found = find_shortcuts(h, road, want_tunnels)
    ids = [n for n in range(len(found)) if n not in discard.tunnels]
    no_branch = frozenset(k for k, n in enumerate(ids) if n in discard.loops)
    main, branches, tunnels, loops = build_tunnels(h, road, [found[n] for n in ids], no_branch)
    avoid = [(a - LOOP_CLEAR, b + LOOP_CLEAR) for a, b in loops]      # el rellano de un puente no toca un lazo
    spots = pick_spots(h, main, avoid, want_valleys)
    profile = main.profile.copy()
    for k in spots:                                           # rellano a la cota del puente: sin escalon al subir
        profile[max(0, k - RELLANO_PTS):k + RELLANO_PTS + 1] = profile[k]
    main = Road(main.path, smooth_profile(profile, main.path), main.carve)
    tunnels, branches = match_loops(main, tunnels, branches, loops, no_branch)
    decks, rivers = [], []
    for k in spots:
        deck, centre, tangent = valley_bridge(main, k)
        decks.append(deck)
        rivers.append((centre, tangent))
    return RallyPlan(main, branches, tunnels, decks, loops, rivers, ids, [n for n in ids if n not in discard.loops],
                     len(ids) - len(branches), len(found) - len(ids))


def match_loops(main: Road, tunnels: list[Tunnel], branches: list[Road], loops: list[tuple[int, int]],
                no_branch: frozenset[int]) -> tuple[list[Tunnel], list[Road]]:
    """Tuneles y ramas a la cota final de la calzada en sus bocas: el suelo del tunel es el perfil final y cada
    rama se corrige con una rampa lineal para empalmar sin escalon."""
    carved = iter(branches)
    out_t, out_b = [], []
    for n, ((a, b), tunnel) in enumerate(zip(loops, tunnels)):
        za, zb = float(main.profile[a]), float(main.profile[b])
        out_t.append(Tunnel(tunnel.pts, np.linspace(za, zb, len(tunnel.floor)), tunnel.covered))
        if n in no_branch:
            continue
        branch = next(carved)
        t = np.linspace(0.0, 1.0, len(branch.profile))
        fixed = branch.profile + (za - branch.profile[0]) * (1.0 - t) + (zb - branch.profile[-1]) * t
        out_b.append(Road(branch.path, fixed, branch.carve))
    return out_t, out_b


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

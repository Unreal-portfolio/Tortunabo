"""Barranco: un cauce hondo que cruza el camino principal una vez, con un rio en el fondo, y que
el principal salva con un puente (bridge.Deck). Modo "deadly": ningun camino baja y el fondo
lleva cajas de muerte (manifest "kill_boxes_uu", ATN_MapVariantLoader pone un
ATN_DeathZoneVolume en cada una).

El eje sale del punto de cruce en perpendicular al principal hacia los dos lados, serpenteando,
hasta el borde del mapa o hasta quedar a menos de su anchura de otro camino (ahi se cierra en
cuna: el fondo sube hasta el relieve)."""

from __future__ import annotations

import math
from dataclasses import dataclass

import numpy as np
from scipy.spatial import cKDTree

from terrain_vol.density import smooth

from .bridge import Deck
from .curves import resample
from .layout import MAP_MAX_M, MAP_MIN_M, WATER_M

FLOOR_M = WATER_M - 1.5          # fondo del cauce (el agua del nivel queda 1,5 m por encima)
BANK_M = WATER_M + 0.4           # orillas de arena junto a las paredes
WALL_DEG = 62.0                  # paredes del barranco (media; el ruido las abre o las cierra)
EDGE_KEEP_M = 45.0               # el barranco se cierra antes de llegar a esta distancia del borde
KILL_TOP_M = WATER_M + 2.0       # cara de arriba de las cajas de muerte
END_TAPER_M = 18.0               # tramo final en el que el fondo sube hasta cerrarse (cuna)
MIN_SIDE_M = 26.0                # cada lado del cruce mide al menos esto (si no, se descarta)


@dataclass(frozen=True)
class Canyon:
    pts: np.ndarray              # (N, 2) eje cada 1 m, de un extremo a otro
    half: np.ndarray             # (N,) semiancho en lo alto
    depth: np.ndarray            # (N,) 0..1: 1 = fondo completo, 0 = cerrado (extremos en cuna)
    s_main: float                # arco del principal donde lo cruza
    mode: str
    z_main: float                # cota del principal (tablero) sobre el barranco
    flat: float                  # tramo llano del principal a cada lado del eje
    crossings: tuple = ()        # (linea, arco) de los otros caminos que lo cruzan en puente


def raise_main(prof, arc: np.ndarray, canyon: Canyon, grade: float):
    """Perfil del principal con el tramo del puente llano a z_main y rampas a la pendiente maxima
    hasta su cota original (solo sube; las uniones quedan fuera por construccion)."""
    from dataclasses import replace

    from .profile import clamp_grade
    plateau = np.abs(arc - canyon.s_main) <= canyon.flat
    z = prof.z.copy()
    z[plateau] = np.maximum(z[plateau], canyon.z_main)
    z = clamp_grade(z, arc, np.full(len(z), grade), set(np.nonzero(plateau)[0].tolist()))
    return replace(prof, z=np.maximum(z, prof.z))


def _walk(rng: np.random.Generator, start: np.ndarray, heading: float, blocked, max_len: float) -> np.ndarray:
    pts, h, p = [start.copy()], heading, start.copy()
    phase, wave = rng.uniform(0.0, 2 * math.pi), rng.uniform(40.0, 70.0)
    for step in range(int(max_len)):
        h += math.radians(2.2) * math.sin(2 * math.pi * step / wave + phase) + rng.normal(0.0, math.radians(0.8))
        p = p + np.array([math.cos(h), math.sin(h)])
        # Lejos del borde: el barranco se cierra dentro del mapa (el cauce hasta el horizonte
        # dejaba ver la costura con la corona).
        if min(p[0] - MAP_MIN_M, MAP_MAX_M - p[0], p[1] - MAP_MIN_M, MAP_MAX_M - p[1]) < EDGE_KEEP_M:
            break
        if blocked(p, step, h):
            break
        pts.append(p.copy())
    return np.array(pts)


def plan_canyon(model, rng: np.random.Generator) -> Canyon | None:
    style = model.style
    if style.canyon == "none":
        return None
    plan, main = model.plan, model.plan.graph.main
    prof = plan.profiles[0]
    S = model.S
    tree_all = cKDTree(S["p"])
    joins = [c.point for c in plan.crossings] + [line.points[k] for line in plan.graph.loops() for k in (0, -1)]
    lo_w, hi_w = style.canyon_width_m
    # Arco en el principal de cada union o cruce que esta sobre el (sus cotas no se pueden mover).
    d_j, k_j = cKDTree(main.points).query(np.array(joins))
    join_arcs = [float(main.arc[k]) for d, k in zip(d_j, k_j) if d < 8.0]
    for _ in range(300):
        s = float(rng.uniform(0.04, 0.85) * main.length)
        half0 = 0.5 * float(rng.uniform(lo_w, hi_w))
        flat = 1.2 * half0 + 8.0                     # tramo llano del principal a cada lado del eje
        near = (main.arc > s - flat - 10.0) & (main.arc < s + flat + 10.0)
        if prof.tunnel[near].any() or np.isin(prof.biome[near], (1, 3)).any():
            continue
        p0 = main.point_at(s)
        if any(np.hypot(*(p0 - q)) < flat + 10.0 for q in joins):
            continue
        if any(lid == 0 and a - flat - 15.0 < s < b + flat + 15.0 for lid, a, b in model.deck_cuts + model.arch_ranges):
            continue
        # Cota del principal sobre el barranco: la pedida por el estilo, hasta donde dejan subir las
        # rampas (pendiente maxima) sin tocar las uniones de los lazos.
        room = min([abs(a - s) for a in join_arcs] + [s, main.length - s]) - flat - 5.0
        z_here = float(prof.z[(main.arc > s - flat) & (main.arc < s + flat)].max())
        z_top = min(FLOOR_M + float(rng.uniform(*style.canyon_depth_m)), z_here + room * style.max_grade)
        z_top = max(z_top, z_here)
        if z_top < FLOOR_M + style.canyon_depth_m[0]:
            continue

        crossed: dict[int, float] = {0: s}

        def blocked(p, step, heading, half=half0, flat=flat):
            """True si el barranco tiene que pararse en p. Puede cruzar otro camino (con su propio
            puente) si va alto, de frente y lejos de uniones, tuneles y otros puentes."""
            d, j = tree_all.query(p)
            if d - S["w"][j] >= half + 8.0:
                return False
            lid, s_l = int(S["line"][j]), float(S["s"][j])
            # Solo junto al propio cruce (el tramo del puente); el mismo camino mas alla, no.
            window = flat if lid == 0 else 1.2 * half + 10.0
            if lid in crossed and abs(s_l - crossed[lid]) < window:
                return False
            ok = _can_cross(model, lid, s_l, half, heading)
            if ok:
                crossed[lid] = s_l
            return not ok

        n = main.normal_at(s)
        sides = [_walk(rng, p0, math.atan2(sign * n[1], sign * n[0]), blocked, 260.0) for sign in (1.0, -1.0)]
        lengths = [len(a) - 1 for a in sides]
        if min(lengths) < MIN_SIDE_M:
            continue
        axis = np.vstack([sides[1][::-1], sides[0][1:]])
        pts, arc = resample(axis, 1.0)
        # Semiancho que cambia a lo largo (+-20 %) y fondo que se cierra en los extremos que no
        # llegan al borde del mapa.
        wob = 1.0 + 0.2 * np.sin(arc / rng.uniform(25.0, 45.0) + rng.uniform(0, 6.3))
        half = half0 * wob
        depth = np.ones(len(arc))
        for end in (0, -1):                          # los dos extremos se cierran en cuna
            dist = arc - arc[0] if end == 0 else arc[-1] - arc
            depth = depth * smooth(0.0, END_TAPER_M, dist)
        # Cruce real con cada camino (el mas cercano del eje): si solo lo roza, se descarta.
        crossings = []
        for lid in crossed:
            if lid == 0:
                continue
            line = plan.graph.lines[lid]
            dd, kk = cKDTree(line.points).query(pts)
            if dd.min() > 2.0:
                crossings = None
                break
            crossings.append((lid, float(line.arc[kk[int(np.argmin(dd))]])))
        if crossings is None:
            continue
        return Canyon(pts, half, depth, s, style.canyon, z_top, flat, tuple(crossings))
    return None


def _can_cross(model, lid: int, s_l: float, half: float, heading: float) -> bool:
    plan, style = model.plan, model.style
    if lid == 0:
        return False                                  # el principal solo lo cruza una vez
    line, prof = plan.graph.lines[lid], plan.profiles[lid]
    reach = 1.2 * half + 20.0
    if s_l < reach or s_l > line.length - reach:
        return False
    near = (line.arc > s_l - reach) & (line.arc < s_l + reach)
    if prof.tunnel[near].any() or np.isin(prof.biome[near], (1, 3)).any():
        return False
    if prof.z[near].min() < FLOOR_M + style.canyon_depth_m[0]:
        return False
    t = line.tangent_at(s_l)
    if abs(math.cos(heading) * t[1] - math.sin(heading) * t[0]) < math.sin(math.radians(45.0)):
        return False
    p = line.point_at(s_l)
    joins = [c.point for c in plan.crossings] + [ln.points[k] for ln in plan.graph.loops() for k in (0, -1)]
    if any(np.hypot(*(p - q)) < reach for q in joins):
        return False
    return not any(l2 == lid and a - reach < s_l < b + reach for l2, a, b in model.deck_cuts + model.arch_ranges)


def _inside(p: np.ndarray) -> bool:
    return bool(MAP_MIN_M <= p[0] <= MAP_MAX_M and MAP_MIN_M <= p[1] <= MAP_MAX_M)


class CanyonField:
    def __init__(self, canyon: Canyon, noise):
        self.c = canyon
        self.tree = cKDTree(canyon.pts)
        self.noise = noise

    def query(self, X, Y):
        pts = np.stack([np.ravel(X), np.ravel(Y)], axis=1)
        q, k = self.tree.query(pts)
        return q.reshape(X.shape), self.c.half[k].reshape(X.shape), self.c.depth[k].reshape(X.shape)

    def carve(self, X, Y, height: np.ndarray, path_mask: np.ndarray) -> np.ndarray:
        """Baja el relieve al perfil del barranco (fondo con rio, orillas, paredes de WALL_DEG).
        path_mask (0..1) protege el camino: el principal pasa por encima en el puente."""
        q, half, depth = self.query(X, Y)
        half = half * (1.0 + 0.12 * self.noise(X, Y))
        tan = np.tan(np.radians(WALL_DEG + 12.0 * self.noise(X * 0.7, Y * 0.7)))
        bank_w = 2.0 + 1.0 * self.noise(Y, X)
        bed = FLOOR_M + (BANK_M - FLOOR_M) * smooth(half - bank_w - 1.5, half - bank_w, q)
        profile = bed + np.maximum(q - half, 0.0) * tan
        # En los extremos cerrados el cauce sube hasta el relieve (cuna).
        profile = profile + (1.0 - depth) * 60.0
        carved = np.minimum(height, profile)
        return height * path_mask + carved * (1.0 - path_mask)

    def inside(self, X, Y) -> np.ndarray:
        q, half, depth = self.query(X, Y)
        return (q < half) & (depth > 0.5)


def canyon_deck(model, canyon: Canyon, line_id: int = 0, s: float | None = None) -> tuple[Deck, tuple[int, float, float]]:
    """Puente de un camino sobre el barranco (misma losa que los cruces); por defecto, el principal."""
    main, prof = model.plan.graph.lines[line_id], model.plan.profiles[line_id]
    s = canyon.s_main if s is None else s
    k = int(np.argmin(np.hypot(*(canyon.pts - main.point_at(s)).T)))
    t_main = main.tangent_at(s)
    j0, j1 = max(k - 2, 0), min(k + 2, len(canyon.pts) - 1)
    t_c = canyon.pts[j1] - canyon.pts[j0]
    t_c = t_c / max(float(np.linalg.norm(t_c)), 1e-9)
    sin = max(abs(float(t_main[0] * t_c[1] - t_main[1] * t_c[0])), 0.5)
    clear = float(canyon.half[k]) * 1.12 / sin + 2.0
    reach = clear + 5.0
    sel = (main.arc >= s - reach) & (main.arc <= s + reach)
    pts, arc = resample(main.points[sel], 0.5)
    arc = arc + float(main.arc[sel][0])
    top = np.interp(arc, main.arc, prof.z)
    return Deck(pts, arc - s, top, clear, FLOOR_M, half=1.9, thick=2.0), (line_id, s - clear, s + clear)


def kill_boxes_uu(canyon: Canyon, uu: float = 100.0, piece_m: float = 16.0) -> list[dict]:
    """Cajas que cubren el fondo (de FLOOR_M - 1 a KILL_TOP_M), una por tramo de piece_m."""
    out = []
    n = len(canyon.pts)
    step = int(piece_m)
    for a in range(0, n - 1, step):
        b = min(a + step, n - 1)
        if canyon.depth[a:b + 1].max() < 0.5 or not (_inside(canyon.pts[a]) or _inside(canyon.pts[b])):
            continue
        p0, p1 = canyon.pts[a], canyon.pts[b]
        mid = 0.5 * (p0 + p1)
        d = p1 - p0
        length = float(np.linalg.norm(d))
        half = float(canyon.half[a:b + 1].max()) * 1.15
        z0, z1 = FLOOR_M - 1.0, KILL_TOP_M
        out.append({"center": [mid[0] * uu, mid[1] * uu, 0.5 * (z0 + z1) * uu],
                    "extent": [(0.5 * length + 0.5 * half) * uu, half * uu, 0.5 * (z1 - z0) * uu],
                    "yaw": math.degrees(math.atan2(d[1], d[0]))})
    return out

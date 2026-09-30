"""Validez de un pais de Rally (decision del director, 2026-09-30). Un pais vale si:

  - la calzada es transitable en buggy del inicio al final: ROAD_WIDTH_M de ancho y ROAD_SLOPE_DEG de pendiente;
  - hay al menos MIN_ROAD_BRIDGES puentes en la calzada, y el mapa pasa costuras, islas y presupuesto;
  - todo tunel tallado pasa check_tunnel (galibo, ancho y pendiente sobre la densidad final).

El tunel solo es obligatorio donde la calzada, sin el, no pasa: si la cota superior ya une inicio y final con esa
pendiente, el pais vale sin tuneles. Los lazos (tunel + calzada vieja) son opcionales: el que sale roto se descarta
(su rama no se talla) y se cuenta; no invalida el mapa.

Todo en 2,5D sobre `top` (una muestra por metro, [Norte, Este]); los tuneles entran como enlaces entre sus bocas.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

from terrain_vol.validate import _nearest, components, corridor_ok, wide_ground

ROAD_WIDTH_M = 12.0
ROAD_SLOPE_DEG = 12.0
MIN_ROAD_BRIDGES = 2
SUSTAINED_M = 6.0                   # tramo sobre el que se mide la pendiente sostenida de la calzada

Point = tuple[int, int]
Link = tuple[Point, Point]


@dataclass(frozen=True)
class Discard:
    """Atajos (por su indice en find_shortcuts) que no se tallan: `loops`, solo la rama del lazo (el tunel se
    queda); `tunnels`, el atajo entero (la calzada sigue por donde iba)."""
    loops: frozenset[int] = frozenset()
    tunnels: frozenset[int] = frozenset()

    def __bool__(self) -> bool:
        return bool(self.loops or self.tunnels)

    def merged(self, other: "Discard") -> "Discard":
        return Discard(self.loops | other.loops, self.tunnels | other.tunnels)


@dataclass(frozen=True)
class RallyChecks:
    road_open: bool                 # la calzada une inicio y final por la cota superior, sin tuneles
    road_linked: bool               # idem contando los tuneles validos como enlaces entre sus bocas
    tunnels: tuple[dict, ...]       # check_tunnel de cada tunel tallado
    loops: tuple[bool, ...]         # cada lazo tallado, transitable o no
    road_bridges: int
    discarded_loops: int = 0
    discarded_tunnels: int = 0


def road_reach(top: np.ndarray, a: Point, b: Point, links: list[Link], width: float = ROAD_WIDTH_M,
               slope: float = ROAD_SLOPE_DEG) -> tuple[bool, bool]:
    """(sin tuneles, con tuneles): a y b unidos por suelo de width m de ancho y pendiente <= slope sobre la cota
    superior; con tuneles, cada enlace une ademas sus dos bocas."""
    wide = wide_ground(top, width, slope)
    pa, pb = _nearest(wide, a, 6), _nearest(wide, b, 6)
    if pa is None or pb is None:
        return False, False
    labels = components(top, wide, slope)
    parent: dict[int, int] = {}

    def root(k: int) -> int:
        while parent.get(k, k) != k:
            k = parent[k]
        return k

    la, lb = int(labels[pa]), int(labels[pb])
    is_open = la == lb
    for p, q in links:
        pp, pq = _nearest(wide, p, 8), _nearest(wide, q, 8)
        if pp is not None and pq is not None:
            parent[root(int(labels[pp]))] = root(int(labels[pq]))
    return is_open, root(la) == root(lb)


def road_ok(top: np.ndarray, a: Point, b: Point, links: list[Link], width: float = ROAD_WIDTH_M,
            slope: float = ROAD_SLOPE_DEG) -> bool:
    """Calzada transitable de a a b, con los tuneles dados como enlaces."""
    return road_reach(top, a, b, links, width, slope)[1]


def loop_ok(top: np.ndarray, path: np.ndarray, width: float = ROAD_WIDTH_M, slope: float = ROAD_SLOPE_DEG) -> bool:
    """La rama de un lazo (indices de top) une sus dos extremos con el ancho y la pendiente de la calzada."""
    a, b = (int(path[0][0]), int(path[0][1])), (int(path[-1][0]), int(path[-1][1]))
    return bool(corridor_ok(top, a, b, width, slope))


def road_breaks(top: np.ndarray, path: np.ndarray, width: float = ROAD_WIDTH_M, slope: float = ROAD_SLOPE_DEG,
                count: int = 8) -> list[Point]:
    """Hasta count puntos (indices de top) de la calzada fuera del tramo ancho del inicio: donde se corta."""
    wide = wide_ground(top, width, slope)
    labels = components(top, wide, slope)
    inside = [(int(i), int(j)) for i, j in path if 0 <= i < top.shape[0] and 0 <= j < top.shape[1]]
    first = next((labels[p] for p in inside if labels[p] >= 0), -1)
    bad = [p for p in inside if labels[p] != first]
    return bad[::max(1, len(bad) // count)][:count]


def sustained_grade_deg(arc: np.ndarray, z: np.ndarray, run_m: float = SUSTAINED_M) -> np.ndarray:
    """Pendiente longitudinal sostenida (grados) en cada punto de un perfil: el desnivel entre run_m / 2 antes y
    run_m / 2 despues (un escalon suelto de la malla no cuenta; un repecho de run_m, si)."""
    if len(arc) < 2:
        return np.zeros(len(arc))
    lo = np.interp(arc - run_m / 2.0, arc, z)
    hi = np.interp(arc + run_m / 2.0, arc, z)
    return np.degrees(np.arctan(np.abs(hi - lo) / run_m))


def new_discards(checks: RallyChecks, loop_ids: list[int], tunnel_ids: list[int]) -> Discard:
    """Lo que hay que dejar de tallar: la rama de cada lazo roto y el atajo de cada tunel que no pasa check_tunnel."""
    return Discard(frozenset(i for i, ok in zip(loop_ids, checks.loops) if not ok),
                   frozenset(i for i, c in zip(tunnel_ids, checks.tunnels) if not c["ok"]))


def rally_verdict(base_ok: bool, checks: RallyChecks) -> dict:
    """Veredicto del pais con la regla de arriba: `ok` y los motivos por los que no vale. base_ok es el informe
    comun (final a pie, islas, presupuesto y costuras)."""
    reasons = []
    if not base_ok:
        reasons.append("mapa")
    if checks.road_bridges < MIN_ROAD_BRIDGES:
        reasons.append("puentes")
    if not all(t["ok"] for t in checks.tunnels):
        reasons.append("tunel_no_valido")
    if not checks.road_linked:
        # Sin calzada continua a <= 12 grados hace falta un tunel (o rebajar el paso) en el corte.
        reasons.append("calzada_cortada" if checks.tunnels else "tunel_necesario")
    return {"ok": not reasons, "motivos": reasons, "calzada": checks.road_linked,
            "tuneles_necesarios": not checks.road_open, "tuneles": list(checks.tunnels),
            "lazos": list(checks.loops), "lazos_rotos": int(sum(not ok for ok in checks.loops)),
            "lazos_descartados": checks.discarded_loops, "tuneles_descartados": checks.discarded_tunnels,
            "puentes_en_calzada": checks.road_bridges, "ancho_m": ROAD_WIDTH_M, "pendiente_max_deg": ROAD_SLOPE_DEG}


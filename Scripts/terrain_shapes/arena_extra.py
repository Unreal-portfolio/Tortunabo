"""Arenas del catalogo que faltaban (Docs/Catalogo-Mapas-2026-09-29.md §3): A04 Reloj y A06 Panal. Mismo
lienzo y convenciones que terrain_shapes/arena.py (cotas sobre el agua, semilla, ShapeMap).
"""

from __future__ import annotations

import math
from dataclasses import asdict, dataclass

import numpy as np

from .canvas import Canvas, plateau, sd_circle, smoothstep
from .kit import KitModel, above, ramp, ramp_slope_deg, sea_floor
from .model import ShapeMap


def _pt(r: float, deg: float) -> tuple[float, float]:
    return r * math.cos(math.radians(deg)), r * math.sin(math.radians(deg))


def hour_angle(hour: int) -> float:
    """Angulo (grados desde el Este, antihorario) de la hora `hour` de un reloj con las 12 al Norte."""
    return 90.0 - 30.0 * (hour % 12)


# ── A04 Reloj ────────────────────────────────────────────────────────────────────
@dataclass(frozen=True)
class ClockSpec:
    """Disco de radius_m a height_m con 12 pilares de las horas (radio pillar_r_m, pillar_h_m sobre el disco)
    en r = hour_ring_m (la de las 12, mayor), 60 marcas de minuto de 0,5 m en el borde y un estrado central
    suave (eje de las agujas, que son actores)."""
    name: str
    seed: int
    grid: int = 2
    radius_m: float = 60.0
    height_m: float = 4.0
    hour_ring_m: float = 50.0
    pillar_r_m: float = 3.0
    pillar_h_m: float = 3.0
    hub_r_m: float = 7.0
    hub_h_m: float = 1.0
    description: str = ""
    mode: str = "tct"


def clock_a04(seed: int = 3004) -> ClockSpec:
    return ClockSpec("A04_reloj", seed, description=(
        "Reloj (Todos contra Todos): disco de 120 m a +4 m con los 12 pilares de las horas (+7 m; el de las 12, "
        "+8 m), marcas de minuto en el borde y el estrado del eje en el centro; las dos agujas son actores que "
        "barren la arena."))


def build_clock(spec: ClockSpec) -> tuple[ShapeMap, dict]:
    canvas = Canvas(spec.grid)
    e, n = canvas.design_grid()
    disc = sd_circle(e, n, 0.0, 0.0, spec.radius_m)
    height = np.maximum(sea_floor(e, n, disc), plateau(disc, spec.height_m, 2.0))
    hub = sd_circle(e, n, 0.0, 0.0, spec.hub_r_m)
    height = height + spec.hub_h_m * smoothstep(-3.0, 1.0, hub)
    for minute in range(60):
        if minute % 5 == 0:
            continue
        ce, cn = _pt(spec.radius_m - 3.5, 90.0 - 6.0 * minute)
        height = height + 0.5 * smoothstep(-0.6, 0.4, sd_circle(e, n, ce, cn, 0.9))
    hours = {}
    for hour in range(1, 13):
        big = hour == 12
        ce, cn = _pt(spec.hour_ring_m, hour_angle(hour))
        sd = sd_circle(e, n, ce, cn, spec.pillar_r_m * (1.5 if big else 1.0))
        top = above(spec.height_m + spec.pillar_h_m + (1.0 if big else 0.0))
        height = np.maximum(height, np.where(sd > -1.0, above(spec.height_m) + (top - above(spec.height_m))
                                             * smoothstep(-1.0, 0.3, sd), height))
        hours[hour] = _pt(spec.hour_ring_m - 8.0, hour_angle(hour))
    required = {"centro": (0.0, 0.0), **{f"hora_{h}": hours[h] for h in (3, 6, 9, 12)}}
    model = KitModel(canvas, height)
    markers = {"eje_agujas": [(0.0, 0.0)], "catapulta": [(0.0, 0.0)],
               "trampolin": [_pt(spec.hour_ring_m - 8.0, hour_angle(h)) for h in (4, 8, 12)]}
    shape = ShapeMap(spec.name, spec.seed, spec.mode, spec.description, canvas, model, hours[6], None, required, [],
                     params={"generator": "reloj", **asdict(spec)})
    return shape, markers


# ── A06 Panal ────────────────────────────────────────────────────────────────────
def sd_hex(e, n, ce: float, cn: float, side: float):
    """Distancia (aprox., positiva dentro) a un hexagono de lado `side` con dos lados horizontales (plano arriba)."""
    de, dn = np.abs(e - ce), np.abs(n - cn)
    apothem = side * math.sqrt(3.0) / 2.0
    return np.minimum(apothem - dn, apothem - (de * math.sqrt(3.0) / 2.0 + dn / 2.0))


def hex_centres(rings: int, pitch: float) -> dict[tuple[int, int], tuple[float, float]]:
    """Centros (axiales q, r) de un panal de `rings` anillos alrededor del central, hexagonos de plano arriba."""
    out = {}
    for q in range(-rings, rings + 1):
        for r in range(max(-rings, -q - rings), min(rings, -q + rings) + 1):
            out[(q, r)] = (pitch * 1.5 / math.sqrt(3.0) * q, pitch * (r + q / 2.0))
    return out


@dataclass(frozen=True)
class HoneycombSpec:
    """19 hexagonos de lado side_m separados gap_m de agua, a height_m (el central, centre_h_m con el cofre);
    istmos de isthmus_m de ancho entre vecinos: un arbol que los une todos mas extra_links al azar. Los huecos
    sin istmo se saltan (2 m)."""
    name: str
    seed: int
    grid: int = 2
    rings: int = 2
    side_m: float = 12.0
    gap_m: float = 2.0
    height_m: float = 4.0
    centre_h_m: float = 6.0
    isthmus_m: float = 4.0
    extra_links: int = 8
    description: str = ""
    mode: str = "tct"


def honeycomb_a06(seed: int = 3006) -> HoneycombSpec:
    return HoneycombSpec("A06_panal", seed, description=(
        "Panal (Todos contra Todos): 19 hexágonos de 12 m de lado a +4 m separados por 2 m de agua que se saltan; "
        "istmos de 4 m unen el panal y el hexágono central, a +6 m, guarda el cofre."))


def build_honeycomb(spec: HoneycombSpec) -> tuple[ShapeMap, dict]:
    canvas = Canvas(spec.grid)
    e, n = canvas.design_grid()
    apothem = spec.side_m * math.sqrt(3.0) / 2.0
    pitch = 2.0 * apothem + spec.gap_m
    centres = hex_centres(spec.rings, pitch)
    land = np.full(e.shape, -1e3)
    height = np.full(e.shape, -1e3)
    for key, (ce, cn) in centres.items():
        sd = sd_hex(e, n, ce, cn, spec.side_m)
        h = spec.centre_h_m if key == (0, 0) else spec.height_m
        land = np.maximum(land, sd)
        height = np.maximum(height, plateau(sd, h, 0.3, 0.6))
    height = np.maximum(height, sea_floor(e, n, land))
    pairs = sorted({tuple(sorted((a, b))) for a in centres for b in centres
                    if a != b and math.dist(centres[a], centres[b]) < pitch * 1.01})
    rng = np.random.default_rng(spec.seed)
    order = rng.permutation(len(pairs))
    parent = {k: k for k in centres}

    def root(k):
        while parent[k] != k:
            parent[k] = parent[parent[k]]
            k = parent[k]
        return k

    tree, extra = [], []
    for idx in order:
        a, b = pairs[idx]
        if root(a) != root(b):
            parent[root(a)] = root(b)
            tree.append((a, b))
        else:
            extra.append((a, b))
    links = tree + extra[:spec.extra_links]
    corridors, jumps = [], []
    for a, b in links:
        (ae, an), (be, bn) = centres[a], centres[b]
        ha = spec.centre_h_m if a == (0, 0) else spec.height_m
        hb = spec.centre_h_m if b == (0, 0) else spec.height_m
        ue, un = (be - ae) / pitch, (bn - an) / pitch
        reach = apothem - 5.0 if ha == hb else apothem - 3.0
        pa = (ae + ue * reach, an + un * reach)
        pb = (be - ue * reach, bn - un * reach)
        if ramp_slope_deg(above(ha), above(hb), pa, pb) > 18.0:
            raise ValueError(f"istmo {a}-{b} de mas de 18 grados")
        height = ramp(height, e, n, pa, pb, above(ha), above(hb), spec.isthmus_m, blend_m=0.6)
        corridors.append((pa, pb))
    for a, b in pairs:
        if (a, b) not in links and (b, a) not in links:
            (ae, an), (be, bn) = centres[a], centres[b]
            jumps.append((0.5 * (ae + be), 0.5 * (an + bn)))
    required = {f"hex_{q}_{r}": c for (q, r), c in centres.items()}
    model = KitModel(canvas, height)
    markers = {"cofre": [centres[(0, 0)]], "salto": jumps,
               "trampolin": [centres[k] for k in sorted(centres, key=lambda k: -math.hypot(*centres[k]))[:3]]}
    params = {"generator": "panal", **asdict(spec), "links": [[list(a), list(b)] for a, b in links]}
    shape = ShapeMap(spec.name, spec.seed, spec.mode, spec.description, canvas, model, required["hex_0_2"], (0.0, 0.0),
                     required, corridors, corridor_width_m=3.0, corridor_slope_deg=20.0, params=params)
    return shape, markers

"""Rio como tramo del camino: cauce hondo con una orilla-repisa que va cambiando de lado; en
cada cambio, una cadena de islas alargadas cruza en diagonal (se salta de una a otra). Islas
sueltas alargadas a los lados, nunca en ristra por el centro."""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

from terrain_vol.density import smooth

from .curves import knot_noise
from .layout import WATER_M

BANK_TOP_M = WATER_M + 0.7


@dataclass
class Island:
    line: int
    s: float
    q: float          # desplazamiento lateral (m) desde el eje, con signo
    half_len: float
    half_wid: float


@dataclass
class River:
    sides: dict[int, tuple[np.ndarray, np.ndarray]]
    shelf: dict[int, tuple[np.ndarray, np.ndarray]]
    islands: list[Island]


def _segments(mask: np.ndarray, arc: np.ndarray) -> list[tuple[float, float]]:
    out, start = [], None
    for k, flag in enumerate(mask):
        if flag and start is None:
            start = arc[k]
        if not flag and start is not None:
            out.append((start, arc[k - 1]))
            start = None
    if start is not None:
        out.append((start, arc[-1]))
    return out


def plan_river(rng: np.random.Generator, model) -> River | None:
    sides, shelf, islands = {}, {}, []
    for line in model.plan.graph.lines:
        prof = model.plan.profiles[line.id]
        for s0, s1 in _segments(prof.biome == 1, line.arc):
            if s1 - s0 < 20.0:
                continue
            knots = [s0]
            while knots[-1] < s1:
                knots.append(knots[-1] + float(rng.uniform(30.0, 60.0)))
            side = [float(rng.choice([-1.0, 1.0]))]
            for _ in knots[1:]:
                side.append(side[-1] if rng.random() < 0.25 else -side[-1])
            sides[line.id] = (np.array(knots), np.array(side))
            arc = line.arc[(line.arc >= s0) & (line.arc <= s1)]
            shelf[line.id] = (arc, knot_noise(rng, arc, (15.0, 40.0), 1.6, 3.2))
            for k in range(1, len(knots)):
                if side[k] == side[k - 1] or knots[k] >= s1:
                    continue
                w = float(np.interp(knots[k], line.arc, prof.half_width))
                b = float(np.interp(knots[k], *shelf[line.id]))
                q0, q1 = side[k - 1] * (w - b), side[k] * (w - b)
                n = max(1, int(np.ceil(abs(q1 - q0) / 3.2)) - 1)
                for m in range(1, n + 1):
                    t = m / (n + 1)
                    hw = float(rng.uniform(0.9, 1.2))
                    islands.append(Island(line.id, knots[k] + (t - 0.5) * 2.5 * n, q0 + (q1 - q0) * t,
                                          float(rng.uniform(1.9, 2.4)) * hw, hw))
            count = int(model.style.island_per_100m * (s1 - s0) / 100.0)
            for _ in range(count):
                s = float(rng.uniform(s0 + 5.0, s1 - 5.0))
                w = float(np.interp(s, line.arc, prof.half_width))
                hw = float(rng.uniform(0.8, 2.0))
                q = float(rng.choice([-1.0, 1.0])) * float(rng.uniform(0.3, 0.7)) * w
                islands.append(Island(line.id, s, q, float(rng.uniform(1.8, 3.0)) * hw, hw))
    if not sides:
        return None
    return River(sides, shelf, islands)


def river_floor(model, X, Y, i, zf, w):
    """(suelo del rio, peso 0..1 del rio) en cada punto, segun la muestra mas cercana i."""
    S, river = model.S, model.river
    line, s, p, n = S["line"][i], S["s"][i], S["p"][i], S["n"][i]
    q = (X - p[..., 0]) * n[..., 0] + (Y - p[..., 1]) * n[..., 1]
    bed = WATER_M - 1.2 - 0.5 * model.n_floor.unit(X, Y)
    floor = bed.copy()
    weight = np.zeros(X.shape)
    for line_id, (knots, side) in river.sides.items():
        on = line == line_id
        if not on.any():
            continue
        k = np.clip(np.searchsorted(knots, s, side="right") - 1, 0, len(side) - 1)
        b = np.interp(s, *river.shelf[line_id])
        bank = smooth(-0.6, 0.6, side[k] * q - (w - b) + 0.4 * model.n_top(X, Y))
        floor = np.where(on, bed * (1.0 - bank) + BANK_TOP_M * bank, floor)
        arc = river.shelf[line_id][0]
        weight = np.where(on & (s >= arc[0]) & (s <= arc[-1]), 1.0, weight)
    for isl in river.islands:
        on = line == isl.line
        r = np.sqrt(((s - isl.s) / isl.half_len) ** 2 + ((q - isl.q) / isl.half_wid) ** 2)
        top = BANK_TOP_M + 0.15 * model.n_floor(X, Y)
        floor = np.where(on & (r < 1.0), np.maximum(floor, bed + (top - bed) * (1.0 - smooth(0.7, 1.0, r))), floor)
    return floor, weight

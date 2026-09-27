"""Rio como tramo del camino: un cauce hondo que serpentea entre dos orillas de arena andables
(de anchura cambiante, a veces casi desaparece una), con islas alargadas en el sentido del agua,
a un lado y a otro del cauce y nunca en ristra por el centro. Se va andando por las orillas y
se puede saltar por las islas."""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

from terrain_vol.density import smooth

from .curves import knot_noise
from .layout import WATER_M

BANK_TOP_M = WATER_M + 0.7
BANK_M = (1.0, 2.8)              # anchura de cada orilla (m), cambia a lo largo del rio
MIN_CHANNEL_M = 1.5              # cauce mas estrecho que esto: no hay cauce (todo orilla)
STREAM_BED_M = WATER_M - 0.35
LAGOON_EXTRA = 1.5               # islas de mas por metro de laguna (sobre island_per_100m)    # fondo de un arroyo: se cruza andando con el agua por los tobillos


@dataclass
class Island:
    line: int
    s: float
    q: float          # desplazamiento lateral (m) desde el eje, con signo
    half_len: float
    half_wid: float


@dataclass
class River:
    banks: dict[int, tuple[np.ndarray, np.ndarray, np.ndarray]]      # linea -> (arco, orilla izq., orilla der.)
    islands: list[Island]
    shallow: frozenset = frozenset()     # lineas cuyo cauce es un arroyo vadeable


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


def channel(w, left, right):
    """(borde izquierdo, borde derecho) del cauce en coordenada lateral q (izquierda negativa)."""
    rc = np.minimum(1.5, 0.2 * w)
    return -(w - rc) + left, (w - rc) - right


def plan_river(rng: np.random.Generator, model) -> River | None:
    banks, islands = {}, []
    # El principal lleva rio. Un lazo que pasa por el agua es un paso seco a la cota de la orilla
    # (con rio, sus orillas no siempre enlazaban con las del principal), salvo los arroyos: su
    # cauce es poco hondo y se anda por dentro, asi que no hace falta enlazar orillas.
    lines = [line for line in model.plan.graph.lines if line.id == 0 or model.plan.profiles[line.id].stream]
    shallow = frozenset(line.id for line in lines if line.id != 0)
    for line in lines:
        prof = model.plan.profiles[line.id]
        segments = [seg for seg in _segments(prof.biome == 1, line.arc) if seg[1] - seg[0] >= 20.0]
        if not segments:
            continue
        left = knot_noise(rng, line.arc, (15.0, 40.0), *BANK_M)
        right = knot_noise(rng, line.arc, (15.0, 40.0), *BANK_M)
        banks[line.id] = (line.arc, left, right)
        if line.id in shallow:
            continue                                  # el arroyo no lleva islas
        lagoon = prof.lagoon if prof.lagoon is not None else np.zeros(len(line.arc))
        for s0, s1 in segments:
            wide = (line.arc >= s0) & (line.arc <= s1) & (lagoon > 0.5)
            wide_m = float(np.count_nonzero(wide)) * float(np.median(np.diff(line.arc)))
            count = int(model.style.island_per_100m * ((s1 - s0) + LAGOON_EXTRA * wide_m) / 100.0)
            for _ in range(count * 3):
                if count <= 0:
                    break
                s = float(rng.uniform(s0 + 5.0, s1 - 5.0))
                w = float(np.interp(s, line.arc, prof.half_width))
                lo, hi = channel(w, float(np.interp(s, line.arc, left)), float(np.interp(s, line.arc, right)))
                if hi - lo < 5.0:
                    continue
                if float(np.interp(s, line.arc, lagoon)) > 0.5:
                    # Laguna: islitas repartidas por todo el agua, algo mayores.
                    hw = float(rng.uniform(0.8, 2.2))
                    q = float(rng.uniform(lo + hw + 1.5, hi - hw - 1.5))
                else:
                    hw = float(rng.uniform(0.6, min(1.6, 0.2 * (hi - lo))))
                    # Hacia una orilla del cauce, nunca en el centro.
                    third = float(rng.choice([-1.0, 1.0]))
                    q = 0.5 * (lo + hi) + third * float(rng.uniform(0.28, 0.42)) * (hi - lo)
                islands.append(Island(line.id, s, q, float(rng.uniform(1.9, 3.2)) * hw, hw))
                count -= 1
    if not banks:
        return None
    return River(banks, islands, shallow)


def river_floor(model, X, Y, i, zf, w):
    """(suelo del rio, peso 0..1 del rio) en cada punto, segun la muestra mas cercana i. El peso
    se apaga donde la cota del camino queda por encima de la orilla (entrada y salida del tramo)."""
    S, river = model.S, model.river
    line, s, p, n = S["line"][i], S["s"][i], S["p"][i], S["n"][i]
    q = (X - p[..., 0]) * n[..., 0] + (Y - p[..., 1]) * n[..., 1]
    bed = WATER_M - 1.2 - 0.5 * model.n_floor.unit(X, Y)
    floor = np.full(X.shape, BANK_TOP_M) + 0.1 * model.n_floor(X, Y)
    weight = np.zeros(X.shape)
    for line_id, (arc, left, right) in river.banks.items():
        on = (line == line_id) & (S["biome"][i] == 1)
        if not on.any():
            continue
        lo, hi = channel(w, np.interp(s, arc, left), np.interp(s, arc, right))
        wobble = 0.4 * model.n_top(X, Y)
        inside = smooth(-0.6, 0.6, q - lo + wobble) * smooth(-0.6, 0.6, hi - q + wobble)
        inside = inside * smooth(MIN_CHANNEL_M - 0.5, MIN_CHANNEL_M + 0.5, hi - lo)
        line_bed = STREAM_BED_M - 0.1 * model.n_floor.unit(X, Y) if line_id in river.shallow else bed
        floor = np.where(on, floor * (1.0 - inside) + line_bed * inside, floor)
        weight = np.where(on, 1.0, weight)
    weight = weight * (1.0 - smooth(0.3, 1.2, zf - BANK_TOP_M))
    for isl in river.islands:
        on = line == isl.line
        r = np.sqrt(((s - isl.s) / isl.half_len) ** 2 + ((q - isl.q) / isl.half_wid) ** 2)
        top = BANK_TOP_M + 0.15 * model.n_floor(X, Y)
        floor = np.where(on & (r < 1.0), np.maximum(floor, bed + (top - bed) * (1.0 - smooth(0.7, 1.0, r))), floor)
    return floor, weight

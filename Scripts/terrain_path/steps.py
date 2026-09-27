"""Escalones de medusa: tramos del camino principal que suben de golpe style.jump_step_m (mas de
lo que se salta) y vuelven a bajar de golpe (se baja saltando). Al pie de cada escalon va una
medusa (rebote): es la unica forma de subir. Azar propio (el grafo, los lazos y los cruces no
cambian).

Cada escalon: cara vertical en s0, rellano elevado hasta s1 y caida en s1 (s_end = s1). Sin
rampa: el principal apenas tiene tramos largos libres de uniones."""

from __future__ import annotations

from dataclasses import dataclass, replace

import numpy as np

JELLY_BACK_M = 4.0          # la medusa, a esta distancia del pie de la cara (en el eje del camino)
LINK_M = 4.0                # punto de llegada, pasada la cara (lo usa la comprobacion de recorrido)
MARGIN_BEFORE_M = 7.0       # sin uniones, cruces, tuneles ni arcos tan cerca antes de la cara
MARGIN_AFTER_M = 4.0        # ni tan cerca despues de la rampa
CANYON_CLEAR_M = 60.0
MIN_GAP_M = 40.0            # entre dos escalones


@dataclass(frozen=True)
class JumpStep:
    s0: float                # cara del escalon (arco del principal)
    s1: float                # fin del rellano elevado
    s_end: float             # fin de la rampa de bajada
    height: float
    jelly: tuple[float, float, float]      # medusa (m): al pie de la cara, sobre el suelo bajo
    top: tuple[float, float, float]        # llegada (m): sobre el rellano
    drop_top: tuple[float, float, float]   # borde de la caida (m), sobre el rellano
    drop_bottom: tuple[float, float, float]  # pie de la caida (m)


def _blocked(model) -> list[tuple[float, float]]:
    """Tramos (arco del principal) donde no puede haber escalon."""
    plan, main = model.plan, model.plan.graph.main
    out = [(loop.s_out, loop.s_out) for loop in plan.graph.loops() if loop.parent == 0]
    out += [(loop.s_back, loop.s_back) for loop in plan.graph.loops() if loop.parent == 0]
    out += [(c.s_upper, c.s_upper) if c.upper == 0 else (c.s_lower, c.s_lower)
            for c in plan.crossings if 0 in (c.upper, c.lower)]
    out += [(a, b) for lid, a, b in plan.hill_tunnels if lid == 0]
    out += [(a, b) for lid, a, b in model.deck_cuts if lid == 0]
    out += [(a, b) for lid, a, b in model.arch_ranges if lid == 0]
    for c in model.canyons:
        out.append((c.s_main - CANYON_CLEAR_M, c.s_main + CANYON_CLEAR_M))
    tunnel = plan.profiles[0].tunnel
    if tunnel.any():
        arc = main.arc
        for run in np.split(np.arange(len(arc)), np.nonzero(np.diff(tunnel.astype(int)))[0] + 1):
            if tunnel[run[0]]:
                out.append((float(arc[run[0]]), float(arc[run[-1]])))
    return out


def plan_steps(model, rng: np.random.Generator) -> list[JumpStep]:
    style = model.style
    count = getattr(style, "jump_steps", 0)
    if count <= 0:
        return []
    main, prof = model.plan.graph.main, model.plan.profiles[0]
    height = style.jump_step_m
    ramp = 0.0
    blocked = _blocked(model)
    grid = np.arange(60.0, main.length - 60.0, 1.0)
    steps: list[JumpStep] = []
    for _ in range(count):
        ledge = float(rng.uniform(*style.jump_ledge_m))
        lo, hi = grid - MARGIN_BEFORE_M, grid + ledge + ramp + MARGIN_AFTER_M
        ok = np.ones(len(grid), dtype=bool)
        for a, b in blocked:
            ok &= (b < lo) | (a > hi)
        for st in steps:
            ok &= (hi + MIN_GAP_M < st.s0) | (lo > st.s_end + MIN_GAP_M)
        k_lo = np.searchsorted(main.arc, lo)
        k_hi = np.searchsorted(main.arc, hi)
        dry = np.isin(prof.biome, (0, 2))
        bad = np.concatenate([[0], np.cumsum(~dry)])
        ok &= (bad[np.minimum(k_hi + 1, len(dry))] - bad[k_lo]) == 0
        if not ok.any():
            break
        s0 = float(rng.choice(grid[ok]))
        s1, s_end = s0 + ledge, s0 + ledge + ramp
        z_low = float(np.interp(s0 - JELLY_BACK_M, main.arc, prof.z))
        z_top = float(np.interp(s0 + LINK_M, main.arc, prof.z)) + height
        z_drop = float(np.interp(s1 + LINK_M, main.arc, prof.z))
        steps.append(JumpStep(s0, s1, s_end, height, (*map(float, main.point_at(s0 - JELLY_BACK_M)), z_low),
                              (*map(float, main.point_at(s0 + LINK_M)), z_top),
                              (*map(float, main.point_at(s1 - LINK_M)), z_drop + height),
                              (*map(float, main.point_at(s1 + LINK_M)), z_drop)))
    return sorted(steps, key=lambda st: st.s0)


def lift(arc: np.ndarray, steps: list[JumpStep]) -> np.ndarray:
    """Subida de cada punto del principal: 'height' en el rellano, 0 fuera."""
    out = np.zeros(len(arc))
    for st in steps:
        out += st.height * ((arc >= st.s0) & (arc <= st.s1))
    return out


def raise_steps(profile, arc: np.ndarray, steps: list[JumpStep]):
    return replace(profile, z=profile.z + lift(arc, steps))

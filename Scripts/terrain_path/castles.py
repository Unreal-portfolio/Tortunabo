"""Castillos de arena como estampa del relieve (prueba: si a 1 m de resolucion no quedan bien,
pasan a ser assets de los disenadores). Muralla redonda con puerta hacia el camino y cuatro
torres, en un ensanche, pegado a un lado para dejar paso."""

from __future__ import annotations

import math
from dataclasses import dataclass

import numpy as np

from terrain_vol.density import smooth

CASTLE_RADIUS_M = 3.0
MIN_WIDTH_M = 4.5              # semiancho minimo donde se planta (el camino se ensancha alrededor)
PLAZA_HALF_WIDTH_M = 7.5       # semiancho del ensanche del castillo
PLAZA_LENGTH_M = 15.0          # medio largo del ensanche (con transicion)


@dataclass
class Castle:
    center: np.ndarray
    facing: np.ndarray        # unitario, del castillo hacia el centro del camino
    base: float


def plan_castles(rng: np.random.Generator, model) -> list[Castle]:
    S = model.S
    plan = model.plan
    main = plan.graph.main
    ok = S["open"] & (S["w"] >= MIN_WIDTH_M) & (S["biome"] != 1)
    on_main = S["line"] == 0
    ok &= ~on_main | ((S["s"] > 40.0) & (S["s"] < main.length - 80.0))
    blocked = [c.point for c in plan.crossings] + [line.points[k] for line in plan.graph.loops() for k in (0, -1)]
    candidates = np.nonzero(ok)[0]
    rng.shuffle(candidates)
    out: list[Castle] = []
    for idx in candidates:
        if len(out) >= model.style.castles:
            break
        p = S["p"][idx]
        if any(np.hypot(*(p - q)) < 25.0 for q in blocked) or any(np.hypot(*(p - c.center)) < 40.0 for c in out):
            continue
        if model.tunnel_tree is not None and model.tunnel_tree.query(p)[0] < 25.0:
            continue
        side = float(rng.choice([-1.0, 1.0]))
        n = S["n"][idx]
        # Ensanche del camino alrededor del castillo: el castillo ocupa un lado y el paso queda
        # en el otro (modifica el semiancho de las muestras de esa linea).
        near = (S["line"] == S["line"][idx]) & (np.abs(S["s"] - S["s"][idx]) < PLAZA_LENGTH_M)
        taper = 1.0 - smooth(PLAZA_LENGTH_M * 0.5, PLAZA_LENGTH_M, np.abs(S["s"][near] - S["s"][idx]))
        S["w"][near] = np.maximum(S["w"][near], S["w"][near] + (PLAZA_HALF_WIDTH_M - S["w"][near]) * taper)
        center = p + n * side * (PLAZA_HALF_WIDTH_M - CASTLE_RADIUS_M - 0.8)
        out.append(Castle(center, -n * side, float(S["z"][idx])))
    return out


def castle_stamp(X, Y, height, castle: Castle):
    dx, dy = X - castle.center[0], Y - castle.center[1]
    dist = np.hypot(dx, dy)
    if dist.min() > CASTLE_RADIUS_M + 3.0:
        return height
    # Se asienta en el suelo real del camino en su centro (no en la cota de la muestra: con la
    # rugosidad y la pendiente difieren unos decimetros).
    castle.base = float(height.flat[int(np.argmin(dist))])
    yaw = math.atan2(castle.facing[1], castle.facing[0])
    ang = np.abs((np.arctan2(dy, dx) - yaw + math.pi) % (2.0 * math.pi) - math.pi)
    gate = smooth(0.8, 1.2, ang / (1.4 / CASTLE_RADIUS_M))
    wall = (1.0 - smooth(0.45, 0.9, np.abs(dist - CASTLE_RADIUS_M))) * gate
    out = np.where(wall > 0.0, np.maximum(height, castle.base + 1.3 * wall), height)
    for k in range(4):
        a = yaw + math.pi / 4.0 + k * math.pi / 2.0
        tx = castle.center[0] + CASTLE_RADIUS_M * math.cos(a)
        ty = castle.center[1] + CASTLE_RADIUS_M * math.sin(a)
        tower = 1.0 - smooth(0.9, 1.4, np.hypot(X - tx, Y - ty))
        out = np.where(tower > 0.0, np.maximum(out, castle.base + 2.6 * tower), out)
    return out

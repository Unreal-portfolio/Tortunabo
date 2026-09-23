"""Modulos de campo: explanadas de arena sin paredes y mar con islas.

Llevan lados de tipo "open" o "water" (ver core.EDGE_TYPES): por esos lados el modulo se
funde con su vecino sin pared, y el jugador puede cruzar por cualquier punto. Los lados
"crest" que les queden (hacia otra region o hacia fuera del mapa) levantan la cresta de
siempre con su boca de pasillo en el centro.
"""

from __future__ import annotations


import numpy as np

from .core import *  # noqa: F401,F403
from .features import chaikin, dunes, island_chain

# Patrones de lados abiertos (el resto son cresta). El generador de mapa rota el modulo.
FIELD_PATTERNS: dict[str, tuple[str, ...]] = {
    "O1": ("N",),
    "O2a": ("N", "E"),
    "O2o": ("N", "S"),
    "O3": ("N", "E", "S"),
    "O4": ("N", "E", "S", "W"),
}
CREST_BAND_M = (26.0, 8.0)        # la pared de un lado cresta sube entre estas distancias al borde
MOUTH_LANE_M = 55.0               # tramo de pasillo que abre cada boca de un lado cresta
CORNER_MOUND_M = (10.0, 34.0)     # loma de roca en cada esquina (los bordes valen CREST_M alli)


def side_distance(side: str):
    return {"N": HALF_M - XX, "S": HALF_M + XX, "E": HALF_M - YY, "W": HALF_M + YY}[side]


def design_field_module(rng: np.random.Generator, edges: dict[str, str], biome: str,
                        route: tuple[str, ...] = SIDES, secondary: str | None = None):
    """Heightfield (metros) de un modulo de campo, antes de fundirlo con sus bordes.

    route: lados por los que pasa el camino. En el mar solo hay islas hacia esos lados (un
    unico camino por la region de agua); un lado cresta fuera del camino se sella. En la
    explanada todo el llano es camino. secondary: bioma con el que funde un modulo de
    transicion (playa junto a las bocas de cresta del camino)."""
    secondary = secondary or biome
    kind = "water" if "water" in edges.values() else "open"
    crest_sides = [s for s in SIDES if edges[s] == "crest"]
    edge_fade = smoothstep(0.0, 30.0, DIST_TO_EDGE)

    swell = 1.5 * fbm(rng, 120.0, octaves=2) * edge_fade
    flat_areas = []
    islands: list[Room] = []
    if kind == "open":
        # Explanada: dunas grandes y alguna roca baja; nada de paredes.
        ground = swell + dunes(rng, float(rng.uniform(1.5, 3.5)), float(rng.uniform(22.0, 40.0))) * edge_fade
        outcrops = 1.8 * smoothstep(0.7, 0.85, value_noise(rng, 30.0) * 0.5 + 0.5) * edge_fade
        ground += outcrops
        plaza = Room(0.0, 0.0, float(rng.uniform(16.0, 26.0)))
        m = 1.0 - smoothstep(plaza.radius - 6.0, plaza.radius + 6.0, np.hypot(XX, YY))
        ground = ground * (1.0 - m) + float(ground[grid_index((0.0, 0.0))]) * m
        flat_areas.append({"x_m": 0.0, "y_m": 0.0, "radius_m": round(plaza.radius, 2),
                           "height_m": round(float(ground[grid_index((0.0, 0.0))]), 2), "sunken": False})
    else:
        # Mar poco profundo con islas en fila desde el centro hacia cada lado: se salta de
        # una a otra (o se vadea: por ahora el agua no tiene logica).
        ground = SEA_EDGE_M + 0.6 * swell
        central = Room(0.0, 0.0, float(rng.uniform(14.0, 22.0)))
        islands = [central]
        for side in route:
            if edges[side] == "crest":
                continue
            end = scaled(EXIT_POINT[side], 0.8)
            start = scaled(EXIT_POINT[side], (central.radius + 4.0) / HALF_M)
            islands += island_chain(rng, start, end, (6.0, 11.0), (3.0, 5.0))
        # Algun islote suelto fuera de las filas.
        for _ in range(int(rng.integers(0, 4))):
            x, y = (float(v) for v in rng.uniform(-0.7 * HALF_M, 0.7 * HALF_M, 2))
            islands.append(Room(x, y, float(rng.uniform(5.0, 9.0))))
        # Costas irregulares: las islas se miden en un espacio deformado por ruido.
        wx = XX + 5.0 * fbm(rng, 30.0, octaves=2)
        wy = YY + 5.0 * fbm(rng, 30.0, octaves=2)
        land = np.zeros_like(XX)
        for island in islands:
            d = np.hypot(wx - island.x, wy - island.y)
            land = np.maximum(land, 1.0 - smoothstep(island.radius - 5.0, island.radius + 5.0, d))
        top = 0.6 + 0.8 * (fbm(rng, 25.0, octaves=2) * 0.5 + 0.5)
        ground = ground * (1.0 - land) + top * land
        flat_areas.append({"x_m": 0.0, "y_m": 0.0, "radius_m": round(central.radius, 2),
                           "height_m": round(float(ground[grid_index((0.0, 0.0))]), 2), "sunken": False})

    # Lados cresta: pared con su boca y un tramo de pasillo hacia el centro. En el mar el
    # tramo es una calzada a cota 0 que llega a la isla central.
    terrain = ground
    blend = np.zeros_like(XX)
    keep = np.zeros_like(XX)
    if kind == "water":
        keep = np.maximum(keep, land)     # las islas (el camino) no se hunden
    else:
        keep = np.maximum(keep, 1.0 - smoothstep(plaza.radius, plaza.radius + 10.0, np.hypot(XX, YY)))
    for side in crest_sides:
        band = smoothstep(CREST_BAND_M[0], CREST_BAND_M[1], side_distance(side))
        wall = CREST_M + float(rng.uniform(0.0, 6.0)) * (fbm(rng, 60.0, octaves=2) * 0.5 + 0.5)
        if kind == "water":
            # El mar acaba en arena: playa baja y una duna que sube hasta la cresta del
            # borde, con la cara alta empinada (no se trepa al modulo vecino).
            # Linea de playa irregular: entrantes y salientes de hasta ~18 m.
            shore = side_distance(side) + 18.0 * fbm(rng, 55.0, octaves=2) * smoothstep(8.0, 24.0, side_distance(side))
            beach = smoothstep(62.0, 34.0, shore)
            dune = 0.8 + (CREST_M + 1.5 - 0.8) * smoothstep(34.0, 6.0, shore) ** 1.8
            wall = dune + 1.2 * fbm(rng, 40.0, octaves=2) * smoothstep(34.0, 12.0, shore)
            band = beach
        if side not in route:
            # Sin camino: pared entera y la boca del borde canonico sellada por dentro.
            terrain = terrain * (1.0 - band) + wall * band
            ex, ey = EXIT_POINT[side]
            along = np.abs(YY) if side in ("N", "S") else np.abs(XX)
            plug = (along < OPEN_HALF_M + BANK_M + 6.0) & (np.hypot(XX - ex, YY - ey) < 30.0)
            seal = CREST_M + 2.0 if kind == "water" else PLUG_M   # en la playa, una duna; en tierra, roca
            terrain = np.where(plug, np.maximum(terrain, seal), terrain)
            continue
        if secondary != biome:
            blend = np.maximum(blend, 1.0 - smoothstep(20.0, 75.0, side_distance(side)))
        lane_end = scaled(EXIT_POINT[side], 1.0 - MOUTH_LANE_M / HALF_M)
        d_lane = polyline_distance(XX, YY, (lane_end, EXIT_POINT[side]))[0]
        hw = OPEN_HALF_M + float(rng.uniform(0.0, 4.0))
        lane = 1.0 - smoothstep(hw, hw + BANK_M, d_lane)
        terrain = terrain * (1.0 - band * (1.0 - lane)) + wall * band * (1.0 - lane)
        if kind == "water":
            # Calzada que serpentea hasta la isla central (recta solo en la boca).
            bend = scaled(perpendicular(EXIT_DIR[side]), float(rng.uniform(-25.0, 25.0)))
            path = ((0.0, 0.0), added(scaled(EXIT_POINT[side], 0.45), bend), scaled(EXIT_POINT[side], 0.8), EXIT_POINT[side])
            causeway = 1.0 - smoothstep(5.0, 9.0, polyline_distance(XX, YY, chaikin(path, 3))[0])
            terrain = np.maximum(terrain, 0.3 * causeway + SEA_EDGE_M * (1.0 - causeway))
            keep = np.maximum(keep, causeway)
        keep = np.maximum(keep, lane)

    # Loma de roca en cada esquina, donde todos los bordes suben a CREST_M.
    for cx in (-HALF_M, HALF_M):
        for cy in (-HALF_M, HALF_M):
            weight = 1.0 - smoothstep(*CORNER_MOUND_M, np.hypot(XX - cx, YY - cy))
            terrain = terrain * (1.0 - weight) + np.maximum(terrain, CREST_M) * weight

    terrain = blur(terrain)
    stats = {
        "style": "flats" if kind == "open" else "islands",
        "biome": biome,
        "secondary_biome": secondary,
        "causeway": False,
        "rooms": len(flat_areas),
        "sunken": 0,
        "shortcut": False,
        "upper_route": False,
        "fork": "none",
        "arch": False,
        "tunnel": False,
        "cliff": False,
        "maze": False,
        "gate": "normal",
        "tunnel_ramp": False,
        "islands": len(islands),
        "wall_h": 0.0,
        "base_half_width": 0.0,
    }
    return terrain, stats, [], flat_areas, [], blend, np.zeros_like(XX), 1.0 - keep

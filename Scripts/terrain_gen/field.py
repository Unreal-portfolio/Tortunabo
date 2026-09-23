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


def side_distance(side: str):
    return {"N": HALF_M - XX, "S": HALF_M + XX, "E": HALF_M - YY, "W": HALF_M + YY}[side]


def open_relief(rng: np.random.Generator, route: tuple[str, ...]):
    """(terreno, plaza, valles) de una region abierta de arena: montes y lomas en vez de
    llano, sin paredes en los lados. Unos valles de laderas suaves unen el centro con cada
    lado del camino (serpentean, y a veces un segundo valle rodea un monte: varias rutas
    en el mismo modulo); fuera de ellos el relieve sube a montes de 10-24 m que se pueden
    subir por casi todas partes. Hacia el borde el relieve se funde, en una franja ancha,
    con el perfil de cada lado (lomas del borde open o cresta con su boca), asi que no
    queda ningun escalon ni recta a lo largo de la linea de la celda."""
    amplitude = float(rng.uniform(10.0, 24.0))
    broad = fbm(rng, 130.0, octaves=3) * 0.5 + 0.5
    ridged = 1.0 - np.abs(fbm(rng, 85.0, octaves=2))
    mountains = amplitude * (0.6 * broad + 0.4 * ridged ** 2)
    mountains += dunes(rng, float(rng.uniform(0.8, 2.0)), float(rng.uniform(20.0, 34.0)))

    # Valles: del centro a cada lado del camino, con un recodo; a veces uno extra que
    # une dos lados rodeando un monte.
    lanes: list[tuple[Point, ...]] = []
    for side in route:
        bend = scaled(perpendicular(EXIT_DIR[side]), float(rng.uniform(-30.0, 30.0)))
        lanes.append(chaikin(((0.0, 0.0), added(scaled(EXIT_POINT[side], 0.5), bend),
                              scaled(EXIT_POINT[side], 0.85), EXIT_POINT[side]), 3))
    if len(route) >= 2 and rng.random() < 0.6:
        a, b = (route[int(k)] for k in rng.choice(len(route), 2, replace=False))
        corner = scaled(added(EXIT_POINT[a], EXIT_POINT[b]), float(rng.uniform(0.35, 0.55)))
        lanes.append(chaikin((scaled(EXIT_POINT[a], 0.6), corner, scaled(EXIT_POINT[b], 0.6)), 3))
    d_valley = np.full_like(XX, np.inf)
    for lane in lanes:
        d_valley = np.minimum(d_valley, polyline_distance(XX, YY, lane)[0])
    valley_hw = float(rng.uniform(9.0, 15.0))
    valleys = 1.0 - smoothstep(valley_hw, valley_hw + float(rng.uniform(22.0, 34.0)), d_valley)
    valley_floor = 0.8 * fbm(rng, 60.0, octaves=2)
    ground = mountains * (1.0 - valleys) + valley_floor * valleys

    plaza = Room(0.0, 0.0, float(rng.uniform(12.0, 18.0)))
    m = 1.0 - smoothstep(plaza.radius - 5.0, plaza.radius + 5.0, np.hypot(XX, YY))
    ground = ground * (1.0 - m) + float(ground[grid_index((0.0, 0.0))]) * m
    # Sin franja de borde: el relieve llega al borde tal cual y la fusion de bordes del
    # tile lo casa con el vecino.
    return ground, plaza, valleys


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
        ground, plaza, valleys = open_relief(rng, route)
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
        # Sin islotes sueltos: en el mar el camino es uno y cualquier isla fuera de la fila
        # se lee como una segunda ruta.
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
        keep = np.maximum(keep, valleys)
    for side in crest_sides:
        if kind == "open":
            # Sin pared: el relieve llega al borde tal cual (open_relief); los obstaculos que
            # cierren la region los pone el equipo.
            continue
        # El mar acaba en un cordon de dunas de costa y cima irregulares (natural_ridge).
        wall, band = natural_ridge(rng, side_distance(side), 0.8)
        if side not in route:
            # Sin camino: cordon de dunas entero.
            terrain = terrain * (1.0 - band) + wall * band
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

    # Sin lomas en las esquinas: el agua de cuatro modulos que se tocan en una esquina se
    # une en la fusion de bordes.
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

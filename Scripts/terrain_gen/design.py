"""Diseno de un modulo completo y su composicion con el borde."""

from __future__ import annotations

import math
from dataclasses import dataclass
from pathlib import Path

import numpy as np

from .core import *  # noqa: F401,F403
from .features import *  # noqa: F401,F403
from .styles import STYLES


def design_module(rng: np.random.Generator, exits: tuple[str, ...], biome: str, secondary: str):
    """Heightfield del interior (metros) antes de fundirlo con el borde canonico.

    biome manda en la forma (estilo); secondary != biome hace un modulo mixto: un frente
    difuso (blend) lleva hacia el otro bioma el color, el bosque de algas y el mar."""
    edge_fade = smoothstep(0.0, 50.0 * K, DIST_TO_EDGE)
    style_name, style = pick_style(rng, biome)
    is_causeway = bool(style["causeway"])
    wall_h = float(rng.uniform(*style["wall_h"]))
    blend = biome_blend_field(rng) if secondary != biome else np.zeros_like(XX)

    def weight_of(name: str):
        return (1.0 - blend) * float(biome == name) + blend * float(secondary == name)

    # Pasillo con serpenteo (deformacion del dominio) que se apaga junto al borde. Todas
    # las rutas se miden en el mismo espacio deformado, asi que casan entre si.
    warp_amp = float(rng.uniform(35.0, 65.0)) * K * smoothstep(0.0, 70.0 * K, DIST_TO_EDGE)
    wx = XX + warp_amp * fbm(rng, 180.0 * K, octaves=2)
    wy = YY + warp_amp * fbm(rng, 180.0 * K, octaves=2)

    # Bifurcacion: ensancha el tramo de su salida antes de medir el pasillo.
    fork = pick_fork(rng, exits)
    fork_widen, island = (fork_fields(rng, fork, wx, wy) if fork else (0.0, np.zeros_like(XX)))
    d_main = np.full_like(XX, np.inf)
    for exit_side, lane in zip(exits, main_lanes(exits)):
        d_lane = polyline_distance(wx, wy, lane.points)[0]
        if fork and exit_side == fork.exit:
            d_lane = d_lane - fork_widen
        d_main = np.minimum(d_main, d_lane)

    base_hw = float(rng.uniform(*style["corridor_hw"])) * K
    hw = base_hw * (1.0 + 0.3 * value_noise(rng, 110.0 * K))
    hw = OPEN_HALF_M + (hw - OPEN_HALF_M) * edge_fade   # en el borde, la boca canonica
    bank = float(rng.uniform(*style["bank"]))
    bank = BANK_M + (bank - BANK_M) * edge_fade

    # Acantilado: en su tramo el talud se vuelve casi vertical (ver la meseta mas abajo).
    cliff = cliff_field(rng, exits, wx, wy, hw) if rng.random() < style["cliff_prob"] else np.zeros_like(XX)
    cliff = cliff * edge_fade
    bank = bank * (1.0 - cliff) + CLIFF_BANK_M * cliff

    # El tunel se decide antes que las plazas: en 200 m no cabe junto a una plaza lateral.
    wants_tunnel = not is_causeway and rng.random() < style["tunnel_prob"]
    tunnel_exit = exits[int(rng.integers(len(exits)))] if wants_tunnel else None
    rooms = pick_rooms(rng, exits, style["sunken_prob"], tunnel_exit)
    rmask = room_mask(rooms)
    corridor = np.maximum(1.0 - smoothstep(hw, hw + bank, d_main), rmask)
    # Nucleo duro del pasillo (sin talud): la ruta alta corta el talud entero y acaba en
    # un labio casi vertical, y el tablero salva exactamente el hueco entre labios.
    core = np.maximum(1.0 - smoothstep(hw - 5.0, hw - 2.0, d_main), room_mask(rooms, 2.0))
    d_core = d_main - (hw - 2.0)
    for room in rooms:
        d_core = np.minimum(d_core, np.hypot(XX - room.x, YY - room.y) - room.radius)

    # Atajo: misma anchura relativa (con el mismo ruido) y mismo talud que el principal.
    shortcut = pick_shortcut(rng, exits, base_hw)
    if shortcut:
        d_sc = polyline_distance(wx, wy, shortcut.points)[0]
        sc_hw = np.minimum(hw * shortcut.half_width / base_hw, hw)
        corridor = np.maximum(corridor, 1.0 - smoothstep(sc_hw, sc_hw + bank, d_sc))
        core = np.maximum(core, 1.0 - smoothstep(sc_hw - 4.0, sc_hw - 1.0, d_sc))
        d_core = np.minimum(d_core, d_sc - (sc_hw - 1.0))

    # La isla de la bifurcacion se queda fuera del pasillo: toma la cota de la meseta (roca
    # alta) o se rebaja despues a una loma escalable.
    corridor = corridor * (1.0 - island)
    core = core * (1.0 - island)

    # Suelo: cota que ondula despacio y se aplana en las plazas; 0 junto a las salidas.
    # Una plaza hundida baja sunk_m con una rampa ancha (SUNKEN_FEATHER_M) que se camina.
    elev_amp = float(rng.uniform(4.0, 9.0))
    elev = elev_amp * fbm(rng, 260.0 * K, octaves=2) * edge_fade
    floor = elev.copy()
    # Fuera del bioma de agua la plaza hundida no baja del agua: seria un charco, no una hondonada.
    sunk_floor = WATER_M + 1.0 if biome != "water" else -np.inf
    sunken_rooms = set()
    for index, room in enumerate(rooms):
        i, j = grid_index((room.x, room.y))
        sunk = max(0.0, min(room.sunk_m, float(elev[i, j]) - sunk_floor))
        if sunk >= 2.0:
            sunken_rooms.add(index)
        else:
            sunk = 0.0
        feather = SUNKEN_FEATHER_M if sunk > 0.0 else 6.0
        m = 1.0 - smoothstep(room.radius - feather, room.radius + feather, np.hypot(XX - room.x, YY - room.y))
        floor = floor * (1.0 - m) + (elev[i, j] - sunk) * m
    flat_areas = [{"x_m": round(r.x, 2), "y_m": round(r.y, 2), "radius_m": round(r.radius, 2),
                   "height_m": round(float(floor[grid_index((r.x, r.y))]), 2), "sunken": k in sunken_rooms}
                  for k, r in enumerate(rooms)]
    floor += 0.30 * fbm(rng, 18.0, octaves=2)                                  # arena
    off_lane = smoothstep(8.0, 13.0, d_main) * (1.0 - 0.6 * rmask)
    rocks = float(rng.uniform(*style["rocks"])) * smoothstep(*style["rock_threshold"], value_noise(rng, 22.0) * 0.5 + 0.5) * off_lane
    floor += rocks * edge_fade
    if style["puddles"]:
        # Charcos de marisma: bajan por debajo del agua fuera del carril central.
        puddles = 5.5 * smoothstep(0.55, 0.75, value_noise(rng, 40.0) * 0.5 + 0.5) * off_lane * edge_fade
        floor -= puddles
    floor -= pits(rng, rooms, d_main, hw, style["pits"])
    # Fuera del agua, pozos y charcos no bajan de la cota del mar: serian lagunas en el desierto.
    floor = floor + (np.maximum(floor, WATER_M + 0.5) - floor) * (1.0 - weight_of("water"))

    # Meseta: cresta sobre el suelo local + colinas suaves. Variante calzada: exterior bajo
    # un agua poco profunda que se puede caminar.
    hills = (fbm(rng, 170.0 * K, octaves=3) * 0.5 + 0.5) * float(rng.uniform(*style["hills"]))
    sea = WATER_M - float(rng.uniform(*SEA_DEPTH_M)) + 0.5 * fbm(rng, 60.0 * K, octaves=2)
    if is_causeway:
        high = sea
    else:
        high = elev + wall_h + hills
        # Acantilado: pared mucho mas alta, en estratos.
        high = high + float(rng.uniform(*CLIFF_EXTRA_M)) * cliff
        high = high * (1.0 - 0.7 * cliff) + strata(high, CLIFF_STRATA_M) * 0.7 * cliff
        # Modulo mixto con agua: el lado del agua se inunda.
        if secondary != biome and "water" in (biome, secondary):
            water_w = weight_of("water")
            high = high * (1.0 - water_w) + sea * water_w
    # Junto al borde la meseta tiende a la cresta canonica; las salidas cerradas se tapan.
    high = CREST_M + (high - CREST_M) * edge_fade
    closed = np.zeros_like(XX, dtype=bool)
    for side, (ex, ey) in EXIT_POINT.items():
        if side in exits:
            continue
        along = np.abs(YY) if side in ("N", "S") else np.abs(XX)
        toward = np.hypot(XX - ex, YY - ey)
        closed |= (along < OPEN_HALF_M + BANK_M + 6.0) & (toward < 30.0)
    high = np.where(closed, np.maximum(high, PLUG_M), high)

    terrain = high * (1.0 - corridor) + floor * corridor
    if fork and fork.low:
        # Loma: poca altura y laderas suaves; se puede subir y cruzar por encima.
        terrain = terrain * (1.0 - island) + (floor + fork.low_height) * island
    elif fork and is_causeway:
        # En la calzada la meseta esta bajo el agua: la isla se levanta como roca propia.
        terrain = terrain * (1.0 - island) + (floor + wall_h) * island

    if style["fort"]:
        # Fuerte de arena: foso de agua y murallita alrededor de la plaza central.
        moat, rampart = fort_fields(rooms[0], d_main, hw)
        terrain = terrain * (1.0 - moat) + (WATER_M - 1.5) * moat
        terrain = terrain + 2.2 * rampart

    # Ruta alta: sube por el talud, recorre la meseta y cruza el pasillo por un puente.
    bridges: list[Bridge] = []
    has_upper = False
    if not is_causeway and rng.random() < UPPER_PROB:
        for lane in upper_lane_candidates(rng, exits, wall_h + 2.0):
            d_up, s_up, total = polyline_distance(wx, wy, lane.points)
            lane_half = float(rng.uniform(5.0, 7.0))
            lane_mask = 1.0 - smoothstep(lane_half, lane_half + 5.0, d_up)
            lane_height = lane_profile_height(floor, d_core, lane, s_up, total)
            s_cross0, s_cross1 = lane.arc_range(*lane.crossing)
            found = bridges_for_lane(lane_mask, core, lane_height, terrain, lane_half, s_up, (s_cross0, s_cross1))
            if found is None:
                continue
            weight = lane_mask * (1.0 - core)
            candidate = terrain * (1.0 - weight) + lane_height * weight
            # Misma validacion que la final, sobre el candidato: si el tablero no apoya
            # bien (p. ej. cae sobre la propia bajada de la ruta) se prueba otro trazado.
            if bridge_problem(blur(candidate), found):
                continue
            terrain = candidate
            bridges = found
            has_upper = True
            break

    terrain = blur(terrain)

    tunnel = None
    if wants_tunnel:
        tunnel = place_arch(rng, terrain, d_main, bridges, "tunnel", rooms)
        if tunnel:
            bridges = bridges + [tunnel]
    arch = None
    if not is_causeway and rng.random() < style["arch_prob"]:
        arch = place_arch(rng, terrain, d_main, bridges)
        if arch:
            bridges = bridges + [arch]

    monoliths = place_monoliths(rng, terrain, d_main, hw, rooms, bridges,
                                int(rng.integers(style["monoliths"][0], style["monoliths"][1] + 1)))
    foliage_amount = style["foliage"] if biome == "algae" else STYLES["kelp_forest"]["foliage"]
    foliage = foliage_density(rng, terrain, core, weight_of("algae"), foliage_amount)

    stats = {
        "style": style_name,
        "biome": biome,
        "secondary_biome": secondary,
        "causeway": is_causeway,
        "rooms": len(rooms),
        "sunken": len(sunken_rooms),
        "shortcut": shortcut is not None,
        "upper_route": has_upper,
        "fork": "none" if not fork else ("low" if fork.low else "high"),
        "arch": arch is not None,
        "tunnel": tunnel is not None,
        "cliff": bool(np.any(cliff > 0.5)),
        "wall_h": round(wall_h, 2),
        "base_half_width": round(base_hw, 2),
    }
    return terrain, stats, bridges, flat_areas, monoliths, blend, foliage


def encode_biome_mask(blend, foliage) -> np.ndarray:
    """Mascara de 16 bits: byte alto = peso del bioma secundario, byte bajo = densidad del
    bosque de algas. Mismo formato de PNG que el heightfield (lo lee el mismo importador)."""
    high = np.rint(np.clip(blend, 0.0, 1.0) * 255.0).astype(np.uint16)
    low = np.rint(np.clip(foliage, 0.0, 1.0) * 255.0).astype(np.uint16)
    return (high << 8) | low


def compose_module(seed: int, exits: tuple[str, ...], biome: str, secondary: str):
    rng = np.random.default_rng(seed)
    design, stats, bridges, flat_areas, monoliths, blend, foliage = design_module(rng, exits, biome, secondary)
    heights_m = design * (1.0 - BORDER_WEIGHT) + CANONICAL * BORDER_WEIGHT
    quantized = np.rint(heights_m * UNITS_PER_M) + HEIGHT_ZERO
    heights = np.clip(quantized, 0, 65535).astype(np.uint16)
    return heights, stats, bridges, flat_areas, monoliths, encode_biome_mask(blend, foliage)


def module_biomes(n: int, count: int) -> tuple[str, str]:
    """(bioma, secundario) del modulo n de una topologia. Los ultimos 3 * MIXED_PER_PAIR son
    mixtos (arena-agua, arena-algas, agua-algas); el resto se reparte a partes iguales."""
    pairs = [(a, b) for i, a in enumerate(BIOMES) for b in BIOMES[i + 1:]]
    mixed = min(len(pairs) * MIXED_PER_PAIR, count // 4)
    pure = count - mixed
    if n < pure:
        biome = BIOMES[n * len(BIOMES) // max(pure, 1)]
        return biome, biome
    a, b = pairs[min((n - pure) * len(pairs) // max(mixed, 1), len(pairs) - 1)]
    return a, b

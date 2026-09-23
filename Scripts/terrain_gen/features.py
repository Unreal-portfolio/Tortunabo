"""Rasgos del interior: rutas, plazas, arcos, tuneles, monolitos, acantilados, algas."""

from __future__ import annotations

import math
from dataclasses import dataclass
from pathlib import Path

import numpy as np

from .core import *  # noqa: F401,F403 (paquete interno del generador)
from .styles import STYLES


@dataclass(frozen=True)
class Fork:
    """Isla en mitad de un tramo del pasillo principal: el pasillo se ensancha, se parte en
    dos canales y vuelve a ser uno. along/half_len en metros sobre el eje centro-salida."""
    exit: str
    along: float
    half_len: float
    island_half_w: float
    offset: float
    low: bool
    low_height: float


def pick_fork(rng: np.random.Generator, exits: tuple[str, ...]) -> Fork | None:
    if rng.random() > FORK_PROB:
        return None
    exit_side = exits[int(rng.integers(len(exits)))]
    half_len = float(rng.uniform(16.0, 30.0))
    island_half_w = float(rng.uniform(4.0, 8.0))
    # El tramo cabe entre la plaza central y la franja del borde.
    along = float(rng.uniform(0.42, 0.62)) * HALF_M
    offset = float(rng.uniform(-0.35, 0.35)) * island_half_w
    low = bool(rng.random() < FORK_LOW_ISLAND_PROB)
    return Fork(exit_side, along, half_len, island_half_w, offset, low, float(rng.uniform(1.2, 2.2)))


def fork_fields(rng: np.random.Generator, fork: Fork, wx, wy):
    """(ensanche del pasillo en m, mascara de isla 0..1) medidos en el espacio deformado."""
    dx, dy = EXIT_DIR[fork.exit]
    along = wx * dx + wy * dy
    across = -wx * dy + wy * dx
    taper = 14.0
    bump = 1.0 - smoothstep(fork.half_len, fork.half_len + taper, np.abs(along - fork.along))
    widen = (1.6 * fork.island_half_w + 3.0) * bump
    ell = np.hypot((along - fork.along) / fork.half_len, (across - fork.offset) / fork.island_half_w)
    ell = ell + 0.12 * value_noise(rng, 14.0)
    island = 1.0 - smoothstep(0.78, 1.05, ell)
    return widen, island


def chaikin(points: tuple[Point, ...], iterations: int = 2) -> tuple[Point, ...]:
    """Redondea las esquinas de una polilinea conservando sus extremos."""
    pts = list(points)
    for _ in range(iterations):
        out = [pts[0]]
        for a, b in zip(pts[:-1], pts[1:]):
            out.append((0.75 * a[0] + 0.25 * b[0], 0.75 * a[1] + 0.25 * b[1]))
            out.append((0.25 * a[0] + 0.75 * b[0], 0.25 * a[1] + 0.75 * b[1]))
        out.append(pts[-1])
        pts = out
    return tuple(pts)


def main_lanes(exits: tuple[str, ...]) -> list[Lane]:
    return [Lane(((0.0, 0.0), EXIT_POINT[e]), 0.0) for e in exits]


def pick_shortcut(rng: np.random.Generator, exits: tuple[str, ...], base_half_width: float) -> Lane | None:
    """Atajo a ras de suelo entre dos salidas: cuerda curva de una esquina, o rodeo de la
    plaza. Se abre y se cierra con la misma anchura relativa y el mismo talud que el pasillo
    principal, para que se lea como un brazo del camino y no como una zanja aparte."""
    if len(exits) < 2 or rng.random() > SHORTCUT_PROB:
        return None
    pairs = [(a, b) for i, a in enumerate(exits) for b in exits[i + 1:]]
    perpendicular_pairs = [(a, b) for a, b in pairs if EXIT_DIR[a][0] * EXIT_DIR[b][0] + EXIT_DIR[a][1] * EXIT_DIR[b][1] == 0.0]
    a, b = perpendicular_pairs[int(rng.integers(len(perpendicular_pairs)))] if perpendicular_pairs else pairs[int(rng.integers(len(pairs)))]
    p1 = scaled(EXIT_POINT[a], float(rng.uniform(0.5, 0.72)))
    p2 = scaled(EXIT_POINT[b], float(rng.uniform(0.5, 0.72)))
    half_width = base_half_width * float(rng.uniform(0.55, 0.8))
    if perpendicular_pairs:
        # Curva de Bezier cuadratica con el control hacia la esquina: la cuerda se comba
        # como un camino que ataja siguiendo el terreno, no en linea recta.
        corner = added(EXIT_POINT[a], EXIT_POINT[b])
        mid = scaled(added(p1, p2), 0.5)
        pull = float(rng.uniform(0.1, 0.3))
        control = added(mid, scaled(added(corner, scaled(mid, -1.0)), pull))
    else:
        side = 1.0 if rng.random() < 0.5 else -1.0
        normal = perpendicular(EXIT_DIR[a])
        control = added(scaled(added(p1, p2), 0.5), scaled(normal, side * float(rng.uniform(70.0, 100.0)) * K * 2.0))
    samples = [((1 - t) ** 2 * p1[0] + 2 * (1 - t) * t * control[0] + t * t * p2[0],
                (1 - t) ** 2 * p1[1] + 2 * (1 - t) * t * control[1] + t * t * p2[1])
               for t in np.linspace(0.0, 1.0, 12)]
    return Lane(tuple(samples), half_width)


def smooth_lane(lane: Lane) -> Lane:
    """Redondea las esquinas de una ruta alta sin tocar su tramo de cruce: se suaviza por
    separado lo que va antes y despues del cruce y se recalculan sus indices."""
    first, last = lane.crossing
    before = chaikin(lane.points[:first + 1])
    after = chaikin(lane.points[last:])
    points = before + lane.points[first + 1:last] + after
    crossing = (len(before) - 1, len(before) - 1 + (last - first))
    return Lane(points, lane.half_width, lane.rise_m, crossing)


def upper_lane_candidates(rng: np.random.Generator, exits: tuple[str, ...], rise_m: float) -> list[Lane]:
    """Rutas altas posibles: nacen en la salida A, cruzan el pasillo de X y bajan tras el cruce."""
    candidates = []
    order = list(exits)
    rng.shuffle(order)
    # Varios puntos de cruce por pareja: con el modulo de 200 m las plazas ocupan buena parte
    # del pasillo y el hueco solo es salvable por un arco donde el pasillo va estrecho.
    crossing_fractions = [0.3, 0.42, 0.55, 0.68, 0.8]
    rng.shuffle(crossing_fractions)
    for crossed in order:
        for branch in order:
            if branch == crossed:
                continue
            for fraction in crossing_fractions:
                dir_a, dir_x = EXIT_DIR[branch], EXIT_DIR[crossed]
                p1 = scaled(EXIT_POINT[branch], float(rng.uniform(0.55, 0.75)))
                crossing = scaled(EXIT_POINT[crossed], fraction)
                p2 = scaled(EXIT_POINT[crossed], min(fraction + 0.15, 0.9))
                # Sale del pasillo y vuelve a entrar siempre en perpendicular: asi la rampa
                # arranca en el talud y no hay tramos oblicuos dentro del pasillo.
                if dir_a[0] * dir_x[0] + dir_a[1] * dir_x[1] < 0.0:
                    # Salidas opuestas: la ruta corre paralela al pasillo y lo cruza en perpendicular.
                    side = 1.0 if rng.random() < 0.5 else -1.0
                    normal = scaled(perpendicular(dir_x), side * UPPER_OFFSET_M)
                    back = scaled(normal, -1.0)
                    candidates.append(Lane((p1, added(p1, normal), added(crossing, normal),
                                            added(crossing, back), added(p2, back), p2), 0.0, rise_m, (2, 3)))
                else:
                    # Salidas perpendiculares: sube hacia X, cruza su pasillo y baja al otro lado.
                    w1 = added(p1, scaled(dir_x, UPPER_OFFSET_M))
                    far = added(crossing, scaled(dir_a, -UPPER_OFFSET_M))
                    far2 = added(p2, scaled(dir_a, -UPPER_OFFSET_M))
                    candidates.append(Lane((p1, w1, far, far2, p2), 0.0, rise_m, (1, 2)))
    return [smooth_lane(lane) for lane in candidates]


def pick_rooms(rng: np.random.Generator, exits: tuple[str, ...], sunken_prob: float = 0.0,
               free_exit: str | None = None) -> list[Room]:
    """Plaza central y, por salida, otra a mitad de camino. Solo las laterales se hunden: la
    central es el cruce de todas las rutas y debe quedar a la cota del pasillo. free_exit
    queda sin plaza lateral: su tramo se reserva para un tunel."""
    rooms = [Room(0.0, 0.0, float(rng.uniform(45.0, 70.0)) * K)]
    for e in exits:
        if rng.random() < 0.7 and e != free_exit:
            ex, ey = EXIT_POINT[e]
            t = float(rng.uniform(0.42, 0.68))
            sunk = float(rng.uniform(*SUNKEN_DEPTH_M)) if rng.random() < sunken_prob else 0.0
            rooms.append(Room(ex * t, ey * t, float(rng.uniform(32.0, 58.0)) * K, sunk))
    return rooms


def room_mask(rooms: list[Room], feather: float = 6.0):
    mask = np.zeros_like(XX)
    for room in rooms:
        d = np.hypot(XX - room.x, YY - room.y)
        mask = np.maximum(mask, 1.0 - smoothstep(room.radius - feather, room.radius + feather, d))
    return mask


def pick_style(rng: np.random.Generator, biome: str) -> tuple[str, dict]:
    names = [n for n, s in STYLES.items() if s["biome"] == biome]
    weights = np.array([STYLES[n]["weight"] for n in names], dtype=float)
    name = names[int(rng.choice(len(names), p=weights / weights.sum()))]
    return name, STYLES[name]


def lane_frame(exit_side: str, wx, wy):
    """(a lo largo, a traves) del eje centro-salida, medidos en el espacio deformado."""
    dx, dy = EXIT_DIR[exit_side]
    return wx * dx + wy * dy, -wx * dy + wy * dx


def cliff_field(rng: np.random.Generator, exits: tuple[str, ...], wx, wy, hw):
    """Mascara 0..1 de un acantilado: un lado del pasillo de una salida, en un tramo de
    40-70 m, entre el talud y 30 m mas alla. None si el modulo no lleva acantilado."""
    exit_side = exits[int(rng.integers(len(exits)))]
    along, across = lane_frame(exit_side, wx, wy)
    side = 1.0 if rng.random() < 0.5 else -1.0
    center = float(rng.uniform(0.45, 0.7)) * HALF_M
    half_len = float(rng.uniform(20.0, 35.0))
    window = 1.0 - smoothstep(half_len, half_len + 12.0, np.abs(along - center))
    lateral = smoothstep(0.0, 4.0, side * across - hw * 0.6) * (1.0 - smoothstep(30.0, 42.0, side * across - hw))
    return window * lateral


def strata(heights, step_m: float):
    """Terrazas horizontales: la roca se lee por capas en vez de como una rampa lisa."""
    q = heights / step_m
    frac = q - np.floor(q)
    return (np.floor(q) + smoothstep(0.75, 1.0, frac)) * step_m


def biome_blend_field(rng: np.random.Generator):
    """Peso 0..1 del bioma secundario de un modulo mixto: frente difuso que cruza el modulo
    en una direccion al azar. Se apaga en la franja del borde (el borde es comun a todos)."""
    angle = float(rng.uniform(0.0, 2.0 * np.pi))
    ramp = (XX * math.cos(angle) + YY * math.sin(angle)) / HALF_M + 0.35 * fbm(rng, 60.0, octaves=2)
    return smoothstep(-0.3, 0.3, ramp)


def fort_fields(central: Room, d_main, hw):
    """(foso, murallita) alrededor de la plaza central de un fuerte. El pasillo atraviesa
    ambos: entra por un paso a la cota del suelo, como la puerta de un castillo de arena."""
    d = np.hypot(XX - central.x, YY - central.y)
    gate = smoothstep(hw - 2.0, hw + 3.0, d_main)
    moat = smoothstep(central.radius + 1.0, central.radius + 3.0, d) * (1.0 - smoothstep(central.radius + 8.0, central.radius + 11.0, d))
    wall = smoothstep(central.radius - 3.5, central.radius - 2.0, d) * (1.0 - smoothstep(central.radius - 0.5, central.radius + 0.5, d))
    return moat * gate, wall * gate


def foliage_density(rng: np.random.Generator, terrain, core, algae_weight, amount: float):
    """Densidad 0..1 del bosque de algas por vertice (la usa el tile para sembrar
    instancias). Matas agrupadas por ruido; nada en el nucleo del pasillo, bajo el agua ni
    en pendientes que no se podrian pisar."""
    if amount <= 0.0 or not np.any(algae_weight > 0.0):
        return np.zeros_like(XX)
    gx, gy = np.gradient(terrain, STEP_M)
    slope = np.degrees(np.arctan(np.hypot(gx, gy)))
    clumps = smoothstep(0.35, 0.7, fbm(rng, 30.0, octaves=2) * 0.5 + 0.5)
    open_ground = (1.0 - smoothstep(0.02, 0.3, core)) * (1.0 - smoothstep(28.0, 40.0, slope))
    dry = smoothstep(WATER_M + 0.2, WATER_M + 1.0, terrain)
    edge = smoothstep(FOLIAGE_EDGE_CLEAR_M, FOLIAGE_EDGE_CLEAR_M + 6.0, DIST_TO_EDGE)
    return np.clip(amount * (0.25 + 0.75 * clumps) * open_ground * dry * edge * algae_weight, 0.0, 1.0)


def pits(rng: np.random.Generator, rooms: list[Room], d_corr, hw, count_range: tuple[int, int] = (0, 2)):
    """Pozos: en las plazas (fuera del carril central) y sueltos en la meseta."""
    depression = np.zeros_like(XX)
    for room in rooms:
        if rng.random() < 0.45:
            angle = float(rng.uniform(0.0, 2.0 * np.pi))
            offset = room.radius * 0.55
            cx, cy = room.x + offset * math.cos(angle), room.y + offset * math.sin(angle)
            radius = float(rng.uniform(5.0, 8.0))
            depth = float(rng.uniform(3.5, 6.0))
            d = np.hypot(XX - cx, YY - cy)
            depression += depth * (1.0 - smoothstep(radius * 0.45, radius, d))
    for _ in range(int(rng.integers(count_range[0], count_range[1] + 1))):
        cx, cy = rng.uniform(-0.7 * HALF_M, 0.7 * HALF_M, 2)
        radius = float(rng.uniform(8.0, 16.0))
        depth = float(rng.uniform(4.0, 9.0))
        d = np.hypot(XX - cx, YY - cy)
        bowl = depth * (1.0 - smoothstep(radius * 0.45, radius, d))
        # Solo en la meseta: nunca muerde el pasillo ni su talud.
        depression += bowl * smoothstep(hw + 14.0, hw + 26.0, d_corr)
    return depression



def bridges_for_lane(lane_mask, core, lane_height, terrain, half_width: float, s, crossing_range) -> list[Bridge] | None:
    """Un tablero por hueco que el pasillo abre en la ruta alta. None si no es viable.

    Donde la ruta nace y muere esta a ras del pasillo: esos tramos tambien caen dentro
    del pasillo, pero no son huecos (la ruta esta a la cota del suelo) y no llevan puente.
    """
    # Umbral bajo del nucleo: los taludes finos entre dos pasillos que se tocan (atajo que
    # nace del principal) no parten el hueco en dos: un solo tablero salva la horquilla.
    gap = dilate((lane_mask > 0.5) & (core > 0.15), radius=2)
    components = [c for c in connected_components(gap) if len(c) >= 3]
    components = [c for c in components if float((lane_height - terrain)[c[:, 0], c[:, 1]].mean()) >= 4.0]
    components = [c for c in components if crossing_range[0] <= float(s[c[:, 0], c[:, 1]].mean()) <= crossing_range[1]]
    if not (1 <= len(components) <= 3):
        return None
    bridges = []
    for cells in components:
        xs = cells[:, 0] * STEP_M - HALF_M
        ys = cells[:, 1] * STEP_M - HALF_M
        cx, cy = float(xs.mean()), float(ys.mean())
        cov = np.cov(np.stack([xs - cx, ys - cy]))
        eigen_values, eigen_vectors = np.linalg.eigh(cov)
        axis = eigen_vectors[:, int(np.argmax(eigen_values))]
        along = (xs - cx) * axis[0] + (ys - cy) * axis[1]
        length = float(along.max() - along.min()) + STEP_M + 2.0 * BRIDGE_OVERLAP_M
        if not BRIDGE_MIN_M <= length <= BRIDGE_MAX_M:
            return None
        # Centro en mitad del recorrido a lo largo del eje, no en el centroide: un hueco en
        # L (pasillo oblicuo) desplazaria el centroide fuera de la ruta.
        mid = float(along.max() + along.min()) * 0.5
        cx, cy = cx + axis[0] * mid, cy + axis[1] * mid
        deck = float(lane_height[cells[:, 0], cells[:, 1]].mean())
        bridges.append(Bridge(round(cx, 2), round(cy, 2), round(math.degrees(math.atan2(axis[1], axis[0])), 1),
                              round(length, 2), round(2.0 * half_width + 2.0, 2), round(deck, 2)))
    return bridges


# ── Diseno del interior ───────────────────────────────────────────────────────────
def lane_profile_height(floor, d_core, lane: Lane, s, total: float):
    """Cota de una ruta alta: a ras del pasillo mientras esta dentro de el, rampa desde el
    talud (en funcion de la distancia al nucleo del pasillo) y +rise en la meseta. En el
    tramo de cruce se mantiene alta pase lo que pase debajo: ahi va el puente."""
    i1, j1 = grid_index(lane.points[0])
    i2, j2 = grid_index(lane.points[-1])
    f1, f2 = float(floor[i1, j1]), float(floor[i2, j2])
    s_cross0, s_cross1 = lane.arc_range(*lane.crossing)
    in_crossing = (s >= s_cross0) & (s <= s_cross1)
    ramp = np.where(in_crossing, 1.0, smoothstep(0.0, UPPER_RAMP_M, d_core))
    return f1 + (f2 - f1) * (s / max(total, 1.0)) + lane.rise_m * ramp


def place_arch(rng: np.random.Generator, terrain, d_main, bridges: list[Bridge], kind: str = "arch",
               rooms: list[Room] | None = None, region=None) -> Bridge | None:
    """Arco natural de pared a pared sobre el pasillo principal. Se busca un punto del eje
    lejos del borde y de otros puentes, se estima la direccion del pasillo por PCA de su eje
    cercano y se camina en perpendicular hasta coronar cada pared.

    kind "tunnel": el mismo arco, pero largo a lo largo del pasillo (TUNNEL_LENGTH_M). Exige
    un tramo recto (el eje no se aparta del tablero) sin plazas, y paredes que lleguen al
    techo en toda su longitud, para que no quede un extremo volando."""
    is_tunnel = kind == "tunnel"
    # Paso libre bajo la panza: el tile hunde el grosor bajo el techo (deck - 0,5 m).
    min_gap = ARCH_CLEARANCE_M + (TUNNEL_THICKNESS_M if is_tunnel else BRIDGE_THICKNESS_M) + 0.5
    along_len = float(rng.uniform(*TUNNEL_LENGTH_M)) if is_tunnel else float(rng.uniform(5.0, 10.0))
    margin = 30.0 + (along_len * 0.5 if is_tunnel else 0.0)
    # region: mascara opcional que limita donde se busca (p. ej. el arco sobre una puerta).
    axis_cells = np.argwhere((d_main < 1.5) & (DIST_TO_EDGE > margin) & (True if region is None else region > 0.5))
    if len(axis_cells) == 0:
        return None
    for _ in range(30 if is_tunnel else 10):
        i, j = axis_cells[int(rng.integers(len(axis_cells)))]
        px, py = float(XX[i, j]), float(YY[i, j])
        if any(math.hypot(px - b.x, py - b.y) < 30.0 + along_len * 0.5 for b in bridges):
            continue
        if rooms and any(math.hypot(px - r.x, py - r.y) < r.radius + along_len * 0.5 + 6.0 for r in rooms):
            continue
        near = (np.hypot(XX - px, YY - py) < max(12.0, along_len * 0.5)) & (d_main < 1.5)
        xs, ys = XX[near] - px, YY[near] - py
        if len(xs) < 4:
            continue
        eigen_values, eigen_vectors = np.linalg.eigh(np.cov(np.stack([xs, ys])))
        tangent = eigen_vectors[:, int(np.argmax(eigen_values))]
        across = (-float(tangent[1]), float(tangent[0]))
        if is_tunnel and any(float(d_main[grid_index((px + tangent[0] * t, py + tangent[1] * t))]) > 7.0
                             for t in np.linspace(-along_len * 0.5, along_len * 0.5, 7)):
            continue   # el pasillo se curva dentro del tunel

        ground = float(terrain[i, j])
        reach = []
        for sign in (1.0, -1.0):
            best_t, best_h = None, -np.inf
            for t in np.arange(2.0, 60.0, 1.0):
                h = float(terrain[grid_index((px + sign * across[0] * t, py + sign * across[1] * t))])
                if h > best_h + 0.25:
                    best_t, best_h = t, h
                elif best_h >= ground + min_gap:
                    break   # coronada la pared: deja de subir
            reach.append((best_t, best_h))
        (t1, h1), (t2, h2) = reach
        deck = min(h1, h2)
        if t1 is None or t2 is None or deck - ground < min_gap:
            continue
        length = t1 + t2 + 2.0 * BRIDGE_OVERLAP_M
        cx = px + across[0] * (t1 - t2) * 0.5
        cy = py + across[1] * (t1 - t2) * 0.5
        yaw = math.degrees(math.atan2(across[1], across[0]))
        arch = Bridge(round(cx, 2), round(cy, 2), round(yaw, 1), round(length, 2),
                      round(along_len, 2), round(deck - 0.5, 2), kind)
        if not BRIDGE_MIN_M <= length <= BRIDGE_MAX_M or bridge_problem(terrain, [arch]):
            continue
        if is_tunnel and any(bridge_problem(terrain, [Bridge(round(cx + tangent[0] * t, 2), round(cy + tangent[1] * t, 2),
                                                             arch.yaw_deg, arch.length_m, 2.0, arch.deck_m)])
                             for t in (-along_len * 0.4, along_len * 0.4)):
            continue
        return arch
    return None


def place_monoliths(rng: np.random.Generator, terrain, d_main, hw, rooms: list[Room], bridges: list[Bridge],
                    count: int) -> list[Monolith]:
    """Pilares de roca como hitos: en el borde de las plazas (sin tapar el paso) o en la
    meseta. El pie se entierra 1 m bajo el punto mas bajo de su huella."""
    monoliths: list[Monolith] = []
    for _ in range(count * 6):
        if len(monoliths) >= count:
            break
        radius = float(rng.uniform(*MONOLITH_RADIUS_M))
        if rng.random() < 0.6 and rooms:
            room = rooms[int(rng.integers(len(rooms)))]
            angle = float(rng.uniform(0.0, 2.0 * np.pi))
            dist = room.radius * float(rng.uniform(0.55, 0.85))
            x, y = room.x + dist * math.cos(angle), room.y + dist * math.sin(angle)
            # En la plaza: fuera del carril central (60 % del semiancho local del pasillo).
            lane_clear = 0.6 * float(hw[grid_index((x, y))]) + radius + 3.0
        else:
            x, y = (float(v) for v in rng.uniform(-0.75 * HALF_M, 0.75 * HALF_M, 2))
            # En la meseta: mas alla del talud.
            lane_clear = float(hw[grid_index((x, y))]) + radius + 12.0
        if float(d_main[grid_index((x, y))]) < max(lane_clear, MONOLITH_LANE_CLEARANCE_M + radius):
            continue
        if DIST_TO_EDGE[grid_index((x, y))] < 16.0 + radius:
            continue
        if any(math.hypot(x - b.x, y - b.y) < b.length_m * 0.5 + radius + 6.0 for b in bridges):
            continue
        if any(math.hypot(x - m.x, y - m.y) < m.radius_m + radius + 10.0 for m in monoliths):
            continue
        footprint = np.hypot(XX - x, YY - y) < radius + STEP_M
        base = float(terrain[footprint].min()) - 1.0
        monoliths.append(Monolith(round(x, 2), round(y, 2), round(base, 2), round(radius, 2),
                                  round(float(rng.uniform(*MONOLITH_HEIGHT_M)), 2),
                                  round(float(rng.uniform(0.0, 360.0)), 1), round(float(rng.uniform(0.0, 7.0)), 1)))
    return monoliths


# ── Iteracion 2026-09-23 (tras el playtest de F3/F4) ─────────────────────────────
def dunes(rng: np.random.Generator, amplitude: float, wavelength: float):
    """Campo de dunas transversales: crestas perpendiculares a un viento sorteado, con
    barlovento largo y suave (75 % del periodo) y cara de avalancha corta. Las crestas
    serpentean y cambian de altura para no leerse como un patron."""
    angle = float(rng.uniform(0.0, 2.0 * np.pi))
    along = XX * math.cos(angle) + YY * math.sin(angle)
    across = -XX * math.sin(angle) + YY * math.cos(angle)
    sinuous = 0.35 * wavelength * fbm(rng, 2.5 * wavelength, octaves=2)
    phase = (along + sinuous + 0.15 * across) / wavelength
    frac = phase - np.floor(phase)
    profile = np.where(frac < 0.75, frac / 0.75, (1.0 - frac) / 0.25)
    profile = smoothstep(0.0, 1.0, profile) ** 1.2
    height = 0.45 + 0.55 * (fbm(rng, 3.0 * wavelength, octaves=2) * 0.5 + 0.5)
    return amplitude * profile * height


def sharp_strata(heights, step_m: float):
    """Terrazas de canto vivo: el acantilado se lee como bloques cuadrados, no como un pico."""
    q = heights / step_m
    frac = q - np.floor(q)
    return (np.floor(q) + smoothstep(0.88, 1.0, frac)) * step_m


def gate_mask(exits: tuple[str, ...]):
    """0..1 en el tramo del pasillo justo detras de cada boca (entre 14 y 40 m del borde)."""
    mask = np.zeros_like(XX)
    for side in exits:
        ex, ey = EXIT_POINT[side]
        near_exit = 1.0 - smoothstep(35.0, 50.0, np.hypot(XX - ex, YY - ey))
        band = smoothstep(12.0, 18.0, DIST_TO_EDGE) * (1.0 - smoothstep(34.0, 42.0, DIST_TO_EDGE))
        mask = np.maximum(mask, near_exit * band)
    return mask


def tunnel_ramp(rng: np.random.Generator, terrain, tunnel: Bridge, rooms: list[Room]):
    """Rampa que sube desde el pasillo, pegada a una pared, hasta la boca del tunel a la
    cota de su techo: el tunel se atraviesa por dentro y tambien se cruza por arriba.
    Devuelve el terreno modificado o None si no cabe (borde, plazas, pendiente)."""
    yaw = math.radians(tunnel.yaw_deg)
    across = (math.cos(yaw), math.sin(yaw))          # eje del arco: de pared a pared
    tangent = (-across[1], across[0])                # a lo largo del pasillo
    for _ in range(8):
        side = 1.0 if rng.random() < 0.5 else -1.0
        end = 1.0 if rng.random() < 0.5 else -1.0
        top = added((tunnel.x, tunnel.y), added(scaled(tangent, end * (tunnel.width_m * 0.5 + 3.0)),
                                               scaled(across, side * (tunnel.length_m * 0.5 - 3.0))))
        rise = tunnel.deck_m - float(terrain[grid_index((tunnel.x, tunnel.y))])
        length = rise / math.tan(math.radians(27.0))
        start = added(top, added(scaled(tangent, end * length), scaled(across, -side * tunnel.length_m * 0.3)))
        if DIST_TO_EDGE[grid_index(start)] < BAND_M + 6.0 or DIST_TO_EDGE[grid_index(top)] < BAND_M + 6.0:
            continue
        if any(math.hypot(start[0] - r.x, start[1] - r.y) < r.radius + 4.0 for r in rooms):
            continue
        d, s, total = polyline_distance(XX, YY, (start, top))
        base = float(terrain[grid_index(start)])
        ramp = base + (tunnel.deck_m - base) * np.clip(s / max(total, 1.0), 0.0, 1.0)
        weight = 1.0 - smoothstep(2.5, 4.5, d)
        candidate = terrain * (1.0 - weight) + ramp * weight
        if bridge_problem(candidate, [tunnel]) is None:
            return candidate
    return None


def maze_lanes(rng: np.random.Generator, exits: tuple[str, ...], cells: int = 5, loop_prob: float = 0.2) -> list[Lane]:
    """Laberinto sobre una rejilla gruesa de cells x cells: arbol de expansion aleatorio
    (DFS) mas algunos lazos, y un tramo de cada boca a la celda central de su lado."""
    step = SIZE_M / cells
    center = cells // 2

    def point(i: int, j: int) -> Point:
        return (-HALF_M + (i + 0.5) * step, -HALF_M + (j + 0.5) * step)

    seen = {(center, center)}
    stack = [(center, center)]
    edges: set[tuple[tuple[int, int], tuple[int, int]]] = set()
    while stack:
        i, j = stack[-1]
        options = [(i + di, j + dj) for di, dj in ((1, 0), (-1, 0), (0, 1), (0, -1))
                   if 0 <= i + di < cells and 0 <= j + dj < cells and (i + di, j + dj) not in seen]
        if not options:
            stack.pop()
            continue
        nxt = options[int(rng.integers(len(options)))]
        edges.add(((i, j), nxt))
        seen.add(nxt)
        stack.append(nxt)
    for i in range(cells):
        for j in range(cells):
            for nxt in ((i + 1, j), (i, j + 1)):
                if nxt[0] < cells and nxt[1] < cells and ((i, j), nxt) not in edges \
                        and (nxt, (i, j)) not in edges and rng.random() < loop_prob:
                    edges.add(((i, j), nxt))

    lanes = [Lane((point(*a), point(*b)), 0.0) for a, b in edges]
    mouth_cell = {"N": (cells - 1, center), "S": (0, center), "E": (center, cells - 1), "W": (center, 0)}
    lanes += [Lane((point(*mouth_cell[e]), EXIT_POINT[e]), 0.0) for e in exits]
    return lanes


def island_chain(rng: np.random.Generator, start: Point, end: Point, radius: tuple[float, float],
                 gap: tuple[float, float], bend_m: float = 35.0) -> list[Room]:
    """Islas de start a end a lo largo de una curva (Bezier con el control desplazado hasta
    bend_m a un lado), separadas por canales de agua de gap metros."""
    total = math.dist(start, end)
    if total < 1e-6:
        return []
    direction = ((end[0] - start[0]) / total, (end[1] - start[1]) / total)
    control = added(scaled(added(start, end), 0.5), scaled(perpendicular(direction), float(rng.uniform(-bend_m, bend_m))))

    def at(t: float) -> Point:
        return ((1 - t) ** 2 * start[0] + 2 * (1 - t) * t * control[0] + t * t * end[0],
                (1 - t) ** 2 * start[1] + 2 * (1 - t) * t * control[1] + t * t * end[1])

    islands = []
    pos = 0.0
    while pos < total:
        r = float(rng.uniform(*radius))
        x, y = at(min((pos + r) / total, 1.0))
        islands.append(Room(x, y, r))
        pos += 2.0 * r + float(rng.uniform(*gap))
    return islands

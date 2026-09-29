"""Islas del catalogo que faltaban, Todos contra Todos (Docs/Catalogo-Mapas-2026-09-29.md §2 y §4.3):

  - I02 Galapagos: 5 islas con puentes naturales (archipielago), Isabela con dos conos gemelos y un «caparazon»
    (cupula baja de roca) en cada isla;
  - I03-T Tortuga Magna: caparazon de 13 escamas a +3, +6 y +9 m con rampas, cuatro aletas como rampas desde la
    playa, cola y cabeza con la boca en tunel;
  - I05 Santorini: anillo de caldera con el borde interior en acantilado, dos puentes naturales que cruzan la
    caldera, Thirasia separada por dos canales (uno con puente) y 8 cupulas.

Cada factoria devuelve (ShapeMap, Extras) para kit_writer.write_kit_map; los ids se registran en MAPS (lote C2).
"""

from __future__ import annotations

import math
from dataclasses import asdict, dataclass, replace

import numpy as np

from .archipelago import ArchipelagoSpec, Coast, IslandSpec, build_height, plan_bridges, plateau_point
from .bridges import NaturalBridge
from .canvas import Canvas, plateau, sd_circle, sd_ellipse, smoothstep
from .kit import Axis, KitModel, Tunnel, above, ramp, sea_floor
from .kit_writer import Clearance, Extras
from .model import ShapeMap, ShapeModel

TCT_NESTS = 8


def _pt(r: float, deg: float, c=(0.0, 0.0)) -> tuple[float, float]:
    return c[0] + r * math.cos(math.radians(deg)), c[1] + r * math.sin(math.radians(deg))


def dome(e, n, ce: float, cn: float, radius: float, rise: float) -> np.ndarray:
    """Cupula de `rise` m en el centro y 0 en el borde, sin escalon (tangente horizontal en el borde)."""
    return rise * smoothstep(radius, 0.0, np.hypot(e - ce, n - cn))


# ── I02 Galapagos ────────────────────────────────────────────────────────────────
@dataclass(frozen=True)
class Cone:
    island: str
    center: tuple[float, float]
    base_r: float
    top_r: float
    peak_m: float                     # cima sobre el agua


def galapagos_spec(seed: int = 1002) -> tuple[ArchipelagoSpec, tuple[Cone, ...]]:
    islands = (
        IslandSpec("isabela", (-52.0, 8.0), (30.0, 62.0), 3.0, angle_deg=-10.0),
        IslandSpec("santa_cruz", (26.0, 30.0), (24.0, 22.0), 3.5),
        IslandSpec("san_cristobal", (74.0, -2.0), (19.0, 17.0), 3.0, angle_deg=30.0),
        IslandSpec("floreana", (4.0, -52.0), (14.0, 13.0), 2.5),
        IslandSpec("espanola", (58.0, -58.0), (15.0, 11.0), 2.5, angle_deg=20.0),
    )
    cones = (Cone("isabela", (-47.0, 40.0), 29.0, 4.0, 11.0), Cone("isabela", (-58.0, -26.0), 29.0, 4.0, 11.0))
    spec = ArchipelagoSpec(
        "I02_galapagos", seed, 2,
        "Galápagos (Todos contra Todos): 5 islas unidas por 5 puentes naturales; Isabela con dos volcanes gemelos "
        "de +11 m y cada isla con un caparazón de roca (cúpula de +2,5 m) que guarda el mejor botín; cangrejos rojos "
        "en Floreana y Española.",
        islands, None, (), bridge_count=6, bridge_max_m=45.0, start_island="santa_cruz", extra={"catalogo": "I02"})
    return spec, cones


def build_galapagos(seed: int | None = None) -> tuple[ShapeMap, Extras]:
    spec, cones = galapagos_spec(seed or 1002)
    canvas = Canvas(spec.grid)
    coast = Coast(spec)
    e, n = canvas.design_grid()
    height = build_height(spec, canvas, coast)
    shells = {}
    for island in spec.islands:
        land = smoothstep(0.0, spec.shore_m, coast.sd(island, e, n))
        for cone in (c for c in cones if c.island == island.name):
            r = np.hypot(e - cone.center[0], n - cone.center[1])
            t = np.clip((r - cone.top_r) / (cone.base_r - cone.top_r), 0.0, 1.0)
            profile = above(cone.peak_m) - (cone.peak_m - island.height_m) * t
            height = height + np.clip(profile - height, 0.0, None) * land
        centre = plateau_point(island, coast, spec, tuple((c.center[0], c.center[1], c.base_r) for c in cones))
        shells[island.name] = centre
        height = height + dome(e, n, *centre, 11.0, 2.5) * land
    probe = ShapeModel(canvas, height)

    def height_at(pe: float, pn: float) -> float:
        X, Y = canvas.to_world(pe, pn)
        return float(probe.ground_height(np.array([X]), np.array([Y]))[0])

    links = plan_bridges(spec, coast, height_at)
    model = KitModel(canvas, height, tuple(b for _, _, b in links))
    required = {f"caparazon_{k}": p for k, p in shells.items()}
    required.update({f"cima_{k}": c.center for k, c in enumerate(cones)})
    corridors = [b.inner_points(1.5) for _, _, b in links]
    longest = sorted(links, key=lambda x: -x[2].length)[:3]
    markers = {"puente_colgante": [tuple(0.5 * (np.array(b.a) + np.array(b.b))) for _, _, b in longest],
               "cangrejo_rojo": [shells["floreana"], shells["espanola"]],
               "catapulta": [shells["isabela"], shells["santa_cruz"]],
               "trampolin": [shells["san_cristobal"], shells["floreana"], shells["espanola"]]}
    params = {"generator": "galapagos", **asdict(spec), "cones": [asdict(c) for c in cones]}
    shape = ShapeMap(spec.name, spec.seed, spec.mode, spec.description, canvas, model, shells["santa_cruz"], None,
                     required, corridors, corridor_width_m=3.0, corridor_slope_deg=20.0, params=params)
    return shape, Extras(nests=TCT_NESTS, markers=markers, notes="3 puentes marcados para colgantes")


# ── I03-T Tortuga Magna ──────────────────────────────────────────────────────────
@dataclass(frozen=True)
class TurtleSpec:
    """Caparazon eliptico de semiejes shell_e x shell_n (cabeza al Norte) sobre una playa a beach_h_m; escamas:
    central (hexagono de lado scale_side_m) a h_centre, anillo interior de 6 a h_inner y anillo exterior en 6
    sectores a h_outer, separadas por surcos secos de groove_m; boca en tunel a traves de la cabeza."""
    name: str
    seed: int
    grid: int = 2
    shell_e: float = 44.0
    shell_n: float = 56.0
    scale_side_m: float = 11.0
    groove_m: float = 1.5
    h_centre: float = 9.0
    h_inner: float = 6.0
    h_outer: float = 3.0
    beach_h_m: float = 1.5
    ramp_w_m: float = 5.0
    ramp_slope_deg: float = 15.0
    head_r_m: float = 20.0
    head_h_m: float = 11.5
    mouth_w_m: float = 14.0
    description: str = ""
    mode: str = "tct"


def turtle_i03(seed: int = 1103) -> TurtleSpec:
    return TurtleSpec("I03T_tortuga_magna", seed, description=(
        "Tortuga Magna (Todos contra Todos): isla con forma de tortuga gigante; caparazón de 110 × 88 m con 13 "
        "escamas a +9 (central), +6 (6 interiores) y +3 m (6 exteriores) unidas por rampas a 15°, cuatro aletas que "
        "suben desde la playa y la boca de la cabeza como túnel de 14 m."))


def _hex_sd(e, n, ce, cn, side):
    de, dn = np.abs(e - ce), np.abs(n - cn)
    ap = side * math.sqrt(3.0) / 2.0
    return np.minimum(ap - dn, ap - (de * math.sqrt(3.0) / 2.0 + dn / 2.0))


def build_turtle(seed: int | None = None, spec: TurtleSpec | None = None) -> tuple[ShapeMap, Extras]:
    spec = spec or turtle_i03()
    if seed is not None:
        spec = replace(spec, seed=seed)
    canvas = Canvas(spec.grid)
    e, n = canvas.design_grid()
    shell = sd_ellipse(e, n, 0.0, 0.0, spec.shell_e, spec.shell_n)
    head_c = (0.0, spec.shell_n + spec.head_r_m * 0.55)
    body = np.maximum(sd_ellipse(e, n, 0.0, 0.0, spec.shell_e + 8.0, spec.shell_n + 6.0),
                      sd_circle(e, n, head_c[0], head_c[1] + 4.0, spec.head_r_m + 9.0))
    flippers = []
    for sx, sy, ang in ((1, 1, 35.0), (-1, 1, -35.0), (1, -1, -35.0), (-1, -1, 35.0)):
        c = (sx * (spec.shell_e + 10.0), sy * spec.shell_n * 0.45)
        flippers.append((c, ang))
        body = np.maximum(body, sd_ellipse(e, n, c[0], c[1], 22.0, 8.0, ang if sx * sy > 0 else -ang))
    body = np.maximum(body, sd_ellipse(e, n, 0.0, -spec.shell_n - 8.0, 7.0, 12.0))                # cola
    height = np.maximum(sea_floor(e, n, body), plateau(body, spec.beach_h_m, 2.0))
    # Escamas.
    ap = spec.scale_side_m * math.sqrt(3.0) / 2.0
    pitch = 2.0 * ap + spec.groove_m
    inner_c = [_pt(pitch, 90.0 + 60.0 * k) for k in range(6)]
    centre_sd = _hex_sd(e, n, 0.0, 0.0, spec.scale_side_m)
    inner_sd = np.max(np.stack([_hex_sd(e, n, ce, cn, spec.scale_side_m) for ce, cn in inner_c]), axis=0)
    theta = np.degrees(np.arctan2(n, e))
    phase = np.mod(theta - 60.0, 60.0)
    sector_groove = np.minimum(phase, 60.0 - phase) * np.radians(1.0) * np.hypot(e, n)       # m al borde de sector
    g = spec.groove_m / 2.0
    level = np.where(centre_sd > -g, above(spec.h_centre), np.where(inner_sd > -g, above(spec.h_inner), above(spec.h_outer)))
    groove = (np.abs(centre_sd + 0.0) < g) | ((np.abs(inner_sd) < g) & (centre_sd < -g)) |              ((sector_groove < g) & (inner_sd < -g) & (centre_sd < -g)) | (np.abs(shell - 1.5) < g)
    shell_h = level - 0.3 * groove                      # surco seco de 0,3 m: se ve y se anda
    in_shell = shell > 0.0
    height = np.where(in_shell, np.maximum(height, shell_h), height)
    # Rampas: exterior -> interior (3) e interior -> central (2), a ramp_slope_deg.
    tan = math.tan(math.radians(spec.ramp_slope_deg))
    corridors = []
    ramps = [(inner_c[k], spec.h_outer, spec.h_inner, 1.0) for k in (1, 3, 5)] + \
            [(inner_c[k], spec.h_inner, spec.h_centre, 0.0) for k in (0, 3)]
    for (ce, cn), h0, h1, outward in ramps:
        u = np.array([ce, cn]) / math.hypot(ce, cn)
        length = (h1 - h0) / tan
        if outward:                                    # sube del anillo exterior al borde de la escama interior
            b = np.array([ce, cn]) + u * (ap - 2.0)
            a = b + u * (length + 1.0)
        else:                                          # sube de la escama interior al borde de la central
            b = u * (ap - 2.0)
            a = b + u * (length + 1.0)
        height = ramp(height, e, n, tuple(a), tuple(b), above(h0), above(h1), spec.ramp_w_m)
        corridors.append((tuple(a + u * 2.0), tuple(b - u * 2.5)))
    # Aletas: rampa de la playa (punta) al anillo exterior.
    for (c, ang) in flippers:
        u = np.array(c) / math.hypot(*c)
        a = np.array(c) + u * 10.0
        b = np.array(c) - u * 17.0                     # se mete en el caparazon (el borde queda a ~13 m)
        height = ramp(height, e, n, tuple(a), tuple(b), above(spec.beach_h_m), above(spec.h_outer), 8.0, blend_m=2.0)
        corridors.append((tuple(a), tuple(b - u * 4.0)))
    # Cabeza con la boca en tunel (Norte).
    head = dome(e, n, head_c[0], head_c[1], spec.head_r_m, spec.head_h_m - spec.beach_h_m)
    height = np.where(head > 0.0, np.maximum(height, above(spec.beach_h_m) + head), height)
    mouth_n0, mouth_n1 = head_c[1] + spec.head_r_m + 2.0, spec.shell_n - 4.0
    pts = np.column_stack([np.zeros(40), np.linspace(mouth_n0, mouth_n1, 40)])
    floor = above(spec.beach_h_m) + (spec.h_outer - spec.beach_h_m) * smoothstep(mouth_n0, mouth_n1, pts[:, 1])
    axis = Axis.of(pts, floor)
    tunnel = Tunnel(axis, spec.mouth_w_m, 6.5, name="boca")
    x_mouth = (np.abs(e) < spec.mouth_w_m / 2.0 + 1.0) & (n > mouth_n1) & (n < mouth_n0)
    height = np.where(x_mouth, np.maximum(height, np.interp(n, pts[:, 1][::-1], floor[::-1])), height)
    model = KitModel(canvas, height, voids=(tunnel,))
    required = {"escama_central": (0.0, 0.0), **{f"escama_int_{k}": _pt(pitch * 0.8, 90.0 + 60.0 * k) for k in range(6)},
                **{f"escama_ext_{k}": _pt(pitch * 1.75, 90.0 + 60.0 * k) for k in range(6)},
                "boca": (0.0, mouth_n0 + 3.0), "cola": (0.0, -spec.shell_n - 12.0)}
    markers = {"trampolin": [(0.0, -spec.shell_n - 12.0), flippers[0][0], flippers[1][0]],
               "catapulta": [_pt(pitch, 90.0), _pt(pitch, 270.0)],
               "hundimiento_60s": [_pt(pitch * 1.75, 90.0 + 60.0 * k) for k in range(6)],
               "hundimiento_100s": inner_c}
    params = {"generator": "tortuga_magna", **asdict(spec)}
    shape = ShapeMap(spec.name, spec.seed, spec.mode, spec.description, canvas, model, required["escama_ext_3"], (0.0, 0.0),
                     required, corridors, corridor_width_m=3.0, corridor_slope_deg=20.0, params=params)
    return shape, Extras(nests=TCT_NESTS, markers=markers,
                         clearances=[Clearance(Axis.of(pts[3:-3], floor[3:-3]), spec.mouth_w_m, 5.5, "boca")])


# ── I05 Santorini ────────────────────────────────────────────────────────────────
@dataclass(frozen=True)
class CalderaSpec:
    name: str
    seed: int
    grid: int = 2
    r_in: float = 30.0
    r_out: float = 75.0
    rim_h_m: float = 10.0
    edge_h_m: float = 2.0
    rim_flat_m: float = 9.0
    channels_deg: tuple[float, ...] = (118.0, 202.0)      # canales de agua que cortan el anillo (Thirasia)
    channel_w_m: float = 12.0
    bridged_channel: int = 0                               # canal con puente natural
    bridge_angles_deg: tuple[float, ...] = (0.0, 90.0)     # puentes que cruzan la caldera de lado a lado
    domes: int = 8
    description: str = ""
    mode: str = "tct"


def santorini_i05(seed: int = 1005) -> CalderaSpec:
    return CalderaSpec("I05_santorini", seed, description=(
        "Santorini (Todos contra Todos): anillo de caldera de 150 m con el borde interior en acantilado a +10 m y "
        "bajada suave a +2 m hacia fuera; dos puentes naturales cruzan la caldera, Thirasia queda separada por dos "
        "canales (uno con puente) y 8 cúpulas dan cobertura."))


def build_caldera(seed: int | None = None, spec: CalderaSpec | None = None) -> tuple[ShapeMap, Extras]:
    spec = spec or santorini_i05()
    if seed is not None:
        spec = replace(spec, seed=seed)
    canvas = Canvas(spec.grid)
    e, n = canvas.design_grid()
    rng = np.random.default_rng(spec.seed)
    from terrain_vol.noise import Fbm2D
    wobble = Fbm2D(rng, 25.0, octaves=3)
    r = np.hypot(e, n) + 2.0 * wobble(n + 300.0, e + 300.0)
    theta = np.degrees(np.arctan2(n, e))
    ring_sd = np.minimum(r - spec.r_in, spec.r_out - r)
    for deg in spec.channels_deg:
        lateral = np.abs(r * np.sin(np.radians(theta - deg)))
        facing = np.cos(np.radians(theta - deg)) > 0.0
        ring_sd = np.where(facing, np.minimum(ring_sd, lateral - spec.channel_w_m / 2.0), ring_sd)
    slope_t = np.clip((r - spec.r_in - spec.rim_flat_m) / (spec.r_out - spec.r_in - spec.rim_flat_m - 3.0), 0.0, 1.0)
    level = spec.rim_h_m - (spec.rim_h_m - spec.edge_h_m) * smoothstep(0.0, 1.0, slope_t)
    sea = sea_floor(e, n, ring_sd)
    top = above(level) - 1.0 + 1.0 * smoothstep(0.0, 1.5, ring_sd)
    inner = r < spec.r_in + 4.0
    shore = np.where(inner, smoothstep(-0.8, 0.6, ring_sd), smoothstep(-3.0, 3.0, ring_sd))
    height = sea + (np.maximum(top, above(0.5)) - sea) * shore
    domes = []
    for k in range(spec.domes):
        deg = 22.5 + 45.0 * k
        c = _pt(spec.r_in + 12.0, deg)
        if any(abs(((deg - ch + 180.0) % 360.0) - 180.0) < 12.0 for ch in spec.channels_deg):
            c = _pt(spec.r_in + 12.0, deg + 14.0)
        domes.append(c)
        height = height + dome(e, n, *c, 5.0, 3.5) * (ring_sd > 2.0)
    probe = ShapeModel(canvas, height)

    def height_at(pe: float, pn: float) -> float:
        X, Y = canvas.to_world(pe, pn)
        return float(probe.ground_height(np.array([X]), np.array([Y]))[0])

    bridges, corridors = [], []
    for deg in spec.bridge_angles_deg:
        a, b = _pt(spec.r_in + 3.5, deg), _pt(spec.r_in + 3.5, deg + 180.0)
        bridge = NaturalBridge(a, b, height_at(*a), height_at(*b), width_m=4.0, rise_m=0.8)
        bridges.append(bridge)
        corridors.append(bridge.inner_points(1.5))
    ch = spec.channels_deg[spec.bridged_channel]
    mid_r = spec.r_in + 0.55 * (spec.r_out - spec.r_in)
    half_gap = math.degrees((spec.channel_w_m / 2.0 + 4.0) / mid_r)
    a, b = _pt(mid_r, ch - half_gap), _pt(mid_r, ch + half_gap)
    bridge = NaturalBridge(a, b, height_at(*a), height_at(*b), width_m=4.0, rise_m=0.4)
    bridges.append(bridge)
    corridors.append(bridge.inner_points(1.5))
    model = KitModel(canvas, height, tuple(bridges))
    required = {"este": _pt(mid_r, 0.0 + 45.0), "norte": _pt(mid_r, 80.0), "thirasia": _pt(mid_r, 160.0),
                "sur": _pt(mid_r, 270.0 + 20.0)}
    markers = {"catapulta": [_pt(spec.r_in + 6.0, 45.0), _pt(spec.r_in + 6.0, 225.0)],
               "trampolin": [_pt(spec.r_out - 8.0, d) for d in (60.0, 180.0, 300.0)], "cupula": domes}
    params = {"generator": "caldera", **asdict(spec)}
    shape = ShapeMap(spec.name, spec.seed, spec.mode, spec.description, canvas, model, required["sur"], None, required,
                     corridors, corridor_width_m=3.0, corridor_slope_deg=20.0, params=params)
    return shape, Extras(nests=TCT_NESTS, markers=markers)


MAPS = {
    "I02": ("C2", lambda seed: build_galapagos(seed)),
    "I03T": ("C2", lambda seed: build_turtle(seed)),
    "I05": ("C2", lambda seed: build_caldera(seed)),
}

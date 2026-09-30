"""Circuitos de Rally con lazos y tuneles (lote 3 y lazos a dos niveles), sobre terrain_shapes/rally_circuit.py:

  N09 Cañon serpiente:   lazo en eses por el fondo de un cañon; dos tuneles atajan los meandros.
  N10 Fiordo de puentes: cuatro viaductos de orilla a orilla y tunel bajo el macizo del oeste.
  N11 Montaña hueca:     tunel de entrada, sala interior con lucernario y tunel de salida.
  N12 Delta de islotes:  nueve islotes unidos por nueve puentes (auto_water) y tunel en el islote de roca.
  N19 Sima sacacorchos:  espiral de vuelta y media que baja a una sima alrededor de un piton y sale por un
                         tunel bajo su propia rampa.
  N20 Nudo de trebol:    el lazo se cruza tres veces consigo mismo, alternando paso elevado y paso inferior.

Cada base devuelve (cota del terreno, distancia con signo de la tierra) sobre el raster; la calzada se talla
encima (camino primero). Todo con semilla (ruido de relieve y de costa) y parametros en RallySpec.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, fields, replace

import numpy as np

from terrain_vol.noise import Fbm2D

from .canvas import sd_box, smoothstep
from .kit import Axis, Deck, Tunnel, above, along_polyline, arc_length, resample
from .kit_writer import Clearance, Extras
from .model import ShapeMap
from .rally_circuit import RallySpec, build_rally, default_base, find_crossings, regional_level

Built = tuple[ShapeMap, Extras]
TUNNEL_CHECK_W, TUNNEL_CHECK_H, TUNNEL_COVER_M = 12.0, 5.0, 8.0     # lo que exige build_rally a cada tunel


# ── Utilidades ───────────────────────────────────────────────────────────────────
def _noise(seed: int, wavelength: float, octaves: int = 3) -> Fbm2D:
    return Fbm2D(np.random.default_rng(seed), wavelength, octaves=octaves)


def _polar(points, centre: tuple[float, float] = (0.0, 0.0)) -> tuple[tuple[float, float], ...]:
    """(grados desde el Este, radio) -> (e, n) alrededor de centre."""
    return tuple((centre[0] + r * math.cos(math.radians(t)), centre[1] + r * math.sin(math.radians(t)))
                 for t, r in points)


def lap_axis(control) -> tuple[np.ndarray, np.ndarray, float]:
    """Eje denso (1 m) del lazo, su arco acumulado y la longitud de la vuelta."""
    pts = resample(control, 1.0, closed=True)
    arc = arc_length(pts)
    return pts, arc, float(arc[-1] + np.hypot(*(pts[0] - pts[-1])))


def _shore(terrain: np.ndarray, land: np.ndarray, run_m: float, beach_m: float = 1.5) -> np.ndarray:
    """Baja el relieve hasta la playa (beach_m sobre el agua) en los run_m ultimos metros de tierra."""
    return above(beach_m) + (terrain - above(beach_m)) * smoothstep(0.0, run_m, land)


def _angle_off(theta_deg, centre_deg: float):
    """Diferencia de angulo en (-180, 180]."""
    return (np.asarray(theta_deg, dtype=np.float64) - centre_deg + 180.0) % 360.0 - 180.0


@dataclass(frozen=True)
class FlushDeck(Deck):
    """Tablero cortado a escuadra 1 m mas alla de sus extremos. El casquete redondo de Deck asoma medio ancho
    sobre la calzada de tierra y, si esta va en pendiente, deja un escalon de medio metro."""

    def density(self, e, n, Z: np.ndarray) -> np.ndarray:
        _, k = self.axis.nearest(e, n)
        a = self.axis.array
        cap = np.full(np.shape(e), 1e3)
        for end, inner in ((0, 1), (len(a) - 1, len(a) - 2)):
            u = (a[inner] - a[end]) / max(float(np.hypot(*(a[inner] - a[end]))), 1e-9)
            cap = np.where(k == end, np.minimum(cap, 1.0 + (e - a[end][0]) * u[0] + (n - a[end][1]) * u[1]), cap)
        return np.minimum(super().density(e, n, Z), cap[..., None])


def _rally(spec: RallySpec, base=None) -> Built:
    """build_rally con los tableros cortados a escuadra (FlushDeck)."""
    shape, extras = build_rally(spec, base)
    shape.model.solids = tuple(FlushDeck(**{f.name: getattr(s, f.name) for f in fields(Deck)}) if isinstance(s, Deck)
                               else s for s in shape.model.solids)
    return shape, extras


def _with_markers(built: Built, markers: dict) -> Built:
    built[1].markers.update(markers)
    return built


# ── N09 Cañon serpiente ──────────────────────────────────────────────────────────
CANYON_R, CANYON_WAVE, CANYON_LOBES = 150.0, 24.0, 5          # lazo r = R + WAVE * cos(LOBES * angulo)
CANYON_MEANDERS = (72.0, 216.0)                               # lobulos que el cañon rodea y la calzada ataja
CANYON_NECK_DEG = 36.0                                        # medio angulo del atajo (de seno a seno)
CANYON_PLATEAU_M = 21.0
CANYON_MEANDER = ((34.0, -24.0), (30.0, -16.0), (26.0, 10.0), (20.0, 46.0), (10.0, 66.0), (0.0, 72.0))


def canyon_radius(theta_deg: float) -> float:
    """Radio de la calzada: onda de cinco lobulos, plana (por dentro) en los dos lobulos con meandro."""
    if any(abs(float(_angle_off(theta_deg, p))) <= CANYON_NECK_DEG for p in CANYON_MEANDERS):
        return CANYON_R - CANYON_WAVE
    return CANYON_R + CANYON_WAVE * math.cos(math.radians(CANYON_LOBES * theta_deg))


def canyon_meander(peak_deg: float) -> np.ndarray:
    """Fondo del meandro: sale de la calzada, rodea el espolon por fuera y vuelve (eje denso)."""
    half = [(peak_deg - off, CANYON_R + dr) for off, dr in CANYON_MEANDER]
    back = [(peak_deg + off, CANYON_R + dr) for off, dr in reversed(CANYON_MEANDER[:-1])]
    return resample(_polar(half + back), 1.0)


def canyon_spec(seed: int = 2209) -> RallySpec:
    ctrl = _polar([(float(t), canyon_radius(float(t))) for t in range(0, 360, 9)])
    return RallySpec(
        "N09_canon_serpiente", seed, ctrl, heights=((0.0, 7.5), (8.0, 4.0), (16.0, 8.0), (24.0, 4.0), (32.0, 8.0)),
        by_ctrl=True, auto_tunnel_m=14.0, land_m=40.0, dune_m=1.0,
        description="Cañón serpiente (Rally): lazo de 1 km en eses por el fondo de un cañón tallado en una mesa de "
                    "+21 m; el cañón rodea dos espolones en meandro y la calzada los ataja con dos túneles "
                    "curvos. Calzada de +4 a +8 m, taludes a 31°.")


def canyon_base(spec: RallySpec):
    wobble, rough = _noise(spec.seed, 45.0), _noise(spec.seed + 1, 70.0, 2)
    meanders = np.vstack([canyon_meander(p) for p in CANYON_MEANDERS])

    def base(canvas, e, n, ctx):
        theta = np.degrees(np.arctan2(ctx.pts[:, 1], ctx.pts[:, 0]))
        in_neck = np.zeros(len(theta), dtype=bool)
        for peak in CANYON_MEANDERS:
            in_neck |= np.abs(_angle_off(theta, peak)) < CANYON_NECK_DEG - 8.0
        d_floor, _ = along_polyline(e, n, np.vstack([ctx.pts[~in_neck], meanders]))
        level = regional_level(canvas, ctx)
        plateau = above(CANYON_PLATEAU_M) + 1.0 * rough(n, e)
        terrain = level + (plateau - level) * smoothstep(15.0, 34.0, d_floor)
        phi = np.degrees(np.arctan2(n, e))
        rim = 240.0 + sum(38.0 * np.exp(-(_angle_off(phi, p) / 22.0) ** 2) for p in CANYON_MEANDERS)
        land = rim - np.hypot(e, n) + 6.0 * wobble(n, e)
        return _shore(terrain, land, 26.0), land
    return base


def build_canyon(seed: int | None = None) -> Built:
    spec = canyon_spec(seed or 2209)
    return _with_markers(_rally(spec, canyon_base(spec)),
                         {"meandro": list(_polar([(p, CANYON_R + 60.0) for p in CANYON_MEANDERS]))})


# ── N10 Fiordo de los puentes ────────────────────────────────────────────────────
FJORD_HALF_M, FJORD_SWAY_M, FJORD_HEAD_N = 30.0, 22.0, 205.0
# Eje (e, n, cota): cuatro cruces del fiordo (Oeste-Este alternos) con una vuelta en cada orilla entre cruce y
# cruce, y regreso por el oeste bajo el macizo.
FJORD_AXIS = ((-150.0, -236.0, 9.0), (-70.0, -214.0, 10.0), (0.0, -208.0, 11.0), (70.0, -212.0, 10.0),
              (135.0, -195.0, 11.0), (160.0, -142.0, 14.0), (135.0, -92.0, 11.0),
              (70.0, -76.0, 10.0), (0.0, -72.0, 11.0), (-70.0, -72.0, 10.0),
              (-128.0, -52.0, 11.0), (-150.0, -5.0, 14.0), (-128.0, 42.0, 11.0),
              (-70.0, 58.0, 10.0), (0.0, 62.0, 11.0), (70.0, 66.0, 10.0),
              (135.0, 84.0, 11.0), (160.0, 130.0, 14.0), (135.0, 176.0, 11.0),
              (70.0, 192.0, 10.0), (0.0, 196.0, 11.0), (-70.0, 200.0, 10.0),
              (-160.0, 212.0, 9.0), (-226.0, 170.0, 8.0), (-246.0, 90.0, 7.0), (-250.0, 0.0, 6.0),
              (-250.0, -90.0, 7.0), (-232.0, -180.0, 8.0))


def fjord_spec(seed: int = 2210) -> RallySpec:
    return RallySpec(
        "N10_fiordo_puentes", seed, tuple((e, n) for e, n, _ in FJORD_AXIS),
        heights=tuple((float(k), h) for k, (_, _, h) in enumerate(FJORD_AXIS)),
        tunnels=((24.3, 25.7, 17.0),), hill_radius_m=55.0, by_ctrl=True, auto_water=True, channel_m=0.0,
        land_m=30.0, dune_m=1.5,
        description="Fiordo de los puentes (Rally): un fiordo de 60 m de ancho parte la isla de sur a norte y el "
                    "circuito lo salta cuatro veces en viaducto a +11 m, con una vuelta en cada orilla entre salto "
                    "y salto; regresa por el oeste en túnel bajo el macizo. 2,1 km por vuelta.")


def fjord_base(spec: RallySpec):
    wobble, crags = _noise(spec.seed, 40.0), _noise(spec.seed + 2, 26.0, 2)

    def base(canvas, e, n, ctx):
        half = FJORD_HALF_M - 45.0 * smoothstep(FJORD_HEAD_N, FJORD_HEAD_N + 47.0, n)
        shore = np.abs(e - FJORD_SWAY_M * np.sin(n / 55.0)) - half + 3.0 * wobble(n + 300.0, e)
        island = sd_box(e, n, 0.0, 0.0, 286.0, 286.0, corner=95.0) + 6.0 * wobble(n, e)
        level = regional_level(canvas, ctx)
        peaks = above(6.0) + 16.0 * smoothstep(20.0, 110.0, shore) * smoothstep(0.0, 70.0, island) + 1.5 * crags(n, e)
        terrain = level + (peaks - level) * smoothstep(spec.road_w, spec.road_w + 45.0, ctx.d_ground)
        return terrain, np.minimum(island, shore)
    return base


def build_fjord(seed: int | None = None) -> Built:
    spec = fjord_spec(seed or 2210)
    return _rally(spec, fjord_base(spec))


# ── N11 Montaña hueca ────────────────────────────────────────────────────────────
HALL_R, HALL_H, HALL_WALL, SKYLIGHT_R = 48.0, 16.0, 5.0, 9.0
HOLLOW_PEAK_M, HOLLOW_FOOT_M, HOLLOW_COAST_R, HOLLOW_FLOOR_M = 37.0, 3.0, 260.0, 6.0
# Eje en polares alrededor de la cima (grados, radio, cota): cornisa exterior por el norte, giro hacia dentro por
# el suroeste, curva dentro de la sala y salida por el sureste.
HOLLOW_AXIS = ((-35.0, 214.0, 5.0), (-10.0, 216.0, 6.0), (15.0, 214.0, 8.0), (40.0, 212.0, 10.0), (65.0, 210.0, 12.0),
               (90.0, 210.0, 13.0), (115.0, 210.0, 12.0), (140.0, 212.0, 10.0), (165.0, 214.0, 8.0),
               (190.0, 216.0, 7.0), (208.0, 214.0, 6.0), (222.0, 208.0, 6.0), (231.0, 174.0, 6.0), (231.0, 128.0, 6.0),
               (230.0, 84.0, 6.0), (230.0, 44.0, 6.0), (270.0, 25.0, 6.0), (310.0, 44.0, 6.0), (310.0, 84.0, 6.0),
               (309.0, 128.0, 6.0), (309.0, 174.0, 6.0), (318.0, 208.0, 5.0))


@dataclass(frozen=True)
class Hall:
    """Sala excavada: planta circular, pared recta de HALL_WALL y boveda eliptica hasta height_m en el centro,
    con un lucernario (pozo vertical hasta el cielo) de skylight_m de radio."""
    center: tuple[float, float]
    radius_m: float
    floor_z: float
    height_m: float
    skylight_m: float
    name: str = "sala"

    def bbox(self) -> tuple[float, float, float, float]:
        r = self.radius_m + 2.0
        return self.center[0] - r, self.center[0] + r, self.center[1] - r, self.center[1] + r

    def void(self, e, n, Z: np.ndarray) -> np.ndarray:
        r = np.hypot(e - self.center[0], n - self.center[1])
        vault = np.sqrt(np.clip(1.0 - (r / self.radius_m) ** 2, 0.0, 1.0))
        ceiling = self.floor_z + HALL_WALL + (self.height_m - HALL_WALL) * vault
        up = Z[None, None, :] - self.floor_z
        cavern = np.minimum(np.minimum((self.radius_m - r)[..., None], ceiling[..., None] - Z[None, None, :]), up)
        return np.maximum(cavern, np.minimum((self.skylight_m - r)[..., None], up))


def hollow_spec(seed: int = 2211) -> RallySpec:
    return RallySpec(
        "N11_montana_hueca", seed, _polar([(t, r) for t, r, _ in HOLLOW_AXIS]),
        heights=tuple((float(k), h) for k, (_, _, h) in enumerate(HOLLOW_AXIS)),
        by_ctrl=True, auto_tunnel_m=14.0, land_m=40.0, dune_m=1.0,
        description="Montaña hueca (Rally): cornisa de +5 a +13 m alrededor de una montaña de +40 m; el circuito "
                    "entra por un túnel al suroeste, gira dentro de una sala de 96 m de diámetro y 16 m de alto "
                    "con lucernario, y sale por otro túnel al sureste. 1,6 km por vuelta.")


def hollow_base(spec: RallySpec):
    wobble, crags = _noise(spec.seed, 45.0), _noise(spec.seed + 2, 30.0, 2)

    def base(canvas, e, n, ctx):
        rho = np.hypot(e, n)
        massif = above(HOLLOW_FOOT_M) + HOLLOW_PEAK_M * (1.0 - smoothstep(40.0, 190.0, rho)) \
            + 1.0 * crags(n, e) * smoothstep(60.0, 110.0, rho)
        terrain = np.maximum(massif, regional_level(canvas, ctx))
        land = HOLLOW_COAST_R - rho + 7.0 * wobble(n, e)
        return _shore(terrain, land, 24.0), land
    return base


def hollow_out(built: Built) -> Built:
    """Parte el tunel unico que encuentra build_rally en entrada, sala y salida: la sala es un hueco propio
    (Hall) y se comprueba con su altura libre, sin exigirle la cobertura de un tunel (tiene lucernario)."""
    shape, extras = built
    bore = next(v for v in shape.model.voids if isinstance(v, Tunnel))
    pts, zs = bore.axis.array, bore.axis.zs
    inside = np.nonzero(np.hypot(pts[:, 0], pts[:, 1]) < HALL_R - 8.0)[0]
    first, last = int(inside[0]), int(inside[-1])
    hall = Hall((0.0, 0.0), HALL_R, float(zs[inside].mean()), HALL_H, SKYLIGHT_R)
    halves = [Axis.of(pts[:first + 1], zs[:first + 1]), Axis.of(pts[last:], zs[last:])]
    shape.model.voids = tuple(Tunnel(a, bore.width_m, bore.clearance_m, "tunel") for a in halves) + (hall,)
    extras.clearances = [Clearance(a, TUNNEL_CHECK_W, TUNNEL_CHECK_H, "tunel", min_cover_m=TUNNEL_COVER_M)
                         for a in halves]
    extras.clearances.append(Clearance(Axis.of(pts[first:last + 1], zs[first:last + 1]), 24.0, 10.0, "sala"))
    shape.params.update({"hall_radius_m": HALL_R, "hall_height_m": HALL_H, "skylight_radius_m": SKYLIGHT_R})
    return _with_markers(built, {"sala": [(0.0, 0.0)], "lucernario": [(0.0, 0.0)]})


def build_hollow(seed: int | None = None) -> Built:
    spec = hollow_spec(seed or 2211)
    return hollow_out(_rally(spec, hollow_base(spec)))


# ── N12 Delta de los islotes ─────────────────────────────────────────────────────
DELTA_ISLETS, DELTA_ROCK = 9, 3                               # islotes por vuelta y el de roca (con tunel)
DELTA_BARS = ((0.0, 25.0), (-52.0, -38.0), (56.0, -30.0))     # barras de arena de la laguna (sin calzada)


def delta_spec(seed: int = 2212) -> RallySpec:
    ctrl = _polar([(float(t), 190.0 + 38.0 * math.cos(math.radians(3.0 * (t - 90.0)))) for t in range(-30, 330, 15)])
    heights = tuple(key for k in range(DELTA_ISLETS)
                    for key in ((k / DELTA_ISLETS, 3.5), ((k + 0.5) / DELTA_ISLETS, 6.5)))
    rock = DELTA_ROCK / DELTA_ISLETS
    return RallySpec(
        "N12_delta_islotes", seed, ctrl, heights=heights, tunnels=((rock - 0.03, rock + 0.03, 17.0),),
        hill_radius_m=48.0, auto_water=True, channel_m=0.0, land_m=40.0, dune_m=1.2,
        description="Delta de los islotes (Rally): nueve islotes de arena en abanico unidos por nueve puentes en "
                    "lomo de +6,5 m sobre los caños; el islote del vértice es de roca y se cruza en túnel. Laguna "
                    "con barras de arena en el centro. 1,2 km por vuelta.")


def delta_base(spec: RallySpec):
    wobble, dunes = _noise(spec.seed, 35.0), _noise(spec.seed + 1, 50.0, 2)
    rng = np.random.default_rng(spec.seed)
    widths = rng.uniform(36.0, 46.0, DELTA_ISLETS)
    widths[DELTA_ROCK] = 54.0
    halves = np.full(DELTA_ISLETS, 45.0)
    halves[DELTA_ROCK] = 58.0

    def base(canvas, e, n, ctx):
        total = float(ctx.arc[-1]) + 1.0
        turn = ctx.arc[ctx.k_full] / total * DELTA_ISLETS
        islet = np.round(turn).astype(int) % DELTA_ISLETS
        along = np.abs(turn - np.round(turn)) * total / DELTA_ISLETS          # metros al centro del islote
        size = np.minimum(widths[islet], halves[islet])                       # islote eliptico a lo largo del eje
        land = size * (1.0 - np.hypot(along / halves[islet], ctx.d_full / widths[islet])) + 7.0 * wobble(n, e)
        terrain = regional_level(canvas, ctx) \
            + spec.dune_m * dunes(n, e) * smoothstep(spec.road_w, spec.road_w + 30.0, ctx.d_ground)
        for be, bn in DELTA_BARS:
            bar = 15.0 - np.hypot(e - be, n - bn) + 3.0 * wobble(n + 200.0, e)
            terrain = np.where(bar > land, above(1.3), terrain)
            land = np.maximum(land, bar)
        return _shore(terrain, land, 10.0), land
    return base


def build_delta(seed: int | None = None) -> Built:
    spec = delta_spec(seed or 2212)
    shape, extras = _rally(spec, delta_base(spec))
    shape.unreachable_ok = list(DELTA_BARS)
    return _with_markers((shape, extras), {"barra_arena": list(DELTA_BARS)})


# ── N19 Sima del sacacorchos ─────────────────────────────────────────────────────
CORK_CENTRE = (0.0, 25.0)
CORK_R0, CORK_PITCH_M, CORK_TURNS = 125.0, 54.0, 1.5          # radio de entrada, paso radial por vuelta y vueltas
CORK_TOP_M, CORK_FLOOR_M, CORK_EXIT_M = 36.0, 6.0, 4.0
CORK_STEPS = 18                                               # puntos de control de la espiral (uno cada 30 grados)
CORK_SPIRE_M, CORK_SPIRE_R, CORK_RIM_R = 30.0, 18.0, 134.0
# Salida (e, n respecto al centro): ese a la derecha y recta radial en tunel bajo la primera vuelta; despues,
# giro a la izquierda y subida por la falda sur (grados, radio) hasta la boca de la espiral.
CORK_EXIT = ((-47.0, -17.0), (-59.0, -37.0), (-77.0, -53.0), (-101.0, -70.0), (-131.0, -91.0), (-158.0, -110.0),
             (-174.5, -135.7), (-168.0, -165.6))
CORK_CLIMB = ((240.0, 226.0), (258.0, 222.0), (276.0, 218.0), (294.0, 213.0), (312.0, 205.0), (328.0, 190.0),
              (342.0, 168.0), (352.0, 146.0))


def corkscrew_spec(seed: int = 2219) -> RallySpec:
    spiral = [(30.0 * k, CORK_R0 - CORK_PITCH_M * k / 12.0) for k in range(CORK_STEPS + 1)]
    oe, on = CORK_CENTRE
    ctrl = _polar(spiral, CORK_CENTRE) + tuple((oe + e, on + n) for e, n in CORK_EXIT) + _polar(CORK_CLIMB, CORK_CENTRE)
    tunnel_out = float(CORK_STEPS + 6)
    # El tunel empieza al pie de la espiral, en un espolon de roca, para que el cruce quede a mas de 40 m de la
    # boca: el prisma de la calzada de fuera no deja roca sobre la boveda mas cerca.
    return RallySpec(
        "N19_sima_sacacorchos", seed, ctrl,
        heights=((0.0, CORK_TOP_M), (float(CORK_STEPS), CORK_FLOOR_M), (tunnel_out, CORK_EXIT_M)),
        tunnels=((CORK_STEPS + 0.7, CORK_STEPS + 4.5, 14.5),), hill_radius_m=40.0,
        by_ctrl=True, auto_tunnel_m=14.0, land_m=40.0, dune_m=1.0,
        description="Sima del sacacorchos (Rally): la calzada corona el borde de una sima a +34 m, baja en espiral "
                    "vuelta y media alrededor de un pitón hasta el fondo (+7 m) y sale por un túnel que pasa bajo "
                    "su propia rampa; después rodea la falda sur y vuelve a subir. 1,6 km por vuelta.")


def corkscrew_base(spec: RallySpec):
    wobble = _noise(spec.seed, 45.0)
    oe, on = CORK_CENTRE

    def base(canvas, e, n, ctx):
        rho = np.hypot(e - oe, n - on)
        end = int(np.argmin(np.hypot(*(ctx.pts - np.asarray(spec.control[CORK_STEPS])).T)))
        rs = np.hypot(ctx.pts[:end + 1, 0] - oe, ctx.pts[:end + 1, 1] - on)
        order = np.argsort(rs)
        # La espiral de Arquimedes con pendiente uniforme esta sobre un cono: la cota solo depende del radio.
        cone = np.interp(rho, rs[order], ctx.z[:end + 1][order]) + np.clip(rho - rs.max(), 0.0, None) / 3.0
        spire = ctx.z[end] + CORK_SPIRE_M * (1.0 - smoothstep(4.0, CORK_SPIRE_R, rho))
        level = regional_level(canvas, ctx)
        inner = 1.0 - smoothstep(CORK_RIM_R, CORK_RIM_R + 38.0, rho)
        terrain = level + (np.maximum(cone, spire) - level) * inner
        land = 231.0 - 31.0 * (n - on) / np.maximum(rho, 1.0) - rho + 6.0 * wobble(n, e)
        return _shore(terrain, land, 36.0), land
    return base


def ramp_crossings(built: Built) -> Built:
    """Donde el tramo bajo de un cruce va en tunel, el paso inferior se comprueba con el galibo del tunel y con
    la roca que lo separa de la rampa de encima (build_rally lo mide como un paso a cielo abierto)."""
    _, extras = built
    extras.clearances = [
        replace(c, width_m=TUNNEL_CHECK_W, clearance_m=TUNNEL_CHECK_H, min_cover_m=TUNNEL_COVER_M)
        if c.name == "paso_inferior" else c for c in extras.clearances]
    return built


def clear_overpasses(built: Built, road_w: float, shoulder_m: float, talud_deg: float = 31.0) -> Built:
    """Recorta el campo de alturas junto a cada paso elevado: por debajo del tablero en su ancho y en talud desde
    el borde del arcen. build_rally solo lo hace donde el eje mas cercano es el del tablero; en el cuadrante
    entre los dos ejes del cruce manda el tramo bajo y la ladera del cono quedaba por encima del tablero."""
    shape, _ = built
    height = shape.model.height.copy()
    e, n = shape.canvas.design_grid()
    tan = math.tan(math.radians(talud_deg))
    for deck in (s for s in shape.model.solids if s.name == "paso_elevado"):
        e0, e1, n0, n1 = deck.axis.bbox(road_w + 40.0)
        win = (e >= e0) & (e <= e1) & (n >= n0) & (n <= n1)
        d, k = deck.axis.nearest(e[win], n[win])
        top = deck.axis.zs[k]
        cap = np.where(d < road_w / 2.0 + 2.0, top - 0.3, top + np.clip(d - road_w / 2.0 - shoulder_m, 0.0, None) * tan)
        inner = (k > 0) & (k < len(deck.axis.zs) - 1)          # mas alla de los extremos sigue la calzada de tierra
        height[win] = np.where(inner, np.minimum(height[win], cap), height[win])
    shape.model.height = height
    return built


def build_corkscrew(seed: int | None = None) -> Built:
    spec = corkscrew_spec(seed or 2219)
    built = clear_overpasses(ramp_crossings(_rally(spec, corkscrew_base(spec))), spec.road_w, spec.shoulder_m)
    return _with_markers(built, {"piton": [CORK_CENTRE]})


# ── N20 Nudo de trebol ───────────────────────────────────────────────────────────
TREFOIL_SCALE, TREFOIL_LOW_M, TREFOIL_HIGH_M = 70.0, 4.0, 14.0


def trefoil_passes(control) -> tuple[list[float], list[float]]:
    """Fracciones de vuelta de los seis pasos por los tres cruces y de las puntas de los tres lobulos."""
    pts, arc, total = lap_axis(control)
    passes = sorted(float(arc[k] / total) for pair in find_crossings(pts, arc, total) for k in pair)
    gaps = [((b - a) % 1.0, (a + 0.5 * ((b - a) % 1.0)) % 1.0) for a, b in zip(passes, passes[1:] + passes[:1])]
    return passes, sorted(tip for _, tip in sorted(gaps)[-3:])


def trefoil_spec(seed: int = 2220) -> RallySpec:
    s = TREFOIL_SCALE
    ctrl = tuple((s * (math.sin(t) + 2.0 * math.sin(2.0 * t)), s * (math.cos(t) - 2.0 * math.cos(2.0 * t)))
                 for t in np.linspace(0.0, 2.0 * math.pi, 36, endpoint=False))
    passes, tips = trefoil_passes(ctrl)
    heights = tuple((t, TREFOIL_HIGH_M if k % 2 == 0 else TREFOIL_LOW_M) for k, t in enumerate(passes))
    return RallySpec(
        "N20_nudo_trebol", seed, ctrl, heights=heights,
        tunnels=tuple(((t - 0.022) % 1.0, (t + 0.022) % 1.0, 17.0) for t in (tips[0], tips[2])),
        water_spans=(((tips[1] - 0.02) % 1.0, (tips[1] + 0.02) % 1.0),), hill_radius_m=55.0, land_m=46.0, dune_m=1.5,
        description="Nudo de trébol (Rally): el lazo dibuja un nudo de tres lóbulos y se cruza tres veces consigo "
                    "mismo, alternando paso elevado (+14 m) y paso inferior (+4 m); dos lóbulos se cruzan en túnel "
                    "y el tercero en viaducto sobre un estrecho, con una laguna dentro de cada lóbulo.")


def build_trefoil(seed: int | None = None) -> Built:
    spec = trefoil_spec(seed or 2220)
    return _rally(spec, default_base(spec))


MAPS = {
    "N09": ("3", build_canyon),
    "N10": ("3", build_fjord),
    "N11": ("3", build_hollow),
    "N12": ("3", build_delta),
    "N19": ("5", build_corkscrew),
    "N20": ("5", build_trefoil),
}

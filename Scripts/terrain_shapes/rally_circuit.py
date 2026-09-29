"""Circuitos de Rally Tortuga parametricos (lazo cerrado): calzada tallada de 14 m con arcenes, perfil por claves
(<= 12 grados), radio >= 25 m, viaductos sobre el agua, tuneles bajo cerros, y pasos a dos niveles donde el lazo
se cruza consigo mismo (el tramo alto va en tablero y el bajo pasa por debajo).

    spec = RallySpec("R_prueba", 7, control=((..., ...), ...), heights=((0.0, 2.0), (0.5, 12.0)))
    shape, extras = build_rally(spec)                      # ShapeMap + Extras (Road, huecos) para write_kit_map

El terreno de alrededor lo da `base` (funcion (canvas, e, n, ctx) -> (cota, distancia con signo de la tierra));
por defecto, dunas suaves que siguen la cota de la calzada y costa a land_m del eje.
"""

from __future__ import annotations

import math
from dataclasses import asdict, dataclass
from typing import Callable

import numpy as np

from terrain_vol.noise import Fbm2D

from .canvas import SEABED_M, Canvas, smoothstep
from .kit import Axis, Deck, KitModel, Tunnel, above, along_polyline, arc_length, profile, resample
from .kit_writer import Clearance, Extras, Road
from .model import ShapeMap

DECK_THICKNESS_M = 1.6
UNDERPASS_CLEAR_M = 6.0
OVERPASS_HALF_M = 36.0
PORTAL_TAN = math.tan(math.radians(60.0))    # fachada de las bocas de tunel          # medio largo del tablero de un paso elevado (los terraplenes quedan lejos del bajo)


@dataclass(frozen=True)
class RallySpec:
    name: str
    seed: int
    control: tuple[tuple[float, float], ...]          # puntos de control del lazo (e, n), spline cerrado
    heights: tuple[tuple[float, float], ...]          # (fraccion de vuelta, cota sobre el agua)
    grid: int = 6
    road_w: float = 14.0
    shoulder_m: float = 5.0
    land_m: float = 45.0                              # costa por defecto: a esto del eje
    water_spans: tuple[tuple[float, float], ...] = ()          # (t0, t1) en viaducto sobre el agua
    tunnels: tuple[tuple[float, float, float], ...] = ()       # (t0, t1, alto del cerro sobre la calzada)
    hill_radius_m: float = 55.0
    laps: int = 2
    dune_m: float = 2.0
    by_ctrl: bool = False              # claves de cota y tramos por indice de punto de control (no por fraccion)
    channel_m: float | None = None     # radio del estrecho tallado bajo cada viaducto (None: land_m + 25; 0: nada)
    auto_water: bool = False           # viaducto donde el terreno de `base` es agua bajo el eje
    auto_tunnel_m: float = 0.0         # >0: tunel donde el terreno de `base` queda al menos esto sobre el eje
    max_talud_deg: float = 31.0        # taludes de desmonte y terraplen (el criterio es <= 35 grados)
    description: str = ""
    mode: str = "rally"


@dataclass
class RoadContext:
    pts: np.ndarray                 # eje denso (1 m)
    z: np.ndarray                   # cota absoluta del eje
    arc: np.ndarray
    ground: np.ndarray              # muestras del eje apoyadas en el campo de alturas
    d_ground: np.ndarray            # distancia de cada pixel al eje apoyado
    k_ground: np.ndarray            # indice (en pts) del punto apoyado mas cercano
    d_full: np.ndarray
    k_full: np.ndarray


Base = Callable[[Canvas, np.ndarray, np.ndarray, RoadContext], tuple[np.ndarray, np.ndarray]]


def span_mask(arc: np.ndarray, total: float, t0: float, t1: float) -> np.ndarray:
    t = arc / total
    return (t >= t0) & (t <= t1) if t1 >= t0 else (t >= t0) | (t <= t1)


def find_crossings(pts: np.ndarray, arc: np.ndarray, total: float) -> list[tuple[int, int]]:
    """Pares (a, b) de muestras del lazo que se cruzan (a menos de 1,5 m y lejos en arco), uno por cruce."""
    from scipy.spatial import cKDTree
    pairs = []
    for a, b in sorted(cKDTree(pts).query_pairs(1.5)):
        gap = abs(arc[a] - arc[b])
        if min(gap, total - gap) > 80.0 and all(min(abs(arc[a] - arc[p]), total - abs(arc[a] - arc[p])) > 60.0 for p, _ in pairs):
            pairs.append((a, b))
    return pairs


def default_base(spec: RallySpec) -> Base:
    def base(canvas, e, n, ctx: RoadContext):
        noise = Fbm2D(np.random.default_rng(spec.seed), 60.0, octaves=2)
        wobble = Fbm2D(np.random.default_rng(spec.seed + 1), 45.0, octaves=3)
        level = regional_level(canvas, ctx)
        dunes = spec.dune_m * noise(n + 500.0, e + 500.0)
        terrain = level + dunes * smoothstep(spec.road_w, spec.road_w + 30.0, ctx.d_ground)
        land = spec.land_m + 12.0 * wobble(n, e) - ctx.d_full
        return terrain, land
    return base


def regional_level(canvas: Canvas, ctx: RoadContext, sigma_m: float = 30.0) -> np.ndarray:
    """Cota de la calzada mas cercana suavizada (sin los saltos de Voronoi entre tramos a distinta cota)."""
    from scipy import ndimage
    return ndimage.gaussian_filter(ctx.z[ctx.k_ground], sigma_m / canvas.px_m, mode="nearest")


def _hill(ctx: RoadContext, e, n, idx: np.ndarray, hill_h: float, radius: float) -> np.ndarray:
    """Cerro en cupula sobre el tramo en tunel (distancia al tramo, con casquetes en los extremos)."""
    d, k = along_polyline(e, n, ctx.pts[idx])
    bump = hill_h * (1.0 - smoothstep(0.3 * radius, radius, d))
    return np.where(bump > 0.05, ctx.z[idx][k] + bump, -1e3)


def build_rally(spec: RallySpec, base: Base | None = None) -> tuple[ShapeMap, Extras]:
    canvas = Canvas(spec.grid)
    e, n = canvas.design_grid()
    pts = resample(spec.control, 1.0, closed=True)
    arc = arc_length(pts)
    total = float(arc[-1] + np.hypot(*(pts[0] - pts[-1])))
    to_t = _ctrl_fraction(spec, pts, arc, total) if spec.by_ctrl else (lambda v: v)
    z = above(profile(arc, [(to_t(t), h) for t, h in spec.heights], total=total, closed=True))
    hw = spec.road_w / 2.0
    # Tramos: viaductos, tuneles y pasos a dos niveles.
    water = np.zeros(len(pts), dtype=bool)
    for t0, t1 in spec.water_spans:
        water |= span_mask(arc, total, to_t(t0), to_t(t1))
    tunnel = np.zeros(len(pts), dtype=bool)
    tunnel_spans = [(to_t(t0), to_t(t1), hh) for t0, t1, hh in spec.tunnels]
    for t0, t1, _ in tunnel_spans:
        tunnel |= span_mask(arc, total, t0, t1)
    if spec.auto_water or spec.auto_tunnel_m > 0.0:
        terrain_at, land_at = _probe_base(spec, canvas, e, n, pts, z, arc, tunnel, base)
        if spec.auto_water:
            water |= _grow(land_at < 2.0, 10) & ~tunnel
        if spec.auto_tunnel_m > 0.0:
            deep = (terrain_at - z >= spec.auto_tunnel_m) & ~water
            deep = _drop_short(deep, 30)
            tunnel |= _grow(deep, 6) & ~water
    upper = np.zeros(len(pts), dtype=bool)
    lower = np.zeros(len(pts), dtype=bool)
    exempt = np.zeros(len(pts), dtype=bool)
    crossings = find_crossings(pts, arc, total)
    for a, b in crossings:
        hi, lo = (a, b) if z[a] > z[b] else (b, a)
        if z[hi] - z[lo] < UNDERPASS_CLEAR_M + DECK_THICKNESS_M + 0.5:
            raise ValueError(f"{spec.name}: cruce a {z[hi] - z[lo]:.1f} m de desnivel; hacen falta "
                             f"{UNDERPASS_CLEAR_M + DECK_THICKNESS_M + 0.5:.1f} m")
        for centre, mask, half in ((hi, upper, OVERPASS_HALF_M), (lo, lower, hw + 10.0), (hi, exempt, 60.0), (lo, exempt, 60.0)):
            gap = np.abs(arc - arc[centre])
            mask |= np.minimum(gap, total - gap) <= half
    decked = water | upper
    ground = ~(decked | tunnel)
    d_ground, kg = along_polyline(e, n, pts[ground])
    k_ground = np.nonzero(ground)[0][kg]
    d_full, k_full = along_polyline(e, n, pts)
    ctx = RoadContext(pts, z, arc, ground, d_ground, k_ground, d_full, k_full)
    terrain, land = (base or default_base(spec))(canvas, e, n, ctx)
    road_z = z[k_ground]
    flat = hw + spec.shoulder_m
    for t0, t1, hill_h in tunnel_spans:
        idx = np.nonzero(span_mask(arc, total, t0, t1))[0]
        terrain = np.maximum(terrain, _hill(ctx, e, n, idx, hill_h, spec.hill_radius_m))
    # Camino primero: el terreno se recorta al prisma de la calzada (desmonte y terraplen a max_talud_deg desde el
    # borde del arcen); lejos de la calzada queda intacto y en las bocas el desmonte termina contra el cerro.
    inside = np.zeros(len(pts))                                  # metros dentro del tunel desde la boca mas cercana
    for idx in _runs(tunnel):
        inside[idx] = np.minimum(np.arange(len(idx)), np.arange(len(idx))[::-1]) + 1.0
    tan_talud = math.tan(math.radians(spec.max_talud_deg))
    d_eff = np.maximum(d_ground, flat + inside[k_full] * PORTAL_TAN / tan_talud)    # fachada de la boca a 60 grados
    reach = np.clip(d_eff - flat, 0.0, None) * tan_talud
    h = np.clip(terrain, road_z - reach, road_z + reach)
    modified = np.abs(h - terrain) > 0.3
    near_axis = d_full < hw + 2.0
    h = np.where(near_axis & decked[k_full], np.minimum(h, z[k_full] - 0.3), h)      # el tablero manda
    h = np.where(near_axis & tunnel[k_full], np.maximum(h, z[k_full]), h)            # suelo macizo del tunel
    reach = spec.land_m + 25.0 if spec.channel_m is None else spec.channel_m
    for idx in (_runs(water) if reach > 0.0 else []):          # estrecho bajo el viaducto
        core = idx[8:-8] if len(idx) > 20 else idx
        d_core, _ = along_polyline(e, n, pts[core])
        land = np.minimum(land, d_core - reach)
    land = np.maximum(land, flat + 4.0 - d_ground)
    if (tunnel | upper).any():                                  # tierra bajo tuneles y pasos elevados
        d_dry, _ = along_polyline(e, n, pts[~water])
        land = np.maximum(land, flat + 4.0 - d_dry)
    height = SEABED_M - 1.5 * smoothstep(-4.0, -30.0, land) + (np.maximum(h, above(0.8)) - SEABED_M) * smoothstep(-3.0, 3.0, land)
    solids, voids, holes = [], [], []
    for mask, name, pillars in ((water, "viaducto", 24.0), (upper, "paso_elevado", 0.0)):
        for idx in _runs(mask):
            axis = Axis.of(pts[idx], z[idx])
            solids.append(Deck(axis, spec.road_w, DECK_THICKNESS_M, pier_m=6.0, pillar_every_m=pillars, name=name))
    for idx in _runs(tunnel):
        ext = _extend(idx, len(pts), 4)
        axis = Axis.of(pts[ext], z[ext])
        voids.append(Tunnel(axis, spec.road_w, 6.0, name="tunel"))
        holes.append(Clearance(Axis.of(pts[idx], z[idx]), 12.0, 5.0, "tunel", min_cover_m=8.0))
    for idx in _runs(lower):
        holes.append(Clearance(Axis.of(pts[idx], z[idx]), spec.road_w, UNDERPASS_CLEAR_M - 0.2, "paso_inferior"))
    trail = 1.0 - smoothstep(hw - 1.0, hw + 0.5, d_full)
    model = KitModel(canvas, height, solids=solids, voids=voids, trail=trail)
    # Sin puntos requeridos a pie: la continuidad de la calzada la comprueba check_road (ancho y pendiente en cada
    # muestra abierta) y la de los tramos cubiertos, check_clearance; la cota superior no ve los tuneles.
    quarter: dict[str, tuple[float, float]] = {}
    road = Road(pts, z, closed=True, covered=tunnel | lower, exempt=exempt)
    params = {"generator": "rally_circuit", **asdict(spec), "lap_m": round(total, 1), "crossings": len(crossings)}
    shape = ShapeMap(spec.name, spec.seed, spec.mode, spec.description, canvas, model, tuple(pts[0]), None, quarter, [],
                     params=params)
    talud = talud_check(spec, canvas, height, d_ground, k_ground, flat, modified, tunnel | decked, land)
    return shape, Extras(road=road, clearances=holes, notes=f"{spec.laps} vueltas = {spec.laps * total:.0f} m",
                         static={"taludes": talud})


def _ctrl_fraction(spec: RallySpec, pts: np.ndarray, arc: np.ndarray, total: float):
    """Fraccion de vuelta de un punto de control (indice real: se interpola entre dos consecutivos)."""
    ctrl = np.asarray(spec.control, dtype=np.float64)
    ks = [int(np.argmin(np.hypot(*(pts - c).T))) for c in ctrl]
    ts = [arc[k] / total for k in ks]
    ts = [t if i == 0 or t > ts[0] else t + 1.0 for i, t in enumerate(ts)] + [ts[0] + 1.0]

    def to_t(v: float) -> float:
        i = int(math.floor(v)) % len(ctrl)
        f = v - math.floor(v)
        return float((ts[i] + (ts[i + 1] - ts[i]) * f) % 1.0) if v < len(ctrl) else 1.0
    return to_t


def _probe_base(spec: RallySpec, canvas: Canvas, e, n, pts, z, arc, tunnel, base):
    """Cota y tierra de `base` bajo cada muestra del eje (antes de tallar la calzada)."""
    ground = ~tunnel
    d_g, kg = along_polyline(e, n, pts[ground])
    d_f, k_f = along_polyline(e, n, pts)
    ctx = RoadContext(pts, z, arc, ground, d_g, np.nonzero(ground)[0][kg], d_f, k_f)
    terrain, land = (base or default_base(spec))(canvas, e, n, ctx)
    ij = np.array([canvas.to_ij(*p) for p in pts])
    px = np.asarray(ij, dtype=np.float64) / canvas.px_m - 0.5
    from scipy import ndimage
    at = [ndimage.map_coordinates(r, [px[:, 0], px[:, 1]], order=1, mode="nearest") for r in (terrain, land)]
    return at[0], at[1]


def _grow(mask: np.ndarray, by: int) -> np.ndarray:
    """Dilata una mascara de un lazo `by` muestras a cada lado."""
    padded = np.concatenate([mask[-by:], mask, mask[:by]]).astype(float)
    return np.convolve(padded, np.ones(2 * by + 1), "same")[by:-by] > 0


def _drop_short(mask: np.ndarray, min_len: int) -> np.ndarray:
    out = np.zeros_like(mask)
    for idx in _runs(mask):
        if len(idx) >= min_len:
            out[idx] = True
    return out


def talud_check(spec: RallySpec, canvas: Canvas, height: np.ndarray, d_ground, k_ground, flat: float,
                modified: np.ndarray, special: np.ndarray, land) -> dict:
    """Pendiente de los taludes (desmonte y terraplen) que talla la calzada: pixeles que la calzada ha recortado,
    fuera del arcen, en tierra y lejos (15 muestras) de tuneles y tableros. Criterio: p99 <= 35 grados."""
    near_special = _grow(special, 15)
    zone = modified & (d_ground > flat + 1.0) & ~near_special[k_ground] & (land > 4.0)
    gy, gx = np.gradient(height, canvas.px_m)
    slope = np.degrees(np.arctan(np.hypot(gx, gy)))[zone]
    if len(slope) == 0:
        return {"p99_deg": 0.0, "max_deg": 0.0, "ok": True, "summary": "sin taludes"}
    p99 = float(np.percentile(slope, 99))
    return {"p99_deg": round(p99, 1), "max_deg": round(float(slope.max()), 1), "ok": p99 <= 35.0,
            "summary": f"taludes p99 {p99:.0f}°"}


def _runs(mask: np.ndarray) -> list[np.ndarray]:
    """Tramos contiguos de un lazo (el que cruza el final se une con el del principio)."""
    idx = np.nonzero(mask)[0]
    if len(idx) == 0:
        return []
    splits = np.nonzero(np.diff(idx) > 1)[0] + 1
    runs = np.split(idx, splits)
    if len(runs) > 1 and runs[0][0] == 0 and runs[-1][-1] == len(mask) - 1:
        runs = [np.concatenate([runs[-1], runs[0]])] + runs[1:-1]
    return runs


def _extend(idx: np.ndarray, count: int, by: int) -> np.ndarray:
    return np.concatenate([np.arange(idx[0] - by, idx[0]) % count, idx, np.arange(idx[-1] + 1, idx[-1] + 1 + by) % count])


def ellipse(a: float, b: float, count: int = 16, angle_deg: float = 0.0, centre=(0.0, 0.0), wobble: float = 0.0,
            seed: int = 0) -> tuple[tuple[float, float], ...]:
    """Puntos de control de un lazo eliptico (con wobble relativo al radio, semilla)."""
    rng = np.random.default_rng(seed)
    out = []
    rot = math.radians(angle_deg)
    for k in range(count):
        t = 2.0 * math.pi * k / count
        f = 1.0 + (float(rng.uniform(-wobble, wobble)) if wobble else 0.0)
        u, v = a * f * math.cos(t), b * f * math.sin(t)
        out.append((centre[0] + u * math.cos(rot) - v * math.sin(rot), centre[1] + u * math.sin(rot) + v * math.cos(rot)))
    return tuple(out)

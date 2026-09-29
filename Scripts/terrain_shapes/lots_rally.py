"""Circuitos de Rally Tortuga (terrain_shapes/rally_circuit.py) del catalogo y de los mapas inventados:

  lote C2: I03-R Tortuga Magna (lazo por el borde del caparazon, tuneles en cabeza y cola), I04 Volcan Hueco
           (flanco, borde de la caldera, tunel al interior, viaducto sobre el lago de lava y tunel de salida) e
           I06 Feroe (parametrica: tres islas alargadas, cuatro viaductos sobre los estrechos y tunel bajo la cresta);
  lote 3:  N09 Cañon serpiente, N10 Fiordo de los puentes, N11 Montaña hueca y N12 Delta de los islotes.

Cada base devuelve (cota del terreno, distancia con signo de la tierra) sobre el raster; la calzada se talla
encima. Todo con semilla (ruido de costa y dunas) y parametros en RallySpec.
"""

from __future__ import annotations

import math
from dataclasses import replace

import numpy as np

from terrain_vol.noise import Fbm2D

from .canvas import sd_circle, sd_ellipse, smoothstep
from .kit import above
from .kit_writer import Extras
from .model import ShapeMap
from .rally_circuit import RallySpec, build_rally, regional_level


def _noise(seed: int, wavelength: float, octaves: int = 3) -> Fbm2D:
    return Fbm2D(np.random.default_rng(seed), wavelength, octaves=octaves)


def _with_markers(built: tuple[ShapeMap, Extras], markers: dict) -> tuple[ShapeMap, Extras]:
    shape, extras = built
    extras.markers.update(markers)
    return shape, extras


def _at(spec: RallySpec, t: float) -> tuple[float, float]:
    """Punto del eje a la fraccion t de vuelta (para marcas)."""
    from .kit import arc_length, resample
    pts = resample(spec.control, 1.0, closed=True)
    arc = arc_length(pts)
    return tuple(float(v) for v in pts[int(np.searchsorted(arc, t * arc[-1])) % len(pts)])


# ── I03-R Tortuga Magna ──────────────────────────────────────────────────────────
def turtle_rally_spec(seed: int = 2103) -> RallySpec:
    ctrl = ((168.0, 0.0), (150.0, 118.0), (95.0, 188.0), (40.0, 238.0), (0.0, 256.0), (-40.0, 238.0), (-95.0, 188.0),
            (-150.0, 118.0), (-168.0, 0.0), (-150.0, -118.0), (-98.0, -182.0), (-42.0, -220.0), (0.0, -236.0),
            (42.0, -220.0), (98.0, -182.0), (150.0, -118.0))
    return RallySpec(
        "I03R_tortuga_magna", seed, ctrl,
        heights=((0.0, 6.0), (2.0, 9.0), (4.0, 6.0), (6.0, 3.0), (8.0, 6.0), (10.0, 9.0), (12.0, 6.0), (14.0, 3.0)),
        tunnels=((3.0, 5.0, 16.0), (11.2, 12.8, 15.0)), by_ctrl=True, land_m=40.0, dune_m=1.5, hill_radius_m=80.0,
        description="Tortuga Magna (Rally): lazo de 1,4 km por el borde del caparazón de una isla-tortuga gigante; "
                    "entra por la boca de la cabeza y sale por la cola en túnel, con el caparazón escalonado en el "
                    "centro (hasta +18 m) y las aletas como cabos; calzada de +3 a +9 m.")


def turtle_rally_base(spec: RallySpec):
    noise, wobble = _noise(spec.seed, 60.0, 2), _noise(spec.seed + 1, 45.0)

    def base(canvas, e, n, ctx):
        level = regional_level(canvas, ctx)
        terrain = level + spec.dune_m * noise(n + 500.0, e + 500.0) * smoothstep(spec.road_w, spec.road_w + 30.0, ctx.d_ground)
        shell = sd_ellipse(e, n, 0.0, 0.0, 140.0, 180.0)
        ring = np.clip(shell / 140.0, 0.0, 1.0)
        steps = np.floor(ring * 5.0) / 5.0                                   # 5 escalones concentricos
        hexes = 0.6 * np.cos(e / 9.0) * np.cos(n / 9.0 + e / 16.0)
        terrain = np.where(shell > 0.0, np.maximum(terrain, above(6.0) + 12.0 * steps + hexes), terrain)
        body = sd_ellipse(e, n, 0.0, 0.0, 215.0, 232.0)
        for sx, sy in ((1, 1), (-1, 1), (1, -1), (-1, -1)):
            body = np.maximum(body, sd_ellipse(e, n, sx * 205.0, sy * 150.0, 80.0, 28.0, sx * sy * 35.0))
        body = np.maximum(body, sd_circle(e, n, 0.0, 262.0, 52.0))
        body = np.maximum(body, sd_ellipse(e, n, 0.0, -258.0, 26.0, 40.0))
        return terrain, body + 6.0 * wobble(n, e)
    return base


def build_turtle_rally(seed: int | None = None) -> tuple[ShapeMap, Extras]:
    spec = turtle_rally_spec(seed or 2103)
    return _with_markers(build_rally(spec, turtle_rally_base(spec)),
                         {"catapulta": [_at(spec, 0.375), _at(spec, 0.875)]})


# ── I04 Volcan Hueco ─────────────────────────────────────────────────────────────
RIM_IN, RIM_OUT, BASE_R, LAVA_R, RIM_H = 128.0, 172.0, 275.0, 112.0, 21.0

# Eje en polares (grados desde el Este, radio, cota sobre el agua): sube por el flanco Este al borde, lo recorre
# por el Norte, baja por el flanco Oeste, entra bajo el borde en tunel, cruza la lava en viaducto y sale bajo el
# borde Sur hasta la playa.
VOLCANO_AXIS = ((-62.0, 262.0, 2.0), (-25.0, 250.0, 4.0), (12.0, 232.0, 8.0), (45.0, 212.0, 12.0), (75.0, 190.0, 16.0),
                (105.0, 170.0, 18.0), (138.0, 175.0, 18.0), (165.0, 200.0, 13.0), (190.0, 222.0, 8.0),
                (212.0, 205.0, 6.0), (222.0, 160.0, 6.0), (228.0, 105.0, 6.0), (262.0, 62.0, 6.0), (300.0, 82.0, 6.0),
                (310.0, 130.0, 6.0), (308.0, 185.0, 5.0), (296.0, 238.0, 3.0))


def volcano_spec(seed: int = 2104) -> RallySpec:
    ctrl = tuple((r * math.cos(math.radians(t)), r * math.sin(math.radians(t))) for t, r, _ in VOLCANO_AXIS)
    return RallySpec(
        "I04_volcan_hueco", seed, ctrl, heights=tuple((float(k), h) for k, (_, _, h) in enumerate(VOLCANO_AXIS)),
        by_ctrl=True, auto_water=True, auto_tunnel_m=14.0, channel_m=0.0, land_m=40.0, dune_m=1.0,
        description="Volcán Hueco (Rally): sube por el flanco hasta el borde de la caldera a +18 m, baja por el otro "
                    "flanco, entra bajo el borde en túnel, cruza el lago de lava (agua de muerte) en viaducto a +6 m y "
                    "sale bajo el borde sur hasta la playa; 1,5 km por vuelta.")


def volcano_base(spec: RallySpec):
    wobble = _noise(spec.seed, 40.0)

    def base(canvas, e, n, ctx):
        r = np.hypot(e, n) + 3.0 * wobble(n, e)
        flank = above(1.0) + (RIM_H - 1.0) * smoothstep(BASE_R, RIM_OUT, r)
        crater = above(-2.0) + (RIM_H + 2.0) * smoothstep(LAVA_R, RIM_IN, r)
        terrain = np.where(r < RIM_OUT, np.minimum(flank, crater), flank)
        land = np.minimum(BASE_R + 18.0 - r, r - LAVA_R)
        return terrain, land
    return base


def build_volcano(seed: int | None = None) -> tuple[ShapeMap, Extras]:
    spec = volcano_spec(seed or 2104)
    return _with_markers(build_rally(spec, volcano_base(spec)), {"trampolin": [_at(spec, 0.3)]})


# ── I06 Feroe (parametrica) ──────────────────────────────────────────────────────
FAROE = (((-178.0, 20.0), (205.0, 60.0)), ((0.0, 0.0), (235.0, 70.0)), ((176.0, -20.0), (205.0, 60.0)))


def faroe_spec(seed: int = 2106) -> RallySpec:
    ctrl = ((-222.0, 0.0), (-205.0, 72.0), (-145.0, 100.0), (-60.0, 106.0), (0.0, 108.0), (60.0, 106.0), (145.0, 100.0),
            (205.0, 72.0), (222.0, 0.0), (205.0, -72.0), (145.0, -100.0), (60.0, -106.0), (0.0, -108.0),
            (-60.0, -106.0), (-145.0, -100.0), (-205.0, -72.0))
    return RallySpec(
        "I06_feroe", seed, ctrl,
        heights=((0.0, 6.0), (2.0, 10.0), (4.0, 8.0), (6.0, 10.0), (8.0, 6.0), (10.0, 10.0), (12.0, 12.0), (14.0, 10.0)),
        tunnels=(), by_ctrl=True, auto_water=True, auto_tunnel_m=14.0, channel_m=0.0, land_m=30.0, dune_m=1.5,
        description="Islas Feroe (Rally, paramétrica: sin MDE): tres islas alargadas con cresta y acantilados al "
                    "oeste; el lazo salta los estrechos en cuatro viaductos y cruza cada isla por su collado, bajo "
                    "las crestas (sin túnel: no hay 8 m de roca encima); 1,2 km por vuelta.")


def faroe_base(spec: RallySpec):
    wobble, crags = _noise(spec.seed, 35.0), _noise(spec.seed + 2, 22.0, 2)

    def base(canvas, e, n, ctx):
        land = np.full(e.shape, -1e3)
        ridge = np.zeros(e.shape)
        for (ce, cn), (a, b) in FAROE:
            sd = sd_ellipse(e, n, ce, cn, a, b, 100.0)
            land = np.maximum(land, sd)
            ridge = np.maximum(ridge, np.clip(sd / b, 0.0, 1.0))
        level = regional_level(canvas, ctx)
        spine = above(3.0) + 17.0 * smoothstep(0.1, 0.75, ridge) + 1.5 * crags(n, e)
        terrain = level + (spine - level) * smoothstep(spec.road_w, spec.road_w + 45.0, ctx.d_ground)
        return terrain, land + 5.0 * wobble(n, e)
    return base


def build_faroe(seed: int | None = None) -> tuple[ShapeMap, Extras]:
    spec = faroe_spec(seed or 2106)
    return build_rally(spec, faroe_base(spec))


MAPS = {
    "I03R": ("C2", build_turtle_rally),
    "I04": ("C2", build_volcano),
    "I06": ("C2", build_faroe),
}
_ = (math, replace)

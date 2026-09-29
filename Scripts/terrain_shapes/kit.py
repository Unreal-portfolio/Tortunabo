"""Piezas comunes de los mapas inventados (lotes N y el resto del catalogo): ejes (polilineas con cota), tuneles
y bovedas (huecos tallados en el volumen), tableros curvos (viaductos y pasos elevados), islas flotantes, y el
modelo KitModel que los suma al campo de alturas de ShapeModel. Todo en coordenadas de diseño (e, n) y cotas
absolutas en m, como terrain_shapes.canvas.

La densidad es positiva dentro de lo solido: KitModel = max(campo de alturas, puentes, solidos) y luego se le
restan los huecos (min con -hueco).
"""

from __future__ import annotations

import math
from dataclasses import dataclass
from functools import cached_property

import numpy as np
from scipy import interpolate
from scipy.spatial import cKDTree

from terrain_vol.layout import DEFAULT_Z_RANGE, WATER_M, ZRange

from .bridges import NaturalBridge
from .canvas import SEABED_M, Canvas, smoothstep
from .model import ShapeModel

ROAD_COLOR = (0.42, 0.30, 0.17)          # arena pisada de calzadas y caminos (trail del color de vertice)


# ── Curvas ───────────────────────────────────────────────────────────────────────
def resample(points, step_m: float = 1.0, closed: bool = False, smooth: bool = True) -> np.ndarray:
    """Curva por los puntos de control (spline cubico si smooth; si no, polilinea) remuestreada cada step_m.
    Cerrada: el ultimo punto no repite el primero."""
    pts = np.asarray(points, dtype=np.float64)
    if closed:
        pts = np.vstack([pts, pts[:1]])
    if smooth and len(pts) >= 4:
        tck, _ = interpolate.splprep([pts[:, 0], pts[:, 1]], s=0.0, per=1 if closed else 0, k=3)
        dense = np.column_stack(interpolate.splev(np.linspace(0.0, 1.0, 40 * len(pts)), tck))
    else:
        dense = pts
    seg = np.hypot(*np.diff(dense, axis=0).T)
    arc = np.concatenate([[0.0], np.cumsum(seg)])
    count = max(2, int(round(arc[-1] / step_m)) + (0 if closed else 1))
    s = np.linspace(0.0, arc[-1], count, endpoint=not closed)
    return np.column_stack([np.interp(s, arc, dense[:, 0]), np.interp(s, arc, dense[:, 1])])


def arc_length(pts: np.ndarray, closed: bool = False) -> np.ndarray:
    """Arco acumulado de cada punto (m)."""
    seg = np.hypot(*np.diff(pts, axis=0).T)
    return np.concatenate([[0.0], np.cumsum(seg)])


def min_radius(pts: np.ndarray, closed: bool, span_m: float = 8.0) -> tuple[float, int]:
    """Menor radio de giro (m) por el circulo que pasa por p[k - s], p[k], p[k + s] (s = span_m de arco) y su
    indice. Una recta da infinito."""
    step = float(np.median(np.hypot(*np.diff(pts, axis=0).T)))
    s = max(1, int(round(span_m / step)))
    idx = np.arange(len(pts))
    if closed:
        a, b, c = pts[(idx - s) % len(pts)], pts, pts[(idx + s) % len(pts)]
    else:
        idx = idx[s:-s]
        a, b, c = pts[idx - s], pts[idx], pts[idx + s]
    ab, bc, ca = np.hypot(*(b - a).T), np.hypot(*(c - b).T), np.hypot(*(a - c).T)
    cross = np.abs((b[:, 0] - a[:, 0]) * (c[:, 1] - a[:, 1]) - (b[:, 1] - a[:, 1]) * (c[:, 0] - a[:, 0]))
    with np.errstate(divide="ignore", invalid="ignore"):
        r = np.where(cross > 1e-9, ab * bc * ca / (2.0 * cross), np.inf)
    k = int(np.argmin(r))
    return float(r[k]), int(idx[k])


def profile(arc: np.ndarray, keys: list[tuple[float, float]], total: float | None = None, closed: bool = False) -> np.ndarray:
    """Cota a lo largo del arco por claves (fraccion del arco, cota absoluta) con transicion suave (smoothstep)
    entre claves consecutivas; cerrada: la ultima clave enlaza con la primera."""
    total = total or float(arc[-1])
    t = arc / total
    ks = sorted(keys)
    if closed:
        ks = [(ks[-1][0] - 1.0, ks[-1][1])] + ks + [(ks[0][0] + 1.0, ks[0][1])]
    out = np.full(t.shape, ks[0][1])
    for (t0, z0), (t1, z1) in zip(ks[:-1], ks[1:]):
        if t1 - t0 < 1e-9:
            continue
        inside = (t >= t0) & (t <= t1)
        out = np.where(inside, z0 + (z1 - z0) * smoothstep(t0, t1, t), out)
    return np.where(t > ks[-1][0], ks[-1][1], out)


def max_grade_deg(arc: np.ndarray, z: np.ndarray, closed: bool = False, window_m: float = 4.0) -> float:
    """Mayor pendiente sostenida del perfil (grados) sobre tramos de window_m."""
    step = float(np.median(np.diff(arc)))
    k = max(1, int(round(window_m / step)))
    if closed:
        dz = np.abs(np.roll(z, -k) - z)
    else:
        dz = np.abs(z[k:] - z[:-k])
    return float(np.degrees(np.arctan(dz.max() / (k * step)))) if len(dz) else 0.0


# ── Ejes, huecos y tableros ──────────────────────────────────────────────────────
@dataclass(frozen=True)
class Axis:
    """Polilinea densa (e, n) con una cota absoluta por punto (suelo de un tunel, tablero de un viaducto)."""
    pts: tuple[tuple[float, float], ...]
    z: tuple[float, ...]

    @classmethod
    def of(cls, pts: np.ndarray, z: np.ndarray) -> "Axis":
        return cls(tuple(map(tuple, np.round(np.asarray(pts, dtype=np.float64), 3))), tuple(np.round(np.asarray(z, dtype=np.float64), 3)))

    @cached_property
    def array(self) -> np.ndarray:
        return np.asarray(self.pts, dtype=np.float64)

    @cached_property
    def zs(self) -> np.ndarray:
        return np.asarray(self.z, dtype=np.float64)

    @cached_property
    def tree(self) -> cKDTree:
        return cKDTree(self.array)

    @property
    def length(self) -> float:
        return float(arc_length(self.array)[-1])

    def nearest(self, e, n) -> tuple[np.ndarray, np.ndarray]:
        d, k = self.tree.query(np.column_stack([np.ravel(e), np.ravel(n)]))
        shape = np.shape(e)
        return d.reshape(shape), k.reshape(shape)

    def bbox(self, margin: float) -> tuple[float, float, float, float]:
        a = self.array
        return a[:, 0].min() - margin, a[:, 0].max() + margin, a[:, 1].min() - margin, a[:, 1].max() + margin


@dataclass(frozen=True)
class Tunnel:
    """Hueco con suelo plano a lo ancho, paredes rectas hasta el 60 % de clearance_m y boveda eliptica."""
    axis: Axis
    width_m: float = 14.0
    clearance_m: float = 6.5
    name: str = "tunel"

    def bbox(self) -> tuple[float, float, float, float]:
        return self.axis.bbox(self.width_m / 2.0 + 2.0)

    def void(self, e, n, Z: np.ndarray) -> np.ndarray:
        d, k = self.axis.nearest(e, n)
        floor = self.axis.zs[k]
        hw = self.width_m / 2.0
        wall = 0.6 * self.clearance_m
        arch = np.sqrt(np.clip(1.0 - (d / hw) ** 2, 0.0, 1.0))
        ceiling = floor + wall + (self.clearance_m - wall) * arch
        side = np.minimum(hw - d, self._cap(e, n, k))
        Z3 = Z[None, None, :]
        return np.minimum(np.minimum(side[..., None], ceiling[..., None] - Z3), Z3 - floor[..., None])

    def _cap(self, e, n, k) -> np.ndarray:
        """Distancia (positiva dentro) a los planos de las bocas: el hueco termina en seco en cada extremo."""
        a = self.axis.array
        out = np.full(np.shape(e), 1e3)
        for end, inner in ((0, 1), (len(a) - 1, len(a) - 2)):
            u = a[inner] - a[end]
            u = u / max(float(np.hypot(*u)), 1e-9)
            along = (e - a[end][0]) * u[0] + (n - a[end][1]) * u[1]
            out = np.where(k == end, np.minimum(out, along), out)
        return out

    def manifest(self, canvas: Canvas) -> dict:
        a = self.axis.array
        return {"name": self.name, "a_uu": _uu(canvas, a[0], self.axis.zs[0]), "b_uu": _uu(canvas, a[-1], self.axis.zs[-1]),
                "length_m": round(self.axis.length, 1), "width_m": self.width_m, "clearance_m": self.clearance_m}


@dataclass(frozen=True)
class Deck:
    """Tablero curvo (viaducto, paso elevado, pasarela de roca): cara de arriba en la cota del eje, grosor
    thickness_m y estribos que bajan al fondo en los pier_m primeros y ultimos metros; pillar_every_m > 0 pone
    pilas de 3 m de ancho bajo el eje cada tanto (0 = arco libre)."""
    axis: Axis
    width_m: float = 14.0
    thickness_m: float = 1.6
    pier_m: float = 5.0
    pillar_every_m: float = 0.0
    name: str = "tablero"

    def bbox(self) -> tuple[float, float, float, float]:
        return self.axis.bbox(self.width_m / 2.0 + 2.0)

    @cached_property
    def arc(self) -> np.ndarray:
        return arc_length(self.axis.array)

    def density(self, e, n, Z: np.ndarray) -> np.ndarray:
        d, k = self.axis.nearest(e, n)
        top = self.axis.zs[k]
        s = self.arc[k]
        end = np.minimum(s, self.arc[-1] - s)
        pier = 1.0 - smoothstep(0.0, self.pier_m, end)
        if self.pillar_every_m > 0.0:
            phase = np.abs(np.mod(s + 0.5 * self.pillar_every_m, self.pillar_every_m) - 0.5 * self.pillar_every_m)
            pier = np.maximum(pier, ((phase < 1.5) & (d < 1.5)).astype(np.float64))
        bottom = top - self.thickness_m - (top - SEABED_M + 1.0) * pier ** 1.5
        Z3 = Z[None, None, :]
        side = (self.width_m / 2.0 - d)[..., None]
        return np.minimum(np.minimum(side, top[..., None] - Z3), Z3 - bottom[..., None])

    def manifest(self, canvas: Canvas) -> dict:
        a = self.axis.array
        return {"name": self.name, "a_uu": _uu(canvas, a[0], self.axis.zs[0]), "b_uu": _uu(canvas, a[-1], self.axis.zs[-1]),
                "length_m": round(self.axis.length, 1), "width_m": self.width_m}


@dataclass(frozen=True)
class Floating:
    """Isla flotante: meseta eliptica de cara plana a z_top (borde redondeado) y panza en cono invertido de
    depth_m bajo la cara; aire debajo."""
    center: tuple[float, float]
    radii: tuple[float, float]
    z_top: float
    depth_m: float = 8.0
    angle_deg: float = 0.0
    name: str = "flotante"

    def bbox(self) -> tuple[float, float, float, float]:
        r = max(self.radii) + 2.0
        return self.center[0] - r, self.center[0] + r, self.center[1] - r, self.center[1] + r

    def sd(self, e, n) -> np.ndarray:
        a = math.radians(self.angle_deg)
        de, dn = e - self.center[0], n - self.center[1]
        u, v = de * math.cos(a) + dn * math.sin(a), -de * math.sin(a) + dn * math.cos(a)
        return (1.0 - np.hypot(u / self.radii[0], v / self.radii[1])) * min(self.radii)

    def density(self, e, n, Z: np.ndarray) -> np.ndarray:
        sd = self.sd(e, n)
        top = self.z_top - 1.2 * (1.0 - smoothstep(0.0, 2.0, sd))
        depth = np.clip(sd / min(self.radii), 0.0, 1.0) ** 0.6
        bottom = self.z_top - 1.0 - self.depth_m * depth
        Z3 = Z[None, None, :]
        return np.minimum(np.minimum(sd[..., None], top[..., None] - Z3), Z3 - bottom[..., None])


def _uu(canvas: Canvas, p, z: float) -> list[float]:
    X, Y = canvas.to_world(float(p[0]), float(p[1]))
    return [round(X * 100.0, 1), round(Y * 100.0, 1), round(float(z) * 100.0, 1)]


def _overlaps(box, e_lo, e_hi, n_lo, n_hi) -> bool:
    return not (box[1] < e_lo or box[0] > e_hi or box[3] < n_lo or box[2] > n_hi)


# ── Modelo ───────────────────────────────────────────────────────────────────────
class KitModel(ShapeModel):
    """ShapeModel con solidos 3D (tableros, islas flotantes), huecos (tuneles, cuevas) y una mascara de camino
    (raster 0..1 del tamaño de height) que oscurece la arena pisada en el color de vertice."""
    trail_color = ROAD_COLOR
    trail_strength = 0.55

    def __init__(self, canvas: Canvas, height: np.ndarray, bridges: tuple[NaturalBridge, ...] = (), solids=(), voids=(),
                 trail: np.ndarray | None = None, z_range: ZRange = DEFAULT_Z_RANGE):
        super().__init__(canvas, height, bridges, z_range)
        self.solids, self.voids = tuple(solids), tuple(voids)
        self.trail = trail

    def trail_mask(self, x: np.ndarray, y: np.ndarray) -> np.ndarray:
        if self.trail is None:
            return np.zeros(len(x))
        return np.clip(self._sample(self.trail, x, y), 0.0, 1.0)

    def density(self, X: np.ndarray, Y: np.ndarray, Z: np.ndarray, fields=None) -> np.ndarray:
        out = super().density(X, Y, Z, fields)
        if not self.solids and not self.voids:
            return out
        e, n = self.canvas.to_design(X, Y)
        box = (float(e.min()), float(e.max()), float(n.min()), float(n.max()))
        for solid in self.solids:
            if _overlaps(solid.bbox(), *box):
                out = np.maximum(out, solid.density(e, n, Z))
        for hole in self.voids:
            if _overlaps(hole.bbox(), *box):
                out = np.minimum(out, -hole.void(e, n, Z))
        return out


# ── Campo de alturas: utilidades ─────────────────────────────────────────────────
def along_polyline(e, n, pts: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """(distancia, indice del punto mas cercano) de cada pixel a una polilinea densa."""
    d, k = cKDTree(pts).query(np.column_stack([np.ravel(e), np.ravel(n)]))
    return d.reshape(np.shape(e)), k.reshape(np.shape(e))


def ramp(height: np.ndarray, e, n, a, b, z_a: float, z_b: float, width_m: float, blend_m: float = 1.0) -> np.ndarray:
    """Rampa recta de a (cota z_a) a b (z_b) de width_m de ancho pintada sobre el campo (sustituye la cota)."""
    ae, an = a
    de, dn = b[0] - ae, b[1] - an
    length = math.hypot(de, dn)
    ue, un = de / length, dn / length
    along = (e - ae) * ue + (n - an) * un
    lateral = np.abs(-(e - ae) * un + (n - an) * ue)
    t = np.clip(along / length, 0.0, 1.0)
    z = z_a + (z_b - z_a) * t
    inside = (along >= -blend_m) & (along <= length + blend_m)
    w = (1.0 - smoothstep(width_m / 2.0, width_m / 2.0 + blend_m, lateral)) * inside
    return height + (z - height) * w


def ramp_slope_deg(z_a: float, z_b: float, a, b) -> float:
    return math.degrees(math.atan(abs(z_b - z_a) / max(math.hypot(b[0] - a[0], b[1] - a[1]), 1e-6)))


def sea_floor(e, n, land_sd: np.ndarray) -> np.ndarray:
    """Fondo del mar: SEABED_M junto a la costa y 1,5 m mas hondo lejos (land_sd < 0 fuera de tierra)."""
    return SEABED_M - 1.5 * smoothstep(-4.0, -30.0, land_sd)


def above(h: float) -> float:
    """Cota absoluta de una cota sobre el agua."""
    return WATER_M + h

"""GeoRegion: una region del mundo (caja lon/lat o pais ISO) llevada al volumen del juego.

    region = GeoRegion("JP_fuji", bbox=(138.25, 34.62, 139.75, 35.85), edge="coast")
    proj = region.game_projection()          # proyeccion elegida y zoom automatico
    X, Y = proj.to_game(138.7274, 35.3606)   # (Norte, Este) en m de juego
    dem = region.fetch_dem()                 # raster de elevacion real (m) de todo el volumen
    coverage = region.coverage(dem)          # 0..1: tierra del mapa (costa, frontera o ambas)

Proyeccion ("auto"): LAEA de Europa (EPSG:3035) si la region cae en Europa; equirectangular si abarca mas de
LARGE_EXTENT_DEG grados (Mundo, acotado a +-72 de latitud); LAEA centrada en la region en el resto. "merc" es Web
Mercator analitico (el de E01, sin pyproj). Zoom automatico: la region (su caja proyectada) llena el volumen menos
`frame_m` de mar en cada borde. El volumen es el de terrain_vol: `grid` x `grid` trozos de 100 m, X = Norte,
Y = Este, de MAP_MIN_M a MAP_MIN_M + grid * 100.
"""

from __future__ import annotations

import math
import zlib
from dataclasses import dataclass, field
from functools import cached_property
from typing import Literal

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage

from terrain_vol.layout import CELL_M, GRID, MAP_MIN_M
from terrain_vol.noise import Fbm2D

from . import sources

EdgeMode = Literal["coast", "border", "both", "none"]
ProjectionKind = Literal["auto", "laea", "laea_europe", "eqc", "merc"]
EDGE_MODES = ("coast", "border", "both", "none")
EDGE_NAMES = {"coast": "costa", "border": "frontera", "both": "costa y frontera", "none": "el marco del volumen"}
PROJECTIONS = ("auto", "laea", "laea_europe", "eqc", "merc")
EUROPE_BOX = (-25.0, 34.0, 45.0, 72.0)          # lon_min, lat_min, lon_max, lat_max
LARGE_EXTENT_DEG = 40.0
WORLD_LAT_LIMIT = 72.0
RASTER_PX_M = 0.5
SUPERSAMPLE = 4
FRAME_WOBBLE = 0.45


def raster_axis(grid: int = GRID, px_m: float = RASTER_PX_M) -> np.ndarray:
    """Coordenada de juego (Norte o Este) del centro de cada pixel de un raster del volumen."""
    n = int(round(grid * CELL_M / px_m))
    return MAP_MIN_M + (np.arange(n) + 0.5) * px_m


def choose_projection(bounds: tuple[float, float, float, float]) -> str:
    lon_min, lat_min, lon_max, lat_max = bounds
    e_lon0, e_lat0, e_lon1, e_lat1 = EUROPE_BOX
    if e_lon0 <= lon_min and lon_max <= e_lon1 and e_lat0 <= lat_min and lat_max <= e_lat1:
        return "laea_europe"
    if lon_max - lon_min > LARGE_EXTENT_DEG or lat_max - lat_min > LARGE_EXTENT_DEG:
        return "eqc"
    return "laea"


@dataclass(frozen=True)
class GameProjection:
    """lon/lat <-> m proyectados <-> m de juego (X Norte, Y Este). scale = m proyectados por m de juego."""
    kind: str
    center: tuple[float, float]                  # (lon, lat) que cae en el centro del volumen
    scale: float
    grid: int = GRID
    proj4: str = ""
    center_xy: tuple[float, float] = field(default=(0.0, 0.0))   # centro en m proyectados

    @cached_property
    def _pyproj(self):
        from pyproj import CRS, Transformer        # solo para laea/eqc: E01 (merc) no necesita pyproj
        crs = CRS.from_proj4(self.proj4)
        return (Transformer.from_crs("EPSG:4326", crs, always_xy=True),
                Transformer.from_crs(crs, "EPSG:4326", always_xy=True))

    def project(self, lon, lat):
        if self.kind == "merc":
            return sources.lonlat_to_mercator(lon, lat)
        x, y = self._pyproj[0].transform(np.asarray(lon, np.float64), np.asarray(lat, np.float64))
        return np.asarray(x), np.asarray(y)

    def unproject(self, x, y):
        if self.kind == "merc":
            return sources.mercator_to_lonlat(x, y)
        lon, lat = self._pyproj[1].transform(np.asarray(x, np.float64), np.asarray(y, np.float64))
        return np.asarray(lon), np.asarray(lat)

    @property
    def mid(self) -> float:
        return MAP_MIN_M + self.grid * CELL_M / 2.0

    def to_game(self, lon, lat):
        """(X Norte, Y Este) en m de juego."""
        x, y = self.project(lon, lat)
        return self.mid + (y - self.center_xy[1]) / self.scale, self.mid + (x - self.center_xy[0]) / self.scale

    def to_lonlat(self, X, Y):
        return self.unproject(self.center_xy[0] + (np.asarray(Y) - self.mid) * self.scale,
                              self.center_xy[1] + (np.asarray(X) - self.mid) * self.scale)

    def ground_m_per_game_m(self) -> float:
        """Metros de suelo real por metro de juego en el centro (Mercator estira por 1/cos(lat))."""
        if self.kind == "merc":
            return self.scale * math.cos(math.radians(self.center[1]))
        return self.scale


def _proj4(kind: str, center: tuple[float, float]) -> str:
    lon, lat = center
    if kind == "laea_europe":
        return "+proj=laea +lat_0=52 +lon_0=10 +x_0=0 +y_0=0 +ellps=GRS80 +units=m +no_defs"
    if kind == "laea":
        return f"+proj=laea +lat_0={lat:.6f} +lon_0={lon:.6f} +x_0=0 +y_0=0 +ellps=WGS84 +units=m +no_defs"
    if kind == "eqc":
        lat_ts = max(-WORLD_LAT_LIMIT, min(WORLD_LAT_LIMIT, lat))
        return f"+proj=eqc +lat_ts={lat_ts:.6f} +lon_0={lon:.6f} +x_0=0 +y_0=0 +ellps=WGS84 +units=m +no_defs"
    return ""


@dataclass(frozen=True)
class GeoRegion:
    """Region geografica del mapa. bbox = (lon_min, lat_min, lon_max, lat_max); country = ISO 3166 alfa-3 o alfa-2.
    Con los dos, la caja recorta el pais (España sin Canarias). edge: "coast" (la tierra es lo que queda sobre
    water_level_m), "border" (la frontera del pais es la costa), "both" (interseccion) o "none" (todo tierra
    hasta el marco). frame_m: franja de mar en el borde del volumen, para que nada quede cortado ahi."""
    name: str
    bbox: tuple[float, float, float, float] | None = None
    country: str | None = None
    edge: EdgeMode = "coast"
    projection: ProjectionKind = "auto"
    grid: int = GRID
    frame_m: float = 20.0
    center: tuple[float, float] | None = None
    scale: float | None = None
    water_level_m: float = 0.0
    lakes: bool = False
    min_island_m2: float = 0.0
    min_lake_m2: float = 40.0
    keep_points: tuple[tuple[float, float], ...] = ()
    border_scale: str = "50m"

    def __post_init__(self) -> None:
        if self.bbox is None and self.country is None:
            raise ValueError(f"{self.name}: hace falta bbox o country")
        if self.edge not in EDGE_MODES:
            raise ValueError(f"{self.name}: edge {self.edge!r} no es uno de {EDGE_MODES}")
        if self.projection not in PROJECTIONS:
            raise ValueError(f"{self.name}: proyeccion {self.projection!r} no es una de {PROJECTIONS}")
        if self.edge in ("border", "both") and self.country is None:
            raise ValueError(f"{self.name}: edge={self.edge!r} necesita country")
        if self.bbox is not None:
            lon_min, lat_min, lon_max, lat_max = self.bbox
            if not (lon_min < lon_max and lat_min < lat_max and -90.0 <= lat_min and lat_max <= 90.0):
                raise ValueError(f"{self.name}: bbox {self.bbox} invalida")
        if self.grid < 1 or self.frame_m < 0.0 or 2.0 * self.frame_m >= self.grid * CELL_M:
            raise ValueError(f"{self.name}: grid {self.grid} / frame_m {self.frame_m} invalidos")

    # ── Geometria ────────────────────────────────────────────────────────────────
    @cached_property
    def country_polygons(self) -> list:
        return sources.country_polygons(self.country, self.border_scale) if self.country else []

    def bounds(self) -> tuple[float, float, float, float]:
        if self.bbox is not None:
            return self.bbox
        rings = np.concatenate([poly[0] for poly in self.country_polygons])
        return (float(rings[:, 0].min()), float(rings[:, 1].min()), float(rings[:, 0].max()), float(rings[:, 1].max()))

    def projection_kind(self) -> str:
        return choose_projection(self.bounds()) if self.projection == "auto" else self.projection

    @cached_property
    def _projection(self) -> GameProjection:
        lon_min, lat_min, lon_max, lat_max = self.bounds()
        if self.projection_kind() == "eqc":
            lat_min, lat_max = max(lat_min, -WORLD_LAT_LIMIT), min(lat_max, WORLD_LAT_LIMIT)
        center = self.center or (0.5 * (lon_min + lon_max), 0.5 * (lat_min + lat_max))
        kind = self.projection_kind()
        probe = GameProjection(kind, center, 1.0, self.grid, _proj4(kind, center))
        t = np.linspace(0.0, 1.0, 65)
        lon = np.concatenate([lon_min + (lon_max - lon_min) * t, np.full(65, lon_max), lon_max - (lon_max - lon_min) * t,
                              np.full(65, lon_min)])
        lat = np.concatenate([np.full(65, lat_min), lat_min + (lat_max - lat_min) * t, np.full(65, lat_max),
                              lat_max - (lat_max - lat_min) * t])
        x, y = probe.project(lon, lat)
        if self.center is not None:
            cx, cy = (float(v) for v in probe.project(*center))
        else:
            cx, cy = 0.5 * (float(x.min()) + float(x.max())), 0.5 * (float(y.min()) + float(y.max()))
        usable = self.grid * CELL_M - 2.0 * self.frame_m
        scale = self.scale or max(float(x.max() - x.min()), float(y.max() - y.min())) / usable
        return GameProjection(kind, center, scale, self.grid, probe.proj4, (cx, cy))

    def game_projection(self) -> GameProjection:
        return self._projection

    def raster_axis(self, px_m: float = RASTER_PX_M) -> np.ndarray:
        return raster_axis(self.grid, px_m)

    def lonlat_grid(self, px_m: float = RASTER_PX_M) -> tuple[np.ndarray, np.ndarray]:
        """lon, lat de cada pixel del raster del volumen, indexados [Norte, Este]."""
        axis = self.raster_axis(px_m)
        return self._projection.to_lonlat(axis[:, None] + 0.0 * axis[None, :], axis[None, :] + 0.0 * axis[:, None])

    # ── Rasters ──────────────────────────────────────────────────────────────────
    def dem_zoom(self, px_m: float = RASTER_PX_M) -> int:
        lon, lat = self.lonlat_grid(px_m)
        bounds = (float(lon.min()), float(lat.min()), float(lon.max()), float(lat.max()))
        return sources.pick_zoom(bounds, self._projection.ground_m_per_game_m() * px_m)

    def fetch_dem(self, px_m: float = RASTER_PX_M) -> np.ndarray:
        """Elevacion real (m, con batimetria) de cada pixel del volumen, [Norte, Este]. Descarga (con cache)."""
        lon, lat = self.lonlat_grid(px_m)
        return sources.sample_elevation(lon, lat, self.dem_zoom(px_m))

    def rasterize(self, polygons: list, px_m: float = RASTER_PX_M) -> np.ndarray:
        """Cobertura 0..1 de los poligonos (lon, lat) sobre el raster del volumen, [Norte, Este]."""
        n = len(self.raster_axis(px_m))
        size = n * SUPERSAMPLE
        image = Image.new("L", (size, size), 0)
        draw = ImageDraw.Draw(image)
        scale = SUPERSAMPLE / px_m
        top = MAP_MIN_M + n * px_m
        for polygon in polygons:
            for k, ring in enumerate(polygon):
                X, Y = self._projection.to_game(ring[:, 0], ring[:, 1])
                if X.max() < MAP_MIN_M or X.min() > top or Y.max() < MAP_MIN_M or Y.min() > top:
                    continue
                points = list(zip(((Y - MAP_MIN_M) * scale).tolist(), ((top - X) * scale).tolist()))
                draw.polygon(points, fill=255 if k == 0 else 0)
        coverage = np.asarray(image, dtype=np.float64).reshape(n, SUPERSAMPLE, n, SUPERSAMPLE).mean(axis=(1, 3)) / 255.0
        return coverage[::-1]

    def frame_mask(self, px_m: float = RASTER_PX_M, organic: bool = True) -> np.ndarray:
        """Distancia (m) al marco de mar, positiva dentro: un rectangulo de esquinas redondeadas a frame_m del borde
        del volumen y, si organic, ondulado (±FRAME_WOBBLE * frame_m) para que el corte no sea una recta."""
        axis = self.raster_axis(px_m) - MAP_MIN_M
        size = self.grid * CELL_M
        half = size / 2.0 - self.frame_m
        radius = min(2.0 * self.frame_m, half)
        q = np.abs(axis - size / 2.0) - (half - radius)
        qx, qy = q[:, None], q[None, :]
        outside = np.hypot(np.maximum(qx, 0.0), np.maximum(qy, 0.0)) + np.minimum(np.maximum(qx, qy), 0.0) - radius
        d = -outside
        if organic and self.frame_m > 0.0:
            rng = np.random.default_rng(zlib.crc32(self.name.encode("utf-8")))
            wobble = Fbm2D(rng, 45.0, octaves=2)
            X = MAP_MIN_M + axis
            d = d + FRAME_WOBBLE * self.frame_m * wobble(X[:, None] + 0.0 * X[None, :], X[None, :] + 0.0 * X[:, None])
        return d

    def coverage(self, dem: np.ndarray, px_m: float = RASTER_PX_M) -> np.ndarray:
        """Tierra del mapa (0..1) segun `edge`, sin islas menores que min_island_m2 (ni las que no contienen
        keep_points, si hay) y con el marco de mar."""
        land = np.ones(dem.shape, dtype=bool)
        if self.edge in ("coast", "both"):
            land &= ndimage.gaussian_filter(dem, 1.0) > self.water_level_m
            if self.lakes:
                land &= self.rasterize(sources.lake_polygons(self.bounds()), px_m) < 0.5
        if self.edge in ("border", "both"):
            land &= self.rasterize(self.country_polygons, px_m) >= 0.5
        land &= self.frame_mask(px_m) > 0.0
        land = self.fill_small_lakes(self.filter_islands(land, px_m), px_m)
        return ndimage.gaussian_filter(land.astype(np.float64), 1.0)

    def fill_small_lakes(self, land: np.ndarray, px_m: float = RASTER_PX_M) -> np.ndarray:
        """Charcas interiores (agua que no toca el borde del volumen) de menos de min_lake_m2: tierra."""
        water, count = ndimage.label(~land)
        if count == 0 or self.min_lake_m2 <= 0.0:
            return land
        areas = ndimage.sum(~land, water, index=np.arange(1, count + 1)) * px_m ** 2
        edge = np.unique(np.concatenate([water[0], water[-1], water[:, 0], water[:, -1]]))
        fill = np.zeros(count + 1, dtype=bool)
        fill[1:] = areas < self.min_lake_m2
        fill[edge] = False
        return land | fill[water]

    def filter_islands(self, land: np.ndarray, px_m: float = RASTER_PX_M) -> np.ndarray:
        labels, count = ndimage.label(land)
        if count == 0:
            return land
        areas = ndimage.sum(land, labels, index=np.arange(1, count + 1)) * px_m ** 2
        keep = np.zeros(count + 1, dtype=bool)
        keep[1:] = areas >= self.min_island_m2
        if self.keep_points:
            wanted = np.zeros(count + 1, dtype=bool)
            axis = self.raster_axis(px_m)
            for lon, lat in self.keep_points:
                X, Y = self._projection.to_game(lon, lat)
                i = int(np.clip(np.searchsorted(axis, float(X)), 0, len(axis) - 1))
                j = int(np.clip(np.searchsorted(axis, float(Y)), 0, len(axis) - 1))
                wanted[labels[i, j]] = labels[i, j] > 0
            keep &= wanted
        return keep[labels]

    def credits(self, extra: tuple[str, ...] = ()) -> str:
        proj = self._projection
        lines = [f"Mapa {self.name}: región {self.bounds()} (lon_min, lat_min, lon_max, lat_max)"
                 + (f", país {self.country}" if self.country else "") + f", borde por {EDGE_NAMES[self.edge]}.",
                 f"Proyección {proj.kind} ({proj.proj4 or 'Web Mercator esférico, EPSG:3857'}); "
                 f"1 m de juego = {proj.ground_m_per_game_m():.0f} m de suelo real en el centro.",
                 "", sources.CREDITS["terrain_tiles"]]
        if self.edge in ("border", "both") or self.lakes:
            lines.append(sources.CREDITS["natural_earth"])
        return "\n".join(lines + list(extra)) + "\n"

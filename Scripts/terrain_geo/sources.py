"""Fuentes de datos geograficos libres, con cache en disco (Saved/terrain_geo_cache/, no se versiona).

  - Relieve y batimetria: teselas "terrarium" de Mapzen/AWS Terrain Tiles (SRTM, GMTED, ETOPO1, ...).
  - Fronteras, tierra y lagos: Natural Earth (dominio publico).

Es la misma fuente y la misma cache que usa terrain_geo/fetch_spain.py para E01. Nada de aqui se llama desde los
tests: los tests dan los rasters ya hechos.
"""

from __future__ import annotations

import io
import json
import math
import urllib.request
from pathlib import Path

import numpy as np
from PIL import Image
from scipy import ndimage

EARTH_R_M = 6378137.0
WORLD_M = 2.0 * math.pi * EARTH_R_M
TILE_URL = "https://s3.amazonaws.com/elevation-tiles-prod/terrarium/{z}/{x}/{y}.png"
NE_URL = "https://raw.githubusercontent.com/nvkelso/natural-earth-vector/master/geojson/ne_{scale}_{layer}.geojson"
CACHE = Path(__file__).resolve().parents[2] / "Saved" / "terrain_geo_cache"
MAX_ZOOM = 14                    # terrarium llega a 15; 14 ya es ~10 m por pixel en el ecuador
MAX_TILES = 324                  # mosaico maximo (18 x 18 teselas); si no cabe, se baja el zoom

CREDITS = {
    "terrain_tiles": (
        "Relieve y batimetría: Terrain Tiles (Mapzen / Amazon Web Services Open Data), formato terrarium. "
        "https://registry.opendata.aws/terrain-tiles/ . Incluye datos de SRTM y GMTED2010 (U.S. Geological "
        "Survey, dominio público), ETOPO1 (NOAA National Centers for Environmental Information, dominio público), "
        "EU-DEM (producido con fondos de la Unión Europea, Copernicus), ArcticDEM (Polar Geospatial Center, NSF), "
        "NRCan CDEM, LINZ (CC BY 4.0) y otros. Atribución completa: "
        "https://github.com/tilezen/joerd/blob/master/docs/attribution.md"),
    "natural_earth": "Fronteras, costas y lagos: Natural Earth (dominio público). https://www.naturalearthdata.com/",
}


def download(url: str, target: Path) -> bytes:
    """Contenido de url, cacheado en target (la primera vez se descarga)."""
    if target.exists():
        return target.read_bytes()
    target.parent.mkdir(parents=True, exist_ok=True)
    with urllib.request.urlopen(url, timeout=120) as response:
        data = response.read()
    target.write_bytes(data)
    return data


# ── Web Mercator (la proyeccion de las teselas) ───────────────────────────────────
def lonlat_to_mercator(lon, lat):
    lon, lat = np.asarray(lon, dtype=np.float64), np.asarray(lat, dtype=np.float64)
    return EARTH_R_M * np.radians(lon), EARTH_R_M * np.log(np.tan(np.pi / 4.0 + np.radians(lat) / 2.0))


def mercator_to_lonlat(mx, my):
    mx, my = np.asarray(mx, dtype=np.float64), np.asarray(my, dtype=np.float64)
    return np.degrees(mx / EARTH_R_M), np.degrees(2.0 * np.arctan(np.exp(my / EARTH_R_M)) - np.pi / 2.0)


def tile_ground_px_m(zoom: int, lat: float) -> float:
    """Metros de suelo por pixel de una tesela de zoom `zoom` a la latitud lat."""
    return WORLD_M / 2 ** zoom / 256.0 * math.cos(math.radians(lat))


def tile_range(lon_min: float, lat_min: float, lon_max: float, lat_max: float, zoom: int) -> tuple[range, range]:
    tile_m = WORLD_M / 2 ** zoom
    (mx0, mx1), (my0, my1) = lonlat_to_mercator([lon_min, lon_max], [lat_min, lat_max])
    tx0, tx1 = (mx0 + WORLD_M / 2.0) / tile_m, (mx1 + WORLD_M / 2.0) / tile_m
    ty0, ty1 = (WORLD_M / 2.0 - my1) / tile_m, (WORLD_M / 2.0 - my0) / tile_m
    return range(int(math.floor(tx0)), int(math.floor(tx1)) + 1), range(int(math.floor(ty0)), int(math.floor(ty1)) + 1)


def pick_zoom(bounds: tuple[float, float, float, float], ground_px_m: float) -> int:
    """Zoom mas bajo cuyo pixel no es mas grueso que ground_px_m (el pixel de los rasters del juego, en suelo
    real), acotado a MAX_ZOOM y a MAX_TILES teselas."""
    lon_min, lat_min, lon_max, lat_max = bounds
    lat = 0.5 * (lat_min + lat_max)
    zoom = next((z for z in range(0, MAX_ZOOM + 1) if tile_ground_px_m(z, lat) <= ground_px_m), MAX_ZOOM)
    while zoom > 0:
        xs, ys = tile_range(lon_min, lat_min, lon_max, lat_max, zoom)
        if len(xs) * len(ys) <= MAX_TILES:
            break
        zoom -= 1
    return zoom


def fetch_mosaic(bounds: tuple[float, float, float, float], zoom: int) -> tuple[np.ndarray, float, float, float]:
    """(elevacion en m, mx del borde izquierdo, my del borde superior, m Mercator por pixel) del mosaico que cubre
    bounds = (lon_min, lat_min, lon_max, lat_max)."""
    xs, ys = tile_range(*bounds, zoom)
    mosaic = np.zeros((len(ys) * 256, len(xs) * 256))
    for j, ty in enumerate(ys):
        for i, tx in enumerate(xs):
            raw = download(TILE_URL.format(z=zoom, x=tx % 2 ** zoom, y=ty), CACHE / f"{zoom}_{tx % 2 ** zoom}_{ty}.png")
            rgb = np.asarray(Image.open(io.BytesIO(raw)).convert("RGB")).astype(np.float64)
            mosaic[j * 256:(j + 1) * 256, i * 256:(i + 1) * 256] = rgb[..., 0] * 256.0 + rgb[..., 1] + rgb[..., 2] / 256.0 - 32768.0
    tile_m = WORLD_M / 2 ** zoom
    return mosaic, xs[0] * tile_m - WORLD_M / 2.0, WORLD_M / 2.0 - ys[0] * tile_m, tile_m / 256.0


def sample_elevation(lon: np.ndarray, lat: np.ndarray, zoom: int) -> np.ndarray:
    """Elevacion (m, bilineal) en cada (lon, lat) del array; descarga el mosaico que los cubre."""
    bounds = (float(lon.min()), float(lat.min()), float(lon.max()), float(lat.max()))
    mosaic, left, top, px_m = fetch_mosaic(bounds, zoom)
    mx, my = lonlat_to_mercator(lon, lat)
    return ndimage.map_coordinates(mosaic, [(top - my) / px_m - 0.5, (mx - left) / px_m - 0.5], order=1, mode="nearest")


# ── Natural Earth ─────────────────────────────────────────────────────────────────
Polygon = list[np.ndarray]           # [anillo exterior, huecos...], cada uno (n, 2) con (lon, lat)


def natural_earth(layer: str, scale: str = "50m") -> dict:
    name = f"ne_{scale}_{layer}.geojson"
    return json.loads(download(NE_URL.format(scale=scale, layer=layer), CACHE / name))


def _polygons(geometry: dict) -> list[Polygon]:
    parts = geometry["coordinates"] if geometry["type"] == "MultiPolygon" else [geometry["coordinates"]]
    return [[np.asarray(ring, dtype=np.float64)[:, :2] for ring in part] for part in parts]


def matches_country(props: dict, code: str) -> bool:
    """code: ISO 3166-1 alfa-3 (ESP) o alfa-2 (ES), o el nombre en ingles de Natural Earth."""
    code = code.upper()
    keys = ("ISO_A3", "ADM0_A3", "ISO_A3_EH", "ISO_A2", "ISO_A2_EH", "ADMIN", "NAME")
    return any(str(props.get(k, "")).upper() == code for k in keys)


def country_polygons(code: str, scale: str = "50m") -> list[Polygon]:
    out: list[Polygon] = []
    for feature in natural_earth("admin_0_countries", scale)["features"]:
        if matches_country(feature["properties"], code):
            out += _polygons(feature["geometry"])
    if not out:
        raise KeyError(f"pais {code!r} no esta en Natural Earth {scale}")
    return out


def lake_polygons(bounds: tuple[float, float, float, float], scale: str = "10m") -> list[Polygon]:
    lon_min, lat_min, lon_max, lat_max = bounds
    out = []
    for feature in natural_earth("lakes", scale)["features"]:
        for polygon in _polygons(feature["geometry"]):
            ring = polygon[0]
            if ring[:, 0].max() >= lon_min and ring[:, 0].min() <= lon_max and ring[:, 1].max() >= lat_min \
                    and ring[:, 1].min() <= lat_max:
                out.append(polygon)
    return out

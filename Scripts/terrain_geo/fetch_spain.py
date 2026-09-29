"""Descarga el relieve y la frontera de España y los deja como rasters de datos del juego:
Scripts/terrain_geo/data/ES_dem.png (elevacion en m + 5000, PNG de 16 bits) y ES_mask.png (cobertura de
España, 0-255), ambos del volumen entero a 0,5 m de juego por pixel con el Norte arriba.

    uv run --with numpy --with scipy --with pillow python -m terrain_geo.fetch_spain   (desde Scripts/)

Fuentes (dominio publico / abiertas):
  - Relieve y batimetria: teselas "terrarium" de Mapzen/AWS Terrain Tiles (SRTM, GMTED, ETOPO1 ...),
    zoom 7 (~1,2 km por pixel a 40 N): https://registry.opendata.aws/terrain-tiles/
  - Frontera: Natural Earth 1:50m admin-0 (dominio publico): https://www.naturalearthdata.com/
Los datos crudos se cachean en Saved/terrain_geo_cache/ (no se versiona); los PNG de salida si.
"""

from __future__ import annotations

import io
import json
import math
import urllib.request
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage

from .layout import (DATA_DIR, DEM_FILE, ELEVATION_OFFSET_M, EARTH_R_M, MAP_MAX_M, MAP_MIN_M, MASK_FILE, RASTER_PX,
                     RASTER_PX_M, WORLD_M, game_to_lonlat, lonlat_to_game, lonlat_to_mercator, raster_axis)

ZOOM = 7
TILE_URL = "https://s3.amazonaws.com/elevation-tiles-prod/terrarium/{z}/{x}/{y}.png"
BORDERS_URL = "https://raw.githubusercontent.com/nvkelso/natural-earth-vector/master/geojson/ne_50m_admin_0_countries.geojson"
CACHE = Path(__file__).resolve().parents[2] / "Saved" / "terrain_geo_cache"
SUPERSAMPLE = 4                  # la mascara se dibuja a 4x y se promedia: cobertura, no borde escalonado
TILE_M = WORLD_M / 2 ** ZOOM
PIXEL_M = TILE_M / 256.0


def download(url: str, target: Path) -> bytes:
    if target.exists():
        return target.read_bytes()
    target.parent.mkdir(parents=True, exist_ok=True)
    with urllib.request.urlopen(url, timeout=60) as response:
        data = response.read()
    target.write_bytes(data)
    return data


def tile_range() -> tuple[range, range]:
    """Teselas (x, y) que cubren el volumen entero."""
    x, y = np.meshgrid([MAP_MIN_M, MAP_MAX_M], [MAP_MIN_M, MAP_MAX_M])
    lon, lat = game_to_lonlat(x, y)
    mx, my = lonlat_to_mercator(lon, lat)
    tx = (mx + WORLD_M / 2.0) / TILE_M
    ty = (WORLD_M / 2.0 - my) / TILE_M
    return range(int(math.floor(tx.min())), int(math.floor(tx.max())) + 1), range(int(math.floor(ty.min())), int(math.floor(ty.max())) + 1)


def fetch_mosaic() -> tuple[np.ndarray, float, float]:
    """(elevacion en m, mx del borde izquierdo, my del borde superior) del mosaico de teselas."""
    xs, ys = tile_range()
    mosaic = np.zeros((len(ys) * 256, len(xs) * 256))
    for j, ty in enumerate(ys):
        for i, tx in enumerate(xs):
            raw = download(TILE_URL.format(z=ZOOM, x=tx, y=ty), CACHE / f"{ZOOM}_{tx}_{ty}.png")
            rgb = np.asarray(Image.open(io.BytesIO(raw)).convert("RGB")).astype(np.float64)
            mosaic[j * 256:(j + 1) * 256, i * 256:(i + 1) * 256] = rgb[..., 0] * 256.0 + rgb[..., 1] + rgb[..., 2] / 256.0 - 32768.0
    return mosaic, xs[0] * TILE_M - WORLD_M / 2.0, WORLD_M / 2.0 - ys[0] * TILE_M


def game_grid_to_mercator() -> tuple[np.ndarray, np.ndarray]:
    axis = raster_axis()
    lon, lat = game_to_lonlat(axis[:, None], axis[None, :])
    mx, my = lonlat_to_mercator(lon, lat)
    return np.broadcast_to(mx, (RASTER_PX, RASTER_PX)), np.broadcast_to(my, (RASTER_PX, RASTER_PX))


def build_dem() -> np.ndarray:
    mosaic, left, top = fetch_mosaic()
    mx, my = game_grid_to_mercator()
    coords = [(top - my) / PIXEL_M - 0.5, (mx - left) / PIXEL_M - 0.5]
    return ndimage.map_coordinates(mosaic, coords, order=1, mode="nearest")


def spain_polygons() -> list[list[list[tuple[float, float]]]]:
    """Cada poligono de España como [anillo exterior, huecos...] con (lon, lat)."""
    data = json.loads(download(BORDERS_URL, CACHE / "ne_50m_admin_0_countries.geojson"))
    out = []
    for feature in data["features"]:
        props = feature["properties"]
        if props.get("ADMIN") != "Spain" and props.get("NAME") != "Spain":
            continue
        geometry = feature["geometry"]
        parts = geometry["coordinates"] if geometry["type"] == "MultiPolygon" else [geometry["coordinates"]]
        out += [[[(p[0], p[1]) for p in ring] for ring in part] for part in parts]
    return out


def build_mask() -> np.ndarray:
    size = RASTER_PX * SUPERSAMPLE
    image = Image.new("L", (size, size), 0)
    draw = ImageDraw.Draw(image)
    scale = SUPERSAMPLE / RASTER_PX_M
    for polygon in spain_polygons():
        for k, ring in enumerate(polygon):
            lon, lat = np.array(ring).T
            X, Y = lonlat_to_game(lon, lat)
            if X.max() < MAP_MIN_M or X.min() > MAP_MAX_M or Y.max() < MAP_MIN_M or Y.min() > MAP_MAX_M:
                continue                                            # Canarias, Ceuta fuera de la ventana...
            points = list(zip(((Y - MAP_MIN_M) * scale).tolist(), ((MAP_MAX_M - X) * scale).tolist()))
            draw.polygon(points, fill=255 if k == 0 else 0)
    coverage = np.asarray(image, dtype=np.float64).reshape(RASTER_PX, SUPERSAMPLE, RASTER_PX, SUPERSAMPLE).mean(axis=(1, 3))
    return np.rint(coverage[::-1]).astype(np.uint8)          # la imagen tiene el Norte arriba; el raster, la fila 0 al Sur


def save_dem(elevation: np.ndarray, path: Path = DEM_FILE) -> None:
    packed = np.clip(np.rint(elevation + ELEVATION_OFFSET_M), 0, 65535).astype(np.uint16)
    path.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(np.ascontiguousarray(packed[::-1])).save(path, optimize=True)


def load_dem(path: Path = DEM_FILE) -> np.ndarray:
    """Elevacion en m, indexada [Norte, Este] (el PNG guarda el Norte arriba)."""
    with Image.open(path) as image:
        return np.asarray(image, dtype=np.float64)[::-1] - ELEVATION_OFFSET_M


def save_mask(mask: np.ndarray, path: Path = MASK_FILE) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(np.ascontiguousarray(mask[::-1])).save(path, optimize=True)


def load_mask(path: Path = MASK_FILE) -> np.ndarray:
    with Image.open(path) as image:
        return np.asarray(image, dtype=np.float64)[::-1] / 255.0


def main() -> None:
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    dem = build_dem()
    mask = build_mask()
    save_dem(dem)
    save_mask(mask)
    land = mask > 0.5
    print(f"{DEM_FILE.name}: {RASTER_PX}x{RASTER_PX} px, elevacion {dem.min():.0f}..{dem.max():.0f} m; "
          f"{MASK_FILE.name}: {land.mean() * 100:.1f} % del volumen es España; "
          f"cota maxima en España {dem[land].max():.0f} m (radio terrestre {EARTH_R_M:.0f} m)")


if __name__ == "__main__":
    main()

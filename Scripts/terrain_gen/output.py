"""Hojas de contactos para revisar la libreria a ojo."""

from __future__ import annotations

import math
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from PIL import Image

from .core import *  # noqa: F401,F403


# Colores de suelo por bioma en la hoja de contactos (orientativos; los de juego estan en C++).
PREVIEW_FLOOR = {"sand": (0.85, 0.70, 0.45), "water": (0.80, 0.74, 0.58), "algae": (0.42, 0.58, 0.30)}


def hillshade(heights: np.ndarray, bridges: list[Bridge], monoliths: list[Monolith], mask: np.ndarray,
              biome: str, secondary: str) -> np.ndarray:
    meters = (heights.astype(np.float64) - HEIGHT_ZERO) / UNITS_PER_M
    gx, gy = np.gradient(meters, STEP_M)
    nx, ny, nz = -gx, -gy, np.ones_like(gx)
    norm = np.sqrt(nx * nx + ny * ny + nz * nz)
    light = np.array([-0.5, 0.35, 0.79])
    shade = np.clip((nx * light[0] + ny * light[1] + nz * light[2]) / norm, 0.0, 1.0)
    # Tinte por cota y bioma: azul bajo el agua, suelo del bioma, oscuro en la meseta.
    blend = (mask >> 8).astype(np.float64) / 255.0
    foliage = (mask & 0xFF).astype(np.float64) / 255.0
    floor = np.array(PREVIEW_FLOOR[biome]) * (1 - blend)[..., None] + np.array(PREVIEW_FLOOR[secondary]) * blend[..., None]
    water = smoothstep(WATER_M + 1.0, WATER_M - 0.5, meters)
    high = smoothstep(4.0, 9.0, meters)[..., None]
    rgb = floor * (1 - high) + np.array([0.35, 0.32, 0.28]) * high
    rgb = rgb * (1 - 0.6 * foliage)[..., None] + np.array([0.08, 0.30, 0.10]) * (0.6 * foliage)[..., None]
    rgb = rgb * (0.35 + 0.65 * shade)[..., None]
    rgb = rgb * (1 - 0.7 * water)[..., None] + np.array([0.15, 0.35, 0.65]) * (0.7 * water)[..., None]
    for monolith in monoliths:
        rgb[np.hypot(XX - monolith.x, YY - monolith.y) <= monolith.radius_m] = np.array([0.2, 0.15, 0.3])
    for bridge in bridges:
        ax, ay = math.cos(math.radians(bridge.yaw_deg)), math.sin(math.radians(bridge.yaw_deg))
        along = (XX - bridge.x) * ax + (YY - bridge.y) * ay
        across = -(XX - bridge.x) * ay + (YY - bridge.y) * ax
        deck = (np.abs(along) <= bridge.length_m / 2.0) & (np.abs(across) <= bridge.width_m / 2.0 + 1.0)
        rgb[deck] = np.array([0.95, 0.95, 0.85])
    return (np.clip(rgb, 0, 1) * 255).astype(np.uint8)


def write_contact_sheet(path: Path, thumbs: list[np.ndarray], columns: int = 10, size: int = 120) -> None:
    rows = int(math.ceil(len(thumbs) / columns))
    sheet = np.zeros((rows * size, columns * size, 3), dtype=np.uint8)
    for k, thumb in enumerate(thumbs):
        img = Image.fromarray(thumb).resize((size, size), Image.Resampling.BILINEAR)
        # El eje X (Sur -> Norte) va hacia ARRIBA en la miniatura.
        arr = np.asarray(img)[::-1, :, :]
        r, c = divmod(k, columns)
        sheet[r * size:(r + 1) * size, c * size:(c + 1) * size] = arr
    Image.fromarray(sheet).save(path)

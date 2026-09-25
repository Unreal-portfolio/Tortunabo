"""Genera las texturas del agua de Tortunabo (se repiten sin costura):

    Scripts/textures/T_WaterNormal.png   mapa de normales de oleaje fino (RGB, lineal)
    Scripts/textures/T_WaterFoam.png     patron de espuma (gris, lineal)
    Scripts/textures/WaterSurface.bin    rejilla del agua (formato TNTM1 de los trozos del mapa)

Se ejecuta FUERA del editor:
    uv run --with numpy --with pillow python Scripts/gen_water_textures.py

Las importa Scripts/build_water.py. Todo sale de sumas de ondas con frecuencias enteras sobre
el cuadrado, asi que el borde de una copia casa con el de la siguiente.
"""

from __future__ import annotations

from pathlib import Path

import struct

import numpy as np
from PIL import Image

SIZE = 1024
OUT = Path(__file__).resolve().parent / "textures"


def tileable_waves(rng: np.random.Generator, count: int, fmin: int, fmax: int, falloff: float) -> np.ndarray:
    """Suma de ondas con vectores de frecuencia enteros (se repite exacta en el cuadrado),
    con cresta algo afilada (como el oleaje) y amplitud que baja con la frecuencia."""
    y, x = np.mgrid[0:SIZE, 0:SIZE] / SIZE
    h = np.zeros((SIZE, SIZE))
    for _ in range(count):
        f = int(rng.integers(fmin, fmax + 1))
        angle = rng.uniform(0.0, 2.0 * np.pi)
        kx, ky = int(round(f * np.cos(angle))), int(round(f * np.sin(angle)))
        if kx == 0 and ky == 0:
            continue
        phase = rng.uniform(0.0, 2.0 * np.pi)
        wave = np.sin(2.0 * np.pi * (kx * x + ky * y) + phase)
        crest = 1.0 - np.abs(wave)                   # onda de cresta afilada
        amp = (np.hypot(kx, ky)) ** (-falloff)
        h += amp * (0.6 * wave + 0.4 * (crest * 2.0 - 1.0))
    h -= h.min()
    return h / max(h.max(), 1e-9)


def normal_map(height: np.ndarray, strength: float) -> np.ndarray:
    # Diferencias centrales con vuelta (se repite): la normal casa en los bordes.
    dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * 0.5
    dy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * 0.5
    n = np.dstack([-dx * strength, -dy * strength, np.ones_like(height)])
    n /= np.linalg.norm(n, axis=2, keepdims=True)
    return ((n * 0.5 + 0.5) * 255.0).round().astype(np.uint8)


def foam_pattern(rng: np.random.Generator) -> np.ndarray:
    """Espuma: celdas irregulares (maximo de ondas cruzadas) con agujeros, en gris."""
    a = tileable_waves(rng, 48, 3, 20, 0.6)
    b = tileable_waves(rng, 48, 8, 40, 0.4)
    foam = np.clip((a * 0.6 + b * 0.4 - 0.45) * 3.0, 0.0, 1.0)
    return (foam * 255.0).round().astype(np.uint8)


SURFACE_SIZE_UU = 140000.0              # 1,4 km centrados en el mapa
SURFACE_STEPS = 350                     # un vertice cada 4 m: el oleaje se ve de verdad


def write_surface(path: Path) -> None:
    """Rejilla plana del agua en TNTM1 (la convierte en StaticMesh BuildStaticMesh, como los
    trozos del terreno). Cara visible hacia arriba segun Unreal (orden de BuildGridTriangles)."""
    v = SURFACE_STEPS + 1
    axis = np.linspace(-SURFACE_SIZE_UU / 2.0, SURFACE_SIZE_UU / 2.0, v)
    xs, ys = np.meshgrid(axis, axis, indexing="ij")
    vertices = np.stack([xs.ravel(), ys.ravel(), np.zeros(v * v)], axis=1).astype("<f4")
    normals = np.tile(np.array([0.0, 0.0, 1.0], dtype="<f4"), (v * v, 1))
    colors = np.full((v * v, 4), 255, dtype=np.uint8)
    i, j = np.meshgrid(np.arange(v - 1), np.arange(v - 1), indexing="ij")
    i0 = (i * v + j).ravel()
    i1 = ((i + 1) * v + j).ravel()
    i2 = ((i + 1) * v + j + 1).ravel()
    i3 = (i * v + j + 1).ravel()
    tris = np.stack([i0, i3, i1, i1, i3, i2], axis=1).reshape(-1, 3).astype("<u4")
    with open(path, "wb") as handle:
        handle.write(struct.pack("<4sIIII", b"TNTM", 1, len(vertices), len(tris), 0))
        for block in (vertices, normals, colors, tris):
            handle.write(block.tobytes())


def main() -> None:
    rng = np.random.default_rng(20260925)
    OUT.mkdir(exist_ok=True)
    height = tileable_waves(rng, 90, 2, 48, 1.1)
    Image.fromarray(normal_map(height, 45.0), "RGB").save(OUT / "T_WaterNormal.png")
    Image.fromarray(foam_pattern(rng), "L").save(OUT / "T_WaterFoam.png")
    write_surface(OUT / "WaterSurface.bin")
    print(f"texturas y superficie del agua en {OUT}")


if __name__ == "__main__":
    main()

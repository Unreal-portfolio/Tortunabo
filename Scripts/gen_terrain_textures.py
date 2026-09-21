"""Genera las texturas tileables de grano del terreno del grid procedural.

Se ejecuta FUERA del editor:
    uv run --with numpy --with pillow python Scripts/gen_terrain_textures.py

Escribe Scripts/textures/T_TerrainGrain.png, que build_grid_demo_assets.py importa a
/Game/Textures/Terrain. El PNG se versiona para que no haga falta regenerarlo en cada
maquina; este script solo se vuelve a ejecutar si se cambian los parametros.

El ruido es value noise fractal con envoltura periodica: la textura es tileable por
construccion, sin costura en los bordes. Semilla fija => salida identica byte a byte.
"""

from pathlib import Path

import numpy as np
from PIL import Image

SIZE = 512
SEED = 20260921
OCTAVES = ((4, 1.0), (8, 0.5), (16, 0.25), (32, 0.125), (64, 0.0625))
OUTPUT = Path(__file__).resolve().parent / "textures" / "T_TerrainGrain.png"


def smoothstep(t):
    return t * t * (3.0 - 2.0 * t)


def value_noise(size, frequency, rng):
    """Value noise periodico: la rejilla se muestrea con wrap, asi que el resultado tilea."""
    lattice = rng.random((frequency, frequency))

    coords = np.arange(size) * frequency / size
    cell = np.floor(coords).astype(np.int32)
    frac = smoothstep(coords - cell)

    i0, i1 = cell % frequency, (cell + 1) % frequency
    fy = frac[:, None]
    fx = frac[None, :]

    c00 = lattice[np.ix_(i0, i0)]
    c01 = lattice[np.ix_(i0, i1)]
    c10 = lattice[np.ix_(i1, i0)]
    c11 = lattice[np.ix_(i1, i1)]

    top = c00 * (1.0 - fx) + c01 * fx
    bottom = c10 * (1.0 - fx) + c11 * fx
    return top * (1.0 - fy) + bottom * fy


def build_grain():
    rng = np.random.default_rng(SEED)
    field = np.zeros((SIZE, SIZE), dtype=np.float64)
    total = 0.0
    for frequency, amplitude in OCTAVES:
        field += value_noise(SIZE, frequency, rng) * amplitude
        total += amplitude
    field /= total

    # Recentrar en 0.5 y normalizar el rango: el material lo lee como un multiplicador
    # alrededor de 1, de modo que el color de vertice se conserva de media.
    field -= field.mean()
    peak = np.abs(field).max()
    if peak > 0.0:
        field /= peak * 2.0
    return field + 0.5


def main():
    grain = build_grain()
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    pixels = np.clip(np.rint(grain * 255.0), 0, 255).astype(np.uint8)
    Image.fromarray(pixels, mode="L").save(OUTPUT, optimize=True)
    print(f"{OUTPUT} escrita: {SIZE}x{SIZE}, media {grain.mean():.4f}, rango "
          f"[{grain.min():.4f}, {grain.max():.4f}]")


if __name__ == "__main__":
    main()

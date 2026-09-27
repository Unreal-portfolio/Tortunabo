"""Genera las texturas tileables de grano y de normal de detalle del terreno.

Se ejecuta FUERA del editor:
    uv run --with numpy --with pillow python Scripts/gen_terrain_textures.py

Escribe en Scripts/textures (build_grid_demo_assets.py las importa a /Game/Textures/Terrain):
- T_TerrainGrain.png: grano en gris (multiplicador del color).
- T_TerrainFloorN.png: normal de detalle del suelo, RG = arena (rizos de viento y grano),
  BA = camino (arena pisada: grano fino y huellas suaves). XY de la normal en 0..1.
- T_TerrainWallN.png: normal de detalle de la pared (estratos horizontales y roca), RG. El PNG se versiona para que no haga falta regenerarlo en cada
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
FLOOR_OUTPUT = OUTPUT.with_name("T_TerrainFloorN.png")
WALL_OUTPUT = OUTPUT.with_name("T_TerrainWallN.png")
NORMAL_SIZE = 1024


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


def fractal(size: int, octaves, rng: np.random.Generator) -> np.ndarray:
    """Suma de value noise periodico normalizada a -1..1 (tilea)."""
    field = sum(value_noise(size, f, rng) * a for f, a in octaves)
    field = field - field.mean()
    return field / max(np.abs(field).max(), 1e-9)


def height_to_normal_xy(height: np.ndarray, strength: float) -> np.ndarray:
    """XY de la normal (en -1..1) de un relieve periodico: gradiente centrado con envoltura."""
    dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * 0.5 * strength
    dy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * 0.5 * strength
    n = np.stack([-dx, -dy, np.ones_like(dx)], axis=-1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    return n[..., :2]


def build_sand_height(rng: np.random.Generator) -> np.ndarray:
    """Rizos de viento (crestas asimetricas que serpentean y se cortan) sobre grano fino."""
    size = NORMAL_SIZE
    y, x = np.mgrid[0:size, 0:size] / size
    warp = fractal(size, ((3, 1.0), (6, 0.5)), rng)
    fade = 0.5 + 0.5 * fractal(size, ((2, 1.0), (4, 0.5)), rng)
    phase = 14.0 * (x + 0.35 * y) + 1.4 * warp
    frac = phase - np.floor(phase)
    ripple = np.where(frac < 0.7, frac / 0.7, (1.0 - frac) / 0.3)       # cara larga y cara corta
    grain = fractal(size, ((128, 1.0), (256, 0.6), (512, 0.35)), rng)
    return 0.9 * ripple * (0.35 + 0.65 * fade) + 0.05 * grain


def build_path_height(rng: np.random.Generator) -> np.ndarray:
    """Arena pisada: sin rizos, bultos suaves (huellas y compactado) y grano mas fino."""
    size = NORMAL_SIZE
    bumps = fractal(size, ((12, 1.0), (24, 0.6)), rng)
    grain = fractal(size, ((256, 1.0), (512, 0.5)), rng)
    return 1.2 * bumps + 0.06 * grain


def build_wall_height(rng: np.random.Generator) -> np.ndarray:
    """Pared de arena compactada: estratos horizontales (eje y de la textura = altura) que
    ondulan, cortados por grietas, con rugosidad de roca."""
    size = NORMAL_SIZE
    y, x = np.mgrid[0:size, 0:size] / size
    wobble = fractal(size, ((2, 1.0), (5, 0.4)), rng)
    layers = 10.0 * y + 0.6 * wobble
    frac = layers - np.floor(layers)
    strata = np.where(frac < 0.8, frac / 0.8, (1.0 - frac) / 0.2)
    rock = fractal(size, ((16, 1.0), (32, 0.6), (64, 0.35), (256, 0.08)), rng)
    return 0.7 * strata + 0.9 * rock


def to_bytes(xy: np.ndarray) -> np.ndarray:
    return np.clip(np.rint((xy * 0.5 + 0.5) * 255.0), 0, 255).astype(np.uint8)


def build_normals() -> tuple[np.ndarray, np.ndarray]:
    rng = np.random.default_rng(SEED + 1)
    sand = height_to_normal_xy(build_sand_height(rng), 6.0)
    path = height_to_normal_xy(build_path_height(rng), 14.0)
    wall = height_to_normal_xy(build_wall_height(rng), 16.0)
    floor = np.concatenate([to_bytes(sand), to_bytes(path)], axis=-1)
    wall_rgb = np.concatenate([to_bytes(wall), np.full(wall.shape[:2] + (1,), 255, np.uint8)], axis=-1)
    return floor, wall_rgb


def main():
    floor, wall = build_normals()
    Image.fromarray(floor, mode="RGBA").save(FLOOR_OUTPUT, optimize=True)
    Image.fromarray(wall, mode="RGB").save(WALL_OUTPUT, optimize=True)
    print(f"{FLOOR_OUTPUT.name} y {WALL_OUTPUT.name} escritas: {NORMAL_SIZE}x{NORMAL_SIZE}")
    grain = build_grain()
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    pixels = np.clip(np.rint(grain * 255.0), 0, 255).astype(np.uint8)
    Image.fromarray(pixels, mode="L").save(OUTPUT, optimize=True)
    print(f"{OUTPUT} escrita: {SIZE}x{SIZE}, media {grain.mean():.4f}, rango "
          f"[{grain.min():.4f}, {grain.max():.4f}]")


if __name__ == "__main__":
    main()

"""Genera la libreria de modulos de terreno: heightfields de 400 m x 400 m, 50 por topologia.

Se ejecuta FUERA del editor:
    uv run --with numpy --with pillow python Scripts/gen_terrain_modules.py [--count 50]

Escribe en Scripts/terrain_modules/:
    <Topologia>/M_<Topologia>_<NN>.png   heightfield de 16 bits (201 x 201), fuente de verdad
    manifest.json                         lista de modulos con topologia, semilla y codificacion
    preview_<Topologia>.png               hoja de contactos sombreada, para revisar a ojo

Los PNG los importa Scripts/import_terrain_modules.py (dentro del editor) como
UTN_TerrainModuleAsset + BP_Mod_*. Semilla fija => salida identica byte a byte.

Modelo de un modulo (alturas en metros, Z = 0 es el suelo de las salidas):
  - Borde canonico: los cuatro lados comparten el MISMO perfil simetrico (cresta de
    CREST_M con una boca de suelo en el centro). Cualquier modulo casa con cualquier otro
    con cualquier rotacion. Un lado sin salida se cierra con una rampa por dentro.
  - Pasillo: polilineas del centro a cada salida, con serpenteo que se apaga junto al
    borde; ancho variable; plazas (zonas llanas para puzzles) en el cruce y a lo largo.
  - Paredes: talud corto (no escalable) hasta la meseta, con colinas suaves encima.
    Variante "cresta": el exterior baja bajo el agua y el pasillo queda como una calzada.
  - Agujeros: pozos en las plazas (fuera del carril central) y en la meseta.
  - Ruido de pocas octavas + desenfoque final: sin picos.
"""

from __future__ import annotations

import argparse
import json
import math
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from PIL import Image

# ── Geometria y codificacion (deben casar con UTN_TerrainModuleAsset) ─────────────
SIZE_M = 400.0
HALF_M = SIZE_M / 2.0
RES = 201                      # 2 m entre vertices
HEIGHT_SCALE_UU = 0.25         # uu por unidad del PNG
HEIGHT_ZERO = 32768            # valor del PNG para Z = 0
UU_PER_M = 100.0
UNITS_PER_M = UU_PER_M / HEIGHT_SCALE_UU   # 400 unidades por metro

# ── Borde canonico ────────────────────────────────────────────────────────────────
CREST_M = 10.0                 # cota de la cresta en el borde
OPEN_HALF_M = 18.0             # semiancho de la boca del pasillo en el borde
BANK_M = 12.0                  # anchura del talud de la boca
UNDULATE_FROM_M = 60.0         # desde aqui la cresta ondula...
UNDULATE_TO_M = 170.0          # ...y vuelve a ser plana antes de la esquina
BAND_M = 16.0                  # franja en la que el diseno se funde con el borde
PLUG_M = 22.0                  # altura minima tras una salida cerrada: rampa no escalable

BASE_SEED = 20260922
TOPOLOGIES: dict[str, tuple[str, ...]] = {
    "Straight": ("S", "N"),
    "CurveLeft": ("S", "W"),
    "CurveRight": ("S", "E"),
    "TLeft": ("S", "N", "W"),
    "TRight": ("S", "N", "E"),
    "Cross": ("N", "E", "S", "W"),
}
# X = fila (Sur -> Norte), Y = columna (Oeste -> Este), igual que en el asset.
EXIT_POINT = {"N": (HALF_M, 0.0), "E": (0.0, HALF_M), "S": (-HALF_M, 0.0), "W": (0.0, -HALF_M)}

OUTPUT_DIR = Path(__file__).resolve().parent / "terrain_modules"

_AXIS = np.linspace(-HALF_M, HALF_M, RES)
XX, YY = np.meshgrid(_AXIS, _AXIS, indexing="ij")
DIST_TO_EDGE = np.minimum(HALF_M - np.abs(XX), HALF_M - np.abs(YY))


# ── Utilidades ────────────────────────────────────────────────────────────────────
def smoothstep(edge0: float, edge1: float, x):
    t = np.clip((x - edge0) / (edge1 - edge0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def value_noise(rng: np.random.Generator, wavelength_m: float, pts_x=None, pts_y=None):
    """Value noise en [-1, 1] con interpolacion suave; no tilea (no hace falta)."""
    px = XX if pts_x is None else pts_x
    py = YY if pts_y is None else pts_y
    cells = int(math.ceil(SIZE_M / wavelength_m)) + 4
    lattice = rng.uniform(-1.0, 1.0, (cells, cells))
    u = (px + HALF_M) / wavelength_m + 1.0
    v = (py + HALF_M) / wavelength_m + 1.0
    i0 = np.clip(np.floor(u).astype(int), 0, cells - 2)
    j0 = np.clip(np.floor(v).astype(int), 0, cells - 2)
    fu = smoothstep(0.0, 1.0, u - i0)
    fv = smoothstep(0.0, 1.0, v - j0)
    a = lattice[i0, j0]
    b = lattice[i0 + 1, j0]
    c = lattice[i0, j0 + 1]
    d = lattice[i0 + 1, j0 + 1]
    return (a * (1 - fu) + b * fu) * (1 - fv) + (c * (1 - fu) + d * fu) * fv


def fbm(rng: np.random.Generator, wavelength_m: float, octaves: int = 3, gain: float = 0.5, **kw):
    total = np.zeros_like(XX)
    amplitude = 1.0
    norm = 0.0
    for octave in range(octaves):
        total += amplitude * value_noise(rng, wavelength_m / (2 ** octave), **kw)
        norm += amplitude
        amplitude *= gain
    return total / norm


def blur(field, passes: int = 1):
    """Desenfoque separable 1-4-6-4-1 (aprox. gaussiano, sigma ~ 1 muestra)."""
    kernel = np.array([1.0, 4.0, 6.0, 4.0, 1.0]) / 16.0
    out = field
    for _ in range(passes):
        padded = np.pad(out, 2, mode="edge")
        tmp = sum(kernel[k] * padded[k:k + RES, 2:2 + RES] for k in range(5))
        padded = np.pad(tmp, 2, mode="edge")
        out = sum(kernel[k] * padded[2:2 + RES, k:k + RES] for k in range(5))
    return out


def dist_to_segment(px, py, ax, ay, bx, by):
    abx, aby = bx - ax, by - ay
    length_sq = abx * abx + aby * aby
    t = 0.0 if length_sq == 0.0 else np.clip(((px - ax) * abx + (py - ay) * aby) / length_sq, 0.0, 1.0)
    return np.hypot(px - (ax + abx * t), py - (ay + aby * t))


def dist_to_polylines(px, py, segments):
    best = np.full_like(px, np.inf)
    for (ax, ay), (bx, by) in segments:
        best = np.minimum(best, dist_to_segment(px, py, ax, ay, bx, by))
    return best


# ── Borde canonico ────────────────────────────────────────────────────────────────
def canonical_profile(t_abs):
    """Altura del borde en funcion de |t| (t a lo largo del lado, 0 en el centro)."""
    bank = smoothstep(OPEN_HALF_M, OPEN_HALF_M + BANK_M, t_abs)
    fade = smoothstep(UNDULATE_FROM_M - 20.0, UNDULATE_FROM_M, t_abs) * (1.0 - smoothstep(UNDULATE_TO_M - 25.0, UNDULATE_TO_M, t_abs))
    undulation = 1.3 * np.cos(2.0 * np.pi * t_abs / 61.0) + 0.7 * np.cos(2.0 * np.pi * t_abs / 23.0 + 1.1)
    return CREST_M * bank + undulation * fade


def canonical_border_field():
    """Perfil del lado mas cercano, extendido hacia dentro (constante en la esquina)."""
    nearest_is_ns = (HALF_M - np.abs(XX)) <= (HALF_M - np.abs(YY))
    t_abs = np.where(nearest_is_ns, np.abs(YY), np.abs(XX))
    return canonical_profile(t_abs)


CANONICAL = canonical_border_field()
BORDER_WEIGHT = 1.0 - smoothstep(0.0, BAND_M, DIST_TO_EDGE)


# ── Diseno del interior ───────────────────────────────────────────────────────────
@dataclass
class Room:
    x: float
    y: float
    radius: float


def corridor_segments(exits: tuple[str, ...]):
    return [((0.0, 0.0), EXIT_POINT[e]) for e in exits]


def pick_rooms(rng: np.random.Generator, exits: tuple[str, ...]) -> list[Room]:
    rooms = [Room(0.0, 0.0, float(rng.uniform(45.0, 70.0)))]
    for e in exits:
        if rng.random() < 0.7:
            ex, ey = EXIT_POINT[e]
            t = float(rng.uniform(0.42, 0.68))
            rooms.append(Room(ex * t, ey * t, float(rng.uniform(32.0, 58.0))))
    return rooms


def room_mask(rooms: list[Room]):
    mask = np.zeros_like(XX)
    for room in rooms:
        d = np.hypot(XX - room.x, YY - room.y)
        mask = np.maximum(mask, 1.0 - smoothstep(room.radius - 6.0, room.radius + 6.0, d))
    return mask


def pits(rng: np.random.Generator, rooms: list[Room], d_corr, hw):
    """Pozos: en las plazas (fuera del carril central) y sueltos en la meseta."""
    depression = np.zeros_like(XX)
    for room in rooms:
        if rng.random() < 0.45:
            angle = float(rng.uniform(0.0, 2.0 * np.pi))
            offset = room.radius * 0.55
            cx, cy = room.x + offset * math.cos(angle), room.y + offset * math.sin(angle)
            radius = float(rng.uniform(5.0, 8.0))
            depth = float(rng.uniform(3.5, 6.0))
            d = np.hypot(XX - cx, YY - cy)
            depression += depth * (1.0 - smoothstep(radius * 0.45, radius, d))
    for _ in range(int(rng.integers(0, 3))):
        cx, cy = rng.uniform(-140.0, 140.0, 2)
        radius = float(rng.uniform(8.0, 16.0))
        depth = float(rng.uniform(4.0, 9.0))
        d = np.hypot(XX - cx, YY - cy)
        bowl = depth * (1.0 - smoothstep(radius * 0.45, radius, d))
        # Solo en la meseta: nunca muerde el pasillo ni su talud.
        depression += bowl * smoothstep(hw + 14.0, hw + 26.0, d_corr)
    return depression


def design_module(rng: np.random.Generator, exits: tuple[str, ...]) -> tuple[np.ndarray, dict]:
    """Heightfield del interior (metros) antes de fundirlo con el borde canonico."""
    edge_fade = smoothstep(0.0, 50.0, DIST_TO_EDGE)
    is_causeway = rng.random() < 0.2

    # Pasillo con serpenteo (deformacion del dominio) que se apaga junto al borde.
    warp_amp = float(rng.uniform(35.0, 65.0)) * smoothstep(0.0, 70.0, DIST_TO_EDGE)
    wx = XX + warp_amp * fbm(rng, 180.0, octaves=2)
    wy = YY + warp_amp * fbm(rng, 180.0, octaves=2)
    d_corr = dist_to_polylines(wx, wy, corridor_segments(exits))

    base_hw = float(rng.uniform(18.0, 30.0))
    hw = base_hw * (1.0 + 0.3 * value_noise(rng, 110.0))
    hw = OPEN_HALF_M + (hw - OPEN_HALF_M) * edge_fade   # en el borde, la boca canonica
    bank = float(rng.uniform(6.0, 9.0))
    bank = BANK_M + (bank - BANK_M) * edge_fade

    rooms = pick_rooms(rng, exits)
    rmask = room_mask(rooms)
    corridor = np.maximum(1.0 - smoothstep(hw, hw + bank, d_corr), rmask)

    # Suelo: cota que ondula despacio y se aplana en las plazas; 0 junto a las salidas.
    elev_amp = float(rng.uniform(4.0, 9.0))
    elev = elev_amp * fbm(rng, 260.0, octaves=2) * edge_fade
    floor = elev.copy()
    for room in rooms:
        i = int(round((room.x + HALF_M) / SIZE_M * (RES - 1)))
        j = int(round((room.y + HALF_M) / SIZE_M * (RES - 1)))
        m = 1.0 - smoothstep(room.radius - 6.0, room.radius + 6.0, np.hypot(XX - room.x, YY - room.y))
        floor = floor * (1.0 - m) + elev[i, j] * m
    floor += 0.30 * fbm(rng, 18.0, octaves=2)                                  # arena
    off_lane = smoothstep(8.0, 13.0, d_corr) * (1.0 - 0.6 * rmask)
    rocks = float(rng.uniform(1.5, 3.0)) * smoothstep(0.58, 0.76, value_noise(rng, 22.0) * 0.5 + 0.5) * off_lane
    floor += rocks * edge_fade
    floor -= pits(rng, rooms, d_corr, hw)

    # Meseta: cresta sobre el suelo local + colinas suaves. Variante calzada: exterior hundido.
    hills = (fbm(rng, 170.0, octaves=3) * 0.5 + 0.5) * float(rng.uniform(12.0, 30.0))
    wall_h = float(rng.uniform(8.0, 14.0))
    if is_causeway:
        high = -12.0 + 0.35 * hills + 2.0 * fbm(rng, 60.0, octaves=2)
    else:
        high = elev + wall_h + hills
    # Junto al borde la meseta tiende a la cresta canonica; las salidas cerradas se tapan.
    high = CREST_M + (high - CREST_M) * edge_fade
    closed = np.zeros_like(XX, dtype=bool)
    for side, (ex, ey) in EXIT_POINT.items():
        if side in exits:
            continue
        along = np.abs(YY) if side in ("N", "S") else np.abs(XX)
        toward = np.hypot(XX - ex, YY - ey)
        closed |= (along < OPEN_HALF_M + BANK_M + 6.0) & (toward < 44.0)
    high = np.where(closed, np.maximum(high, PLUG_M), high)

    terrain = high * (1.0 - corridor) + floor * corridor
    terrain = blur(terrain)
    stats = {
        "causeway": is_causeway,
        "rooms": len(rooms),
        "wall_h": round(wall_h, 2),
        "base_half_width": round(base_hw, 2),
    }
    return terrain, stats


def compose_module(seed: int, exits: tuple[str, ...]):
    rng = np.random.default_rng(seed)
    design, stats = design_module(rng, exits)
    heights_m = design * (1.0 - BORDER_WEIGHT) + CANONICAL * BORDER_WEIGHT
    quantized = np.rint(heights_m * UNITS_PER_M) + HEIGHT_ZERO
    return np.clip(quantized, 0, 65535).astype(np.uint16), stats


# ── Validacion y salida ───────────────────────────────────────────────────────────
def canonical_edge_vector() -> np.ndarray:
    values = np.rint(canonical_profile(np.abs(_AXIS)) * UNITS_PER_M) + HEIGHT_ZERO
    return values.astype(np.uint16)


def check_border(heights: np.ndarray, expected: np.ndarray, name: str) -> None:
    edges = (heights[0, :], heights[-1, :], heights[:, 0], heights[:, -1])
    for k, edge in enumerate(edges):
        if not np.array_equal(edge, expected):
            bad = int(np.argmax(edge != expected))
            raise AssertionError(f"{name}: lado {k} rompe el borde canonico en la muestra {bad}")
    if not np.array_equal(expected, expected[::-1]):
        raise AssertionError("el perfil canonico no es simetrico")


def slope_degrees(heights: np.ndarray) -> np.ndarray:
    meters = (heights.astype(np.float64) - HEIGHT_ZERO) / UNITS_PER_M
    step = SIZE_M / (RES - 1)
    gx, gy = np.gradient(meters, step)
    return np.degrees(np.arctan(np.hypot(gx, gy)))


def hillshade(heights: np.ndarray) -> np.ndarray:
    meters = (heights.astype(np.float64) - HEIGHT_ZERO) / UNITS_PER_M
    step = SIZE_M / (RES - 1)
    gx, gy = np.gradient(meters, step)
    nx, ny, nz = -gx, -gy, np.ones_like(gx)
    norm = np.sqrt(nx * nx + ny * ny + nz * nz)
    light = np.array([-0.5, 0.35, 0.79])
    shade = np.clip((nx * light[0] + ny * light[1] + nz * light[2]) / norm, 0.0, 1.0)
    # Tinte por cota: azul bajo el agua, arena en el suelo, oscuro en la meseta.
    water = smoothstep(-2.0, -5.0, meters)
    high = smoothstep(4.0, 9.0, meters)
    r = 0.85 * (1 - high) + 0.35 * high
    g = 0.70 * (1 - high) + 0.32 * high
    b = 0.45 * (1 - high) + 0.28 * high
    rgb = np.stack([r, g, b], axis=-1) * (0.35 + 0.65 * shade)[..., None]
    rgb = rgb * (1 - water)[..., None] + np.array([0.15, 0.35, 0.65]) * water[..., None]
    return (np.clip(rgb, 0, 1) * 255).astype(np.uint8)


def write_contact_sheet(path: Path, thumbs: list[np.ndarray], columns: int = 10) -> None:
    size = 120
    rows = int(math.ceil(len(thumbs) / columns))
    sheet = np.zeros((rows * size, columns * size, 3), dtype=np.uint8)
    for k, thumb in enumerate(thumbs):
        img = Image.fromarray(thumb).resize((size, size), Image.Resampling.BILINEAR)
        # El eje X (Sur -> Norte) va hacia ARRIBA en la miniatura.
        arr = np.asarray(img)[::-1, :, :]
        r, c = divmod(k, columns)
        sheet[r * size:(r + 1) * size, c * size:(c + 1) * size] = arr
    Image.fromarray(sheet).save(path)


def main() -> None:
    parser = argparse.ArgumentParser(description="Genera la libreria de modulos de terreno.")
    parser.add_argument("--count", type=int, default=50, help="modulos por topologia")
    parser.add_argument("--out", type=Path, default=OUTPUT_DIR)
    args = parser.parse_args()

    expected_edge = canonical_edge_vector()
    manifest = {
        "size_uu": SIZE_M * UU_PER_M,
        "resolution": RES,
        "height_scale_uu": HEIGHT_SCALE_UU,
        "height_zero": HEIGHT_ZERO,
        "modules": [],
    }
    index = 0
    for topology, exits in TOPOLOGIES.items():
        folder = args.out / topology
        folder.mkdir(parents=True, exist_ok=True)
        thumbs = []
        for n in range(args.count):
            seed = BASE_SEED + index
            index += 1
            name = f"M_{topology}_{n + 1:02d}"
            heights, stats = compose_module(seed, exits)
            check_border(heights, expected_edge, name)
            Image.fromarray(heights).save(folder / f"{name}.png")
            thumbs.append(hillshade(heights))
            slopes = slope_degrees(heights)
            meters = (heights.astype(np.float64) - HEIGHT_ZERO) / UNITS_PER_M
            manifest["modules"].append({
                "name": name,
                "topology": topology,
                "seed": seed,
                "file": f"{topology}/{name}.png",
                "min_m": round(float(meters.min()), 2),
                "max_m": round(float(meters.max()), 2),
                "slope_p99_deg": round(float(np.percentile(slopes, 99)), 1),
                **stats,
            })
        write_contact_sheet(args.out / f"preview_{topology}.png", thumbs)
        print(f"{topology}: {args.count} modulos")

    (args.out / "manifest.json").write_text(json.dumps(manifest, indent=1), encoding="utf-8")
    mods = manifest["modules"]
    print(f"total {len(mods)} modulos; cota [{min(m['min_m'] for m in mods)}, {max(m['max_m'] for m in mods)}] m; "
          f"pendiente p99 max {max(m['slope_p99_deg'] for m in mods)} grados")


if __name__ == "__main__":
    main()

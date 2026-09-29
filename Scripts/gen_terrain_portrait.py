"""Genera un mapa a partir de una foto (brillo = altura, silueta = isla) en
Scripts/terrain_volumes/Variants/<nombre>/ y lo añade a index.json, listo para el desplegable de
ATN_MapVariantLoader.

    uv run --with numpy --with scipy --with pillow --with scikit-image python Scripts/gen_terrain_portrait.py \
        --image <foto.jpg> --name F01_retrato [--relief 22] [--size 540] [--focus 0.37,0.4,0.36,0.42]

--focus es la region de interes (ovalo: u, v del centro y radios, en fracciones de la imagen: u hacia la
derecha, v hacia abajo); la isla es ese ovalo y el resto de la foto no entra. --start y --end (u,v)
por defecto van del pie al extremo alto del ovalo. La foto se lee de
donde este y NO se copia al repositorio; el mapa que sale si contiene un relieve reconocible de ella: no lo
subas a un repositorio compartido sin permiso de quien sale en la foto.
"""

from __future__ import annotations

import argparse
import time
from pathlib import Path

import numpy as np

from gen_terrain_spain import kill_boxes_uu, flood, update_index, VARIANTS
from gen_terrain_volume import build_all, world_index, zone_map
from terrain_geo.portrait import FOCUS, RELIEF_M, SIZE_M, PortraitModel
from terrain_vol.export import global_top, write_map

DEFAULT_NAME = "F01_retrato"
DESCRIPTION = "Mapa a partir de una foto: el brillo es la altura y la silueta, una isla; el mar es la muerte."


def parse_point(text: str) -> tuple[float, float]:
    u, v = (float(t) for t in text.split(","))
    return u, v


def parse_quad(text: str) -> tuple[float, float, float, float]:
    u, v, ru, rv = (float(t) for t in text.split(","))
    return u, v, ru, rv


def build(image: Path, name: str, size_m: float, relief_m: float, focus, start_uv=None, end_uv=None) -> dict:
    t0 = time.time()
    model = PortraitModel(image, size_m, relief_m, focus)
    cu, cv, ru, rv = focus
    start_uv = start_uv or (cu, cv + 0.75 * rv)
    end_uv = end_uv or (cu, cv - 0.7 * rv)
    chunks = build_all(model)
    top = global_top(chunks)
    start_xy, end_xy = model.to_game(*start_uv), model.to_game(*end_uv)
    s_ij, e_ij = world_index(start_xy), world_index(end_xy)
    seen = flood(top, s_ij)
    ok = bool(seen[e_ij])
    out = VARIANTS / name
    write_map(out, name, 0, chunks, (*start_xy, float(top[s_ij])), (*end_xy, float(top[e_ij])), zone_map(model, chunks),
              np.linspace(start_xy, end_xy, 300), style=None,
              extra_manifest={"description": DESCRIPTION, "recorrible": ok, "kill_boxes_uu": kill_boxes_uu()})
    size_mb = round(sum(f.stat().st_size for f in out.rglob("*") if f.is_file()) / (1024 * 1024), 2)
    update_index(name, 0, ok, size_mb, DESCRIPTION)
    idx = np.minimum(2 * np.arange(top.shape[0]), model.land.shape[0] - 1)
    land = model.land[np.ix_(idx, idx)]
    return {"name": name, "ok": ok, "size_mb": size_mb, "time_s": round(time.time() - t0, 1), "top_max": float(top.max()),
            "walkable_share": float(seen[land].sum() / max(int(land.sum()), 1))}


def main() -> None:
    parser = argparse.ArgumentParser(description="Genera un mapa a partir de una foto.")
    parser.add_argument("--image", type=Path, required=True)
    parser.add_argument("--name", default=DEFAULT_NAME)
    parser.add_argument("--size", type=float, default=SIZE_M, help="alto de la imagen en el volumen (m)")
    parser.add_argument("--relief", type=float, default=RELIEF_M, help="amplitud del brillo (m)")
    parser.add_argument("--focus", type=parse_quad, default=FOCUS, help="u,v,radio_u,radio_v del ovalo de interes")
    parser.add_argument("--start", type=parse_point, help="u,v del inicio en la imagen")
    parser.add_argument("--end", type=parse_point, help="u,v del final en la imagen")
    args = parser.parse_args()
    r = build(args.image, args.name, args.size, args.relief, args.focus, args.start, args.end)
    print(f"{r['name']}: {'OK' if r['ok'] else 'NO VALIDO'} {r['time_s']} s, {r['size_mb']} MB; cima {r['top_max']:.1f} m; "
          f"final {'SI' if r['ok'] else 'NO'} a pie desde el inicio; {r['walkable_share'] * 100:.0f} % de la isla alcanzable")


if __name__ == "__main__":
    main()

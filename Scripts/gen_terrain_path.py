"""Genera un mapa "camino primero" (Docs/Diseno_Terreno_CaminoPrimero.md) en
Scripts/terrain_volumes/Variants/<nombre>/ (trozos TNTM2, manifest, vistas) y lo pone el primero
en Variants/index.json (el desplegable de ATN_MapVariantLoader en LVL_MapVariants).

    uv run --with numpy --with scipy --with pillow --with scikit-image python Scripts/gen_terrain_path.py
"""

from __future__ import annotations

import argparse
import json
import time
from pathlib import Path

from gen_terrain_volume import build_all, global_standable, ground_level, walk, world_index, zone_map
from terrain_path.layout import GRID
from terrain_path.model import PathModel, walkable
from terrain_path.style import C01_STYLE
from terrain_vol.export import global_top, write_map
from terrain_vol.mesh import z_levels

VARIANTS = Path(__file__).resolve().parent / "terrain_volumes" / "Variants"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--seed", type=int, default=60001)
    args = parser.parse_args()
    t0 = time.time()
    style = C01_STYLE
    model = PathModel(args.seed, style)
    chunks = build_all(model, grid=GRID)
    standable = walkable(global_standable(chunks, grid=GRID), model.grid.height[1:-1, 1:-1], z_levels())
    s_ij, e_ij = world_index(model.start), world_index(model.end)
    start = (*s_ij, ground_level(standable, *s_ij))
    ok = bool(start[2] >= 0 and walk(standable, start)[e_ij].any())
    top = global_top(chunks, grid=GRID)
    out = VARIANTS / style.name
    write_map(out, style.name, args.seed, chunks, (*model.start, float(top[s_ij])), (*model.end, float(top[e_ij])),
              zone_map(model, chunks, grid=GRID), model.route.points, style=style,
              extra_manifest={"description": style.description, "recorrible": ok}, grid=GRID)
    index_path = VARIANTS / "index.json"
    index = json.loads(index_path.read_text(encoding="utf-8")) if index_path.exists() else []
    index = [e for e in index if e["name"] != style.name]
    size_mb = sum(f.stat().st_size for f in out.rglob("*") if f.is_file()) / (1024 * 1024)
    index.insert(0, {"name": style.name, "seed": args.seed, "description": style.description,
                     "recorrible": ok, "size_mb": round(size_mb, 2)})
    index_path.write_text(json.dumps(index, indent=1, ensure_ascii=False), encoding="utf-8")
    g = model.plan.graph
    print(f"{style.name}: {'OK' if ok else 'NO RECORRIBLE'} {time.time() - t0:.1f}s principal {g.main.length:.0f} m, "
          f"{len(g.loops())} lazos, {len(model.plan.crossings)} cruces, {len(model.plan.hill_tunnels)} tuneles de cerro, "
          f"{len(model.river.islands) if model.river else 0} islas, {len(model.castles)} castillos, {size_mb:.1f} MB")


if __name__ == "__main__":
    main()

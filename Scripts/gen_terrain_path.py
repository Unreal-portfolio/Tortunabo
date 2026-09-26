"""Genera mapas "camino primero" (Docs/Diseno_Terreno_CaminoPrimero.md) en
Scripts/terrain_volumes/Variants/<nombre>/ (trozos TNTM2, manifest, vistas) y los pone al
principio de Variants/index.json (el desplegable de ATN_MapVariantLoader en LVL_MapVariants).

    uv run --with numpy --with scipy --with pillow --with scikit-image python Scripts/gen_terrain_path.py
        C01 (por defecto)
    ... gen_terrain_path.py --catalog [C05_muchos_lazos ...]
        el catalogo de 30 (terrain_path/variants.py), o solo los nombres dados

Cada mapa se comprueba: se llega a pie (o saltando) del inicio al final, se alcanzan todos los
lazos y no se pisa el fondo de vistas. Si no, se reintenta con otra semilla (hasta 3 veces).
"""

from __future__ import annotations

import argparse
import json
import time
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path

import numpy as np

from gen_terrain_volume import build_all, global_standable, ground_level, walk, world_index, zone_map
from terrain_path.layout import GRID
from terrain_path.canyon import kill_boxes_uu
from terrain_path.model import PathModel, walkable
from terrain_path.style import C01_SEED, C01_STYLE, PathStyle
from terrain_path.variants import PATH_VARIANTS
from terrain_vol.export import global_top, write_map
from terrain_vol.mesh import z_levels

VARIANTS = Path(__file__).resolve().parent / "terrain_volumes" / "Variants"
RESEED_STEP = 1000
MAX_TRIES = 3


def check(model: PathModel, chunks) -> dict:
    """Recorrido real sobre la malla: final, lazos y vistas (celdas del fondo alcanzadas)."""
    standable = walkable(global_standable(chunks, grid=GRID), model.grid.height[1:-1, 1:-1], z_levels())
    standable = model.remove_deadly(standable, z_levels())          # caer al barranco es morir
    s_ij, e_ij = world_index(model.start), world_index(model.end)
    start = (*s_ij, ground_level(standable, *s_ij))
    seen = walk(standable, start) if start[2] >= 0 else np.zeros_like(standable)
    flat = seen.any(axis=2)
    loops_ok = 0
    for loop in model.plan.graph.loops():
        i, j = world_index(loop.point_at(loop.length / 2.0))
        loops_ok += bool(flat[i - 2:i + 3, j - 2:j + 3].any())
    vista = int((flat & (model.region[1:-1, 1:-1] == 2)).sum())
    ok = bool(seen[e_ij].any()) and loops_ok == len(model.plan.graph.loops()) and vista == 0
    return {"ok": ok, "loops_ok": loops_ok, "vista": vista}


def build_one(name: str, seed: int, style: PathStyle, description: str) -> dict:
    t0 = time.time()
    for attempt in range(MAX_TRIES):
        used = seed + attempt * RESEED_STEP
        model = PathModel(used, style)
        chunks = build_all(model, grid=GRID)
        result = check(model, chunks)
        if result["ok"]:
            break
    top = global_top(chunks, grid=GRID)
    s_ij, e_ij = world_index(model.start), world_index(model.end)
    out = VARIANTS / name
    write_map(out, name, used, chunks, (*model.start, float(top[s_ij])), (*model.end, float(top[e_ij])),
              zone_map(model, chunks, grid=GRID), model.route.points, style=style,
              extra_manifest={"description": description, "recorrible": result["ok"],
                              "kill_boxes_uu": kill_boxes_uu(model.canyon) if model.canyon else []}, grid=GRID)
    g = model.plan.graph
    return {"name": name, "seed": used, "description": description, "ok": result["ok"],
            "size_mb": round(sum(f.stat().st_size for f in out.rglob("*") if f.is_file()) / (1024 * 1024), 2),
            "time_s": round(time.time() - t0, 1), "length": round(g.main.length), "loops": len(g.loops()),
            "crossings": len(model.plan.crossings), "tunnels": len(model.plan.hill_tunnels),
            "arches": len(model.arch_ranges), "canyon": model.canyon.mode if model.canyon else "no", "islands": len(model.river.islands) if model.river else 0,
            "vista": result["vista"], "loops_ok": result["loops_ok"]}


def update_index(results: list[dict]) -> None:
    index_path = VARIANTS / "index.json"
    index = json.loads(index_path.read_text(encoding="utf-8")) if index_path.exists() else []
    fresh = {r["name"]: {"name": r["name"], "seed": r["seed"], "description": r["description"],
                         "recorrible": r["ok"], "size_mb": r["size_mb"]} for r in results}
    old = {e["name"]: e for e in index}
    old.update(fresh)
    camino = sorted((e for n, e in old.items() if n.startswith("C") and n[1:3].isdigit()), key=lambda e: e["name"])
    rest = [e for n, e in old.items() if not (n.startswith("C") and n[1:3].isdigit())]
    index_path.write_text(json.dumps(camino + rest, indent=1, ensure_ascii=False), encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("names", nargs="*", help="con --catalog: solo estos nombres")
    parser.add_argument("--catalog", action="store_true", help="genera el catalogo C02-C31")
    parser.add_argument("--seed", type=int, default=C01_SEED, help="semilla de C01")
    parser.add_argument("--workers", type=int, default=3)
    args = parser.parse_args()
    if not args.catalog:
        jobs = [(C01_STYLE.name, args.seed, C01_STYLE, C01_STYLE.description)]
    else:
        wanted = set(args.names)
        jobs = [(v.name, v.seed, v.style, v.description) for v in PATH_VARIANTS if not wanted or v.name in wanted]
    results = []
    with ProcessPoolExecutor(max_workers=max(1, min(args.workers, len(jobs)))) as pool:
        futures = {pool.submit(build_one, *job): job[0] for job in jobs}
        for future in as_completed(futures):
            try:
                r = future.result()
            except Exception as exc:                        # noqa: BLE001 (se informa y sigue)
                print(f"{futures[future]}: ERROR {type(exc).__name__}: {exc}", flush=True)
                continue
            results.append(r)
            print(f"{r['name']}: {'OK' if r['ok'] else 'NO VALIDO'} {r['time_s']}s semilla {r['seed']} "
                  f"principal {r['length']} m, {r['loops']} lazos ({r['loops_ok']} alcanzados), "
                  f"{r['crossings']} cruces, barranco {r['canyon']}, {r['arches']} arcos, {r['tunnels']} tuneles, {r['islands']} islas, "
                  f"vistas pisadas {r['vista']}, {r['size_mb']} MB", flush=True)
    update_index(results)
    bad = [r["name"] for r in results if not r["ok"]]
    print(f"{len(results)} mapas, {sum(r['size_mb'] for r in results):.1f} MB; no validos: {bad or 'ninguno'}")


if __name__ == "__main__":
    main()

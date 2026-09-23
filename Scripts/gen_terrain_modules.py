"""Genera la libreria de modulos de terreno: heightfields de 200 m x 200 m por topologia.

Se ejecuta FUERA del editor:
    uv run --with numpy --with pillow python Scripts/gen_terrain_modules.py [--count 100]
        [--out <carpeta>] [--thumb <px>]

Escribe en Scripts/terrain_modules/ (o en --out, para muestras de prueba):
    <Topologia>/M_<Topologia>_<NN>.png   heightfield de 16 bits (101 x 101), fuente de verdad
    manifest.json                         modulos con topologia, semilla, arcos y plazas
    preview_<Topologia>.png               hoja de contactos sombreada, para revisar a ojo

Los PNG los importa Scripts/import_terrain_modules.py (dentro del editor) como
UTN_TerrainModuleAsset + BP_M_*. Semilla fija => salida identica byte a byte.

Modelo de un modulo (alturas en metros, Z = 0 es el suelo de las salidas):
  - Borde canonico: los cuatro lados comparten el MISMO perfil simetrico (cresta de
    CREST_M con una boca de suelo en el centro). Cualquier modulo casa con cualquier otro
    con cualquier rotacion. Un lado sin salida se cierra con una rampa por dentro.
  - Pasillo principal: polilineas del centro a cada salida, con serpenteo que se apaga
    junto al borde; ancho variable; plazas (zonas llanas para puzzles) en el cruce y a lo
    largo.
  - Bifurcacion: un tramo del pasillo se abre alrededor de una isla (roca alta o loma
    escalable) y vuelve a juntarse; dos canales de anchura distinta.
  - Atajo: brazo curvo a ras de suelo entre dos salidas, con la misma anchura relativa y
    el mismo talud que el pasillo, para que se lea como parte del camino.
  - Ruta alta: sube por el talud, recorre la meseta con esquinas redondeadas y cruza el
    pasillo por un arco de roca natural (se camina por arriba y se pasa por debajo).
  - Arco decorativo: arco de roca de pared a pared sobre el pasillo, para pasar por debajo.
    Los arcos no caben en un heightfield: se exportan al manifest y el tile los construye
    como malla (TNTerrainModule::BuildArchMesh).
  - Paredes: talud corto (no escalable) hasta la meseta, con colinas suaves encima.
    Variante "calzada": el exterior baja bajo el agua y el pasillo queda como una cresta.
  - Agujeros: pozos en las plazas (fuera del carril central) y en la meseta.
  - Validacion: borde canonico exacto, arcos apoyados y todas las salidas accesibles a pie;
    un diseno que falla se descarta y se prueba la semilla siguiente.
"""

from __future__ import annotations

import argparse
import json

import numpy as np
from PIL import Image

from terrain_gen.core import *  # noqa: F401,F403
from terrain_gen.design import compose_module, module_biomes
from terrain_gen.field import FIELD_PATTERNS
from terrain_gen.output import hillshade, write_contact_sheet
from terrain_gen.styles import STYLES


def library_jobs(count: int, field_count: int) -> list[dict]:
    """Modulos a generar: count por topologia (todos los lados cresta) y field_count por
    patron de campo (explanada de arena y mar con islas, ver terrain_gen.field)."""
    jobs = []
    for topology, exits in TOPOLOGIES.items():
        for n in range(count):
            biome, secondary = module_biomes(n, count)
            jobs.append({"name": f"M_{topology}_{n + 1:02d}", "folder": topology, "topology": topology,
                         "exits": exits, "biome": biome, "secondary": secondary,
                         "edges": {side: "crest" for side in SIDES}})
    for kind, biome, folder in (("open", "sand", "Open"), ("water", "water", "Water")):
        for pattern, open_sides in FIELD_PATTERNS.items():
            for n in range(field_count):
                jobs.append({"name": f"M_{folder}_{pattern}_{n + 1:02d}", "folder": folder, "topology": "Cross",
                             "exits": SIDES, "biome": biome, "secondary": biome,
                             "edges": {side: (kind if side in open_sides else "crest") for side in SIDES}})
    return jobs


def main() -> None:
    parser = argparse.ArgumentParser(description="Genera la libreria de modulos de terreno.")
    parser.add_argument("--count", type=int, default=100, help="modulos por topologia")
    parser.add_argument("--field-count", type=int, default=12, help="modulos por patron de campo (arena/agua)")
    parser.add_argument("--out", type=Path, default=OUTPUT_DIR)
    parser.add_argument("--thumb", type=int, default=120, help="lado de cada miniatura de la hoja de contactos, en px")
    args = parser.parse_args()

    manifest = {
        "size_uu": SIZE_M * UU_PER_M,
        "resolution": RES,
        "height_scale_uu": HEIGHT_SCALE_UU,
        "height_zero": HEIGHT_ZERO,
        "modules": [],
    }
    rejected = 0
    thumbs: dict[str, list[np.ndarray]] = {}
    for index, job in enumerate(library_jobs(args.count, args.field_count)):
        name, exits, edges = job["name"], job["exits"], job["edges"]
        folder = args.out / job["folder"]
        folder.mkdir(parents=True, exist_ok=True)
        # Un diseno que deja alguna salida sin acceso a pie se descarta y se prueba la
        # semilla siguiente; la semilla final queda en el manifest para reproducirlo.
        for attempt in range(8):
            seed = BASE_SEED + index * 16 + attempt
            heights, stats, bridges, flat_areas, monoliths, mask = compose_module(
                seed, exits, job["biome"], job["secondary"], edges)
            meters = (heights.astype(np.float64) - HEIGHT_ZERO) / UNITS_PER_M
            # La fusion con el borde puede dejar sin apoyo un arco validado antes de
            # fundir: tambien se descarta.
            if walkable_exits_connected(meters, exits) and bridge_problem(meters, bridges) is None:
                break
            rejected += 1
        else:
            raise AssertionError(f"{name}: ninguna semilla da un diseno accesible y con arcos apoyados")
        check_border(heights, edges, name)
        check_bridges(heights, bridges, name)
        Image.fromarray(heights).save(folder / f"{name}.png")
        Image.fromarray(mask).save(folder / f"{name}_mask.png")
        thumbs.setdefault(job["folder"], []).append(
            hillshade(heights, bridges, monoliths, mask, job["biome"], job["secondary"]))
        slopes = slope_degrees(heights)
        manifest["modules"].append({
            "name": name,
            "topology": job["topology"],
            "folder": job["folder"],
            "edges": [edges[side] for side in SIDES],
            "seed": seed,
            "file": f"{job['folder']}/{name}.png",
            "mask_file": f"{job['folder']}/{name}_mask.png",
            "min_m": round(float(meters.min()), 2),
            "max_m": round(float(meters.max()), 2),
            "slope_p99_deg": round(float(np.percentile(slopes, 99)), 1),
            "bridges": [{"x_m": b.x, "y_m": b.y, "yaw_deg": b.yaw_deg, "length_m": b.length_m,
                         "width_m": b.width_m, "deck_m": b.deck_m, "kind": b.kind,
                         "thickness_m": TUNNEL_THICKNESS_M if b.kind == "tunnel" else BRIDGE_THICKNESS_M}
                        for b in bridges],
            "monoliths": [{"x_m": m.x, "y_m": m.y, "base_m": m.base_m, "radius_m": m.radius_m,
                           "height_m": m.height_m, "yaw_deg": m.yaw_deg, "lean_deg": m.lean_deg}
                          for m in monoliths],
            "flat_areas": flat_areas,
            **stats,
        })

    for folder_name, folder_thumbs in thumbs.items():
        write_contact_sheet(args.out / f"preview_{folder_name}.png", folder_thumbs, size=args.thumb)
        print(f"{folder_name}: {len(folder_thumbs)} modulos")

    (args.out / "manifest.json").write_text(json.dumps(manifest, indent=1), encoding="utf-8")
    mods = manifest["modules"]
    print(f"total {len(mods)} modulos; cota [{min(m['min_m'] for m in mods)}, {max(m['max_m'] for m in mods)}] m; "
          f"pendiente p99 max {max(m['slope_p99_deg'] for m in mods)} grados; "
          f"atajos {sum(m['shortcut'] for m in mods)}; rutas altas {sum(m['upper_route'] for m in mods)}; "
          f"puentes {sum(b['kind'] == 'bridge' for m in mods for b in m['bridges'])}; "
          f"arcos {sum(m['arch'] for m in mods)}; tuneles {sum(m['tunnel'] for m in mods)} "
          f"(con rampa {sum(m['tunnel_ramp'] for m in mods)}); "
          f"acantilados {sum(m['cliff'] for m in mods)}; plazas hundidas {sum(m['sunken'] for m in mods)}; "
          f"laberintos {sum(m['maze'] for m in mods)}; "
          f"puertas " + ", ".join(f"{g} {sum(m['gate'] == g for m in mods)}" for g in ("gorge", "flare", "arch")) + "; "
          f"bifurcaciones {sum(m['fork'] != 'none' for m in mods)} (lomas {sum(m['fork'] == 'low' for m in mods)}); "
          f"disenos descartados {rejected}; "
          f"biomas " + ", ".join(f"{b} {sum(m['biome'] == b and m['secondary_biome'] == b for m in mods)}" for b in BIOMES)
          + f", mixtos {sum(m['biome'] != m['secondary_biome'] for m in mods)}; "
          f"estilos " + ", ".join(f"{n} {sum(m['style'] == n for m in mods)}" for n in (*STYLES, "flats", "islands")))


if __name__ == "__main__":
    main()

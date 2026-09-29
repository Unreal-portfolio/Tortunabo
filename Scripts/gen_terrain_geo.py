"""Genera el mapa de una region real (GeoRegion de terrain_geo/presets.py) en
Scripts/terrain_volumes/Variants/<nombre>/ (trozos TNTM2, manifest, vistas y CREDITS.txt) y lo añade a index.json.

    uv run --with numpy --with scipy --with pillow --with scikit-image --with pyproj python Scripts/gen_terrain_geo.py \
        --preset L10_japon_fuji [--refetch] [--list] [--sheet]      (--sheet necesita --with matplotlib)

La primera vez descarga el MDE y la costa (con cache en Saved/terrain_geo_cache/) y guarda los rasters del volumen
en Scripts/terrain_geo/data/<nombre>_dem.png y _mask.png; despues se regenera sin red a partir de ellos. La
exageracion vertical se calibra por la pendiente objetivo del preset (3-6x por defecto) y el mapa se valida
(terrain_vol/validate.py): inicio a pie hasta el final, sin islas inalcanzables salvo las marcadas, presupuesto
de malla y costuras entre trozos.
"""

from __future__ import annotations

import argparse
import json
import time
from pathlib import Path

import numpy as np

from gen_terrain_volume import build_all, world_index, zone_map
from terrain_geo.build import VARIANTS, deepest_dry, dir_size_mb, farthest_reachable, kill_boxes_uu, load_rasters, \
    save_rasters, update_index, world_point, write_credits
from terrain_geo.geomodel import GeoModel, calibrate
from terrain_geo.presets import PRESETS, GeoPreset
from terrain_vol.export import global_top, write_map
from terrain_vol.sheet import render_sheet, sheet_subtitle
from terrain_vol.validate import evaluate_map, reachable

SHEETS = Path(__file__).resolve().parents[1] / "Docs" / "Mapas"


def prepare_rasters(preset: GeoPreset, refetch: bool = False) -> tuple[np.ndarray, np.ndarray]:
    """MDE (m reales) y cobertura del volumen: los guardados o, si no hay (o refetch), descargados y guardados."""
    region = preset.region
    stored = None if refetch else load_rasters(region.name)
    if stored is not None:
        return stored
    dem = region.fetch_dem()
    save_rasters(region.name, dem, region.coverage(dem))
    return load_rasters(region.name)


def build_model(preset: GeoPreset, refetch: bool = False) -> tuple[GeoModel, dict]:
    region = preset.region
    dem, coverage = prepare_rasters(preset, refetch)
    cal = calibrate(region, dem, coverage, preset.target_slope_deg, preset.exaggeration_bounds, preset.params)
    model = GeoModel(region, dem, coverage, cal.k, preset.seed, preset.params, cal.z_range)
    proj = region.game_projection()
    info = {"projection": proj.kind, "proj4": proj.proj4, "bbox": list(region.bounds()), "edge": region.edge,
            "ground_m_per_game_m": round(cal.ground_m_per_game_m, 2), "exaggeration": round(cal.exaggeration, 3),
            "k": cal.k, "exaggeration_capped": cal.capped, "z_range": [cal.z_range.z_min_m, cal.z_range.levels,
                                                                        cal.z_range.step_m]}
    return model, info


def game_ij(model: GeoModel, lonlat: tuple[float, float]) -> tuple[int, int]:
    return world_index(model.to_game(*lonlat))


def build_geo(key: str, name: str | None = None, refetch: bool = False, sheet: bool = False) -> dict:
    t0 = time.time()
    preset = PRESETS[key]
    model, info = build_model(preset, refetch)
    name = name or preset.region.name
    grid = preset.region.grid
    chunks = build_all(model, grid=grid)
    top = global_top(chunks, grid=grid)
    s_ij = game_ij(model, preset.start) if preset.start else deepest_dry(top)
    e_ij = game_ij(model, preset.end) if preset.end else farthest_reachable(top, reachable(top, s_ij), s_ij)
    marked = tuple(game_ij(model, p) for p in preset.unreachable_ok)
    start_xy, end_xy = world_point(s_ij), world_point(e_ij)
    out = VARIANTS / name
    manifest = {"description": preset.description, "mode": preset.mode, "kill_boxes_uu": kill_boxes_uu(grid),
                "geo": info, "unreachable_ok_uu": [[v * 100.0 for v in world_point(p)] for p in marked]}
    write_map(out, name, preset.seed, chunks, (*start_xy, float(top[s_ij])), (*end_xy, float(top[e_ij])),
              zone_map(model, chunks, grid=grid), np.linspace(start_xy, end_xy, 300), style=None,
              extra_manifest=manifest, grid=grid)
    write_credits(out, preset.region.credits((f"Exageración vertical efectiva {info['exaggeration']}x "
                                              f"(calibrada a {preset.target_slope_deg} grados en el p90 de la tierra).",)))
    report = evaluate_map(top, out, s_ij, e_ij, marked=marked, min_island_m2=preset.region.min_island_m2 or 25.0)
    manifest_path = out / "manifest.json"
    data = json.loads(manifest_path.read_text(encoding="utf-8"))
    data["recorrible"] = report["ok"]
    manifest_path.write_text(json.dumps(data, indent=1), encoding="utf-8")
    size_mb = dir_size_mb(out)
    update_index(name, preset.seed, report["ok"], size_mb, preset.description, {"mode": preset.mode})
    if sheet:
        scale = (f"1 m de juego = {info['ground_m_per_game_m']:.0f} m reales; exageración "
                 f"{info['exaggeration']:.2f}x".replace(".", ","))
        render_sheet(top, SHEETS / f"{name}.png", f"{name} ({preset.mode})",
                     sheet_subtitle(preset.description, grid, report, size_mb, scale), s_ij, e_ij)
    return {"name": name, "size_mb": size_mb, "time_s": round(time.time() - t0, 1), "top_max": float(top.max()),
            "start": start_xy, "end": end_xy, **info, **report}


def summary(r: dict) -> str:
    b = r["budget"]
    return (f"{r['name']}: {'OK' if r['ok'] else 'NO VALIDO'} {r['time_s']} s, {r['size_mb']} MB, {b['triangles']} tri "
            f"({b['tri_per_km2']}/km2, tope {b['budget_triangles']}); exageracion {r['exaggeration']}x, "
            f"1 m = {r['ground_m_per_game_m']} m; cima {r['top_max']:.1f} m; final a pie {'SI' if r['end_reached'] else 'NO'}; "
            f"{r['walkable_share'] * 100:.0f} % de la tierra alcanzable; pendiente p50/p90 {r['slope_deg']['p50']:.0f}/"
            f"{r['slope_deg']['p90']:.0f} grados; islas sin marcar inalcanzables {len(r['unreachable_islands'])}; "
            f"costuras {'OK' if r['seams']['ok'] else r['seams']['cracks']}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Genera el mapa de una region real (terrain_geo/presets.py).")
    parser.add_argument("--preset", choices=sorted(PRESETS))
    parser.add_argument("--name", help="nombre de la variante (por defecto el de la region)")
    parser.add_argument("--refetch", action="store_true", help="vuelve a descargar el MDE y la costa")
    parser.add_argument("--list", action="store_true", help="lista los presets")
    parser.add_argument("--sheet", action="store_true", help="dibuja la lamina en Docs/Mapas/<nombre>.png (matplotlib)")
    args = parser.parse_args()
    if args.list or not args.preset:
        for key, preset in PRESETS.items():
            print(f"{key:24} {preset.mode:8} {preset.description}")
        return
    print(summary(build_geo(args.preset, args.name, args.refetch, args.sheet)))


if __name__ == "__main__":
    main()

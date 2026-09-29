"""Escribe un ShapeMap: trozos TNTM2, manifest, vistas, CREDITS, validacion, index.json y (opcional) lamina."""

from __future__ import annotations

import json
import time
from pathlib import Path

import numpy as np

from gen_terrain_volume import build_all, zone_map
from terrain_geo.build import VARIANTS, dir_size_mb, kill_boxes_uu, update_index, write_credits
from terrain_vol.export import global_top, write_map
from terrain_vol.validate import evaluate_map

from .model import ShapeMap

SHEETS = Path(__file__).resolve().parents[2] / "Docs" / "Mapas"
CREDITS = ("Mapa paramétrico de Tortunabo, sin datos geográficos externos: lo genera el script a partir de "
           "los parámetros y la semilla del manifest (clave generator).\n")


def evaluate(shape: ShapeMap, top: np.ndarray, out: Path | None) -> dict:
    """Validadores comunes: inicio, puntos requeridos a pie, pasarelas con ancho y pendiente, islas, malla."""
    c = shape.canvas
    s_ij = c.to_ij(*shape.start)
    e_ij = c.to_ij(*shape.end) if shape.end else None
    names = list(shape.required)
    required = tuple(c.to_ij(*shape.required[k]) for k in names)
    corridors = tuple((c.to_ij(*a), c.to_ij(*b)) for a, b in shape.corridors)
    marked = tuple(c.to_ij(*p) for p in shape.unreachable_ok)
    report = evaluate_map(top, out, s_ij, e_ij, required=required, corridors=corridors, marked=marked,
                          min_width_m=shape.corridor_width_m, max_slope_deg=shape.corridor_slope_deg)
    report["required_names_missed"] = [k for k, p in zip(names, required) if p in report["required_missed"]]
    return report


def _manifest(shape: ShapeMap) -> dict:
    c = shape.canvas
    zr = shape.model.z_range

    def uu(p):
        return [round(v * 100.0, 1) for v in c.to_world(*p)]

    return {"description": shape.description, "mode": shape.mode, "kill_boxes_uu": kill_boxes_uu(c.grid),
            "generator": shape.params, "z_range": [zr.z_min_m, zr.levels, zr.step_m],
            "bridges": [{"a_uu": uu(b.a), "b_uu": uu(b.b), "width_m": b.width_m, "length_m": round(b.length, 2),
                         "slope_deg": round(b.slope_deg(), 1)} for b in shape.model.bridges],
            "points_uu": {k: uu(p) for k, p in shape.required.items()},
            "unreachable_ok_uu": [uu(p) for p in shape.unreachable_ok]}


def write_shape_map(shape: ShapeMap, sheet: bool = False, register: bool = True, variants: Path = VARIANTS,
                    sheets: Path = SHEETS) -> dict:
    """Genera, valida y escribe el mapa en variants/<nombre>; register lo añade a index.json (solo si variants
    es la carpeta del juego) y sheet dibuja la lamina en sheets/<nombre>.png."""
    t0 = time.time()
    c = shape.canvas
    chunks = build_all(shape.model, grid=c.grid)
    top = global_top(chunks, grid=c.grid)
    end = shape.end or shape.start
    start_xy, end_xy = c.to_world(*shape.start), c.to_world(*end)
    s_ij, e_ij = c.to_ij(*shape.start), c.to_ij(*end)
    out = variants / shape.name
    points = [c.to_world(*p) for pair in shape.corridors for p in pair] or [start_xy, end_xy]
    write_map(out, shape.name, shape.seed, chunks, (*start_xy, float(top[s_ij])), (*end_xy, float(top[e_ij])),
              zone_map(shape.model, chunks, grid=c.grid), np.array(points), style=None,
              extra_manifest=_manifest(shape), grid=c.grid)
    write_credits(out, CREDITS)
    report = evaluate(shape, top, out)
    manifest_path = out / "manifest.json"
    data = json.loads(manifest_path.read_text(encoding="utf-8"))
    data["recorrible"] = report["ok"]
    manifest_path.write_text(json.dumps(data, indent=1), encoding="utf-8")
    size_mb = dir_size_mb(out)
    if register and variants == VARIANTS:
        update_index(shape.name, shape.seed, report["ok"], size_mb, shape.description, {"mode": shape.mode})
    result = {"name": shape.name, "size_mb": size_mb, "time_s": round(time.time() - t0, 1),
              "top_max": float(top.max()), "top": top, **report}
    if sheet:
        from terrain_vol.sheet import render_sheet, sheet_subtitle
        marks = {k: c.to_ij(*p) for k, p in shape.required.items()}
        subtitle = sheet_subtitle(shape.description, c.grid, report, size_mb, f"{len(shape.model.bridges)} puentes naturales")
        result["sheet_bytes"] = render_sheet(top, sheets / f"{shape.name}.png", f"{shape.name} ({shape.mode})", subtitle,
                                             s_ij, c.to_ij(*shape.end) if shape.end else None, marks)
    return result


def summary(r: dict) -> str:
    b = r["budget"]
    return (f"{r['name']}: {'OK' if r['ok'] else 'NO VALIDO'} {r['time_s']} s, {r['size_mb']} MB, {b['triangles']} tri "
            f"(tope {b['budget_triangles']}); cima {r['top_max']:.1f} m; {r['walkable_share'] * 100:.0f} % alcanzable; "
            f"puntos sin llegar {r['required_names_missed']}; pasarelas fallidas {len(r['corridors_failed'])}; "
            f"islas inalcanzables {len(r['unreachable_islands'])}; costuras {'OK' if r['seams']['ok'] else r['seams']['cracks']}")

"""Genera un pais entero miniaturizado (terrain_geo/countries.py) en Scripts/terrain_volumes/Variants/<id>/ con la
adaptacion de su modo, lo valida, lo añade a index.json y dibuja su lamina con el contorno real superpuesto.

    uv run --with numpy --with scipy --with pillow --with scikit-image --with pyproj --with matplotlib \
        python Scripts/gen_terrain_country.py L10_japon_v2 [I01_filipinas_v2 ...] [--no-register] [--no-sheet]

Rally: calzada tallada del extremo Sur al Norte (o los puntos del preset), >= 2 puentes (estrechos reales o valles
con rio tallado) y, donde los haya, tuneles bajo montaña real con su lazo (tunel + calzada vieja). La validez es la
de terrain_geo/validity.py: calzada de 12 m a 12 grados como mucho de punta a punta; el tunel solo es obligatorio
donde la calzada no pasa sin el; el lazo que sale roto se descarta (se regenera sin su rama) y se cuenta. TcT:
terrazas y puentes entre islas. Si el mapa pasa del presupuesto de triangulos del modo, se reintenta con un 20 %
menos de trozos.
La primera vez descarga MDE y fronteras (cache en Saved/terrain_geo_cache/); los rasters quedan en terrain_geo/data.
"""

from __future__ import annotations

import argparse
import dataclasses
import json
import time
from pathlib import Path

import numpy as np

from terrain_geo.build import VARIANTS, dir_size_mb, load_rasters, save_rasters, update_index, write_credits
from terrain_geo.countries import COUNTRIES
from terrain_geo.country import (CHUNKS, E01_EXAGGERATION, E01_K, CountryModel, CountryPreset, Road, carve_road,
                                 e01_calibration, endpoint, fit_bridges_to_road, plan_bridges, plan_region, route_road,
                                 snap_inland)
from terrain_geo.rally import RallyDensity, carve_river, check_tunnel, plan_rally
from terrain_geo.region import GeoRegion
from terrain_geo.validity import (Discard, RallyChecks, loop_ok, new_discards, rally_verdict, road_breaks, road_reach,
                                  sustained_grade_deg)
from terrain_vol.export import write_map
from terrain_vol.layout import CELL_M, CELL_SAMPLES, MAP_MIN_M, UU_PER_M, WATER_M
from terrain_vol.mesh import build_chunk
from terrain_vol.validate import MeshBudget, corridor_ok, evaluate_map

SHEETS = Path(__file__).resolve().parents[1] / "Docs" / "Mapas"
DATA = Path(__file__).resolve().parent / "terrain_geo" / "data"


# ── Rasters ─────────────────────────────────────────────────────────────────────
def rasters(region: GeoRegion) -> tuple[np.ndarray, np.ndarray]:
    """MDE y cobertura guardados si son de esta misma proyeccion; si no, descargados y guardados."""
    proj = region.game_projection()
    signature = {"grid": region.grid, "fit": list(region.fit or ()), "rotation": round(region.rotation_deg, 6),
                 "scale": round(proj.scale, 3), "squash": region.squash_e}
    sig_path = DATA / f"{region.name}_geo.json"
    stored = load_rasters(region.name)
    if stored is not None and sig_path.exists() and json.loads(sig_path.read_text(encoding="utf-8")) == signature:
        return stored
    dem = region.fetch_dem()
    save_rasters(region.name, dem, region.coverage(dem))
    sig_path.write_text(json.dumps(signature), encoding="utf-8")
    return load_rasters(region.name)


# ── Montaje ─────────────────────────────────────────────────────────────────────
def rect_top(chunks: dict, region: GeoRegion) -> tuple[np.ndarray, tuple[int, int]]:
    """Cota superior del rectangulo de trozos y el indice (i, j) de la rejilla global de su esquina."""
    c0, r0, cols, rows = region.rect()
    step = CELL_SAMPLES - 1
    top = np.full((rows * step + 1, cols * step + 1), WATER_M - 6.0)
    for (col, row), chunk in chunks.items():
        i0, j0 = (row - r0) * step, (col - c0) * step
        top[i0:i0 + CELL_SAMPLES, j0:j0 + CELL_SAMPLES] = chunk.top
    return top, (r0 * step, c0 * step)


def to_rect(xy, corner) -> tuple[int, int]:
    return int(round(xy[0] - MAP_MIN_M)) - corner[0], int(round(xy[1] - MAP_MIN_M)) - corner[1]


def rect_kill_box(region: GeoRegion) -> list[dict]:
    c0, r0, cols, rows = region.rect()
    x0, y0 = MAP_MIN_M + r0 * CELL_M, MAP_MIN_M + c0 * CELL_M
    z0, z1 = -10.0, WATER_M + 0.2
    return [{"center": [(x0 + rows * 50.0) * UU_PER_M, (y0 + cols * 50.0) * UU_PER_M, 0.5 * (z0 + z1) * UU_PER_M],
             "extent": [rows * 50.0 * UU_PER_M, cols * 50.0 * UU_PER_M, 0.5 * (z1 - z0) * UU_PER_M], "yaw": 0.0}]


def write_rect_preview(out: Path, top: np.ndarray) -> None:
    from PIL import Image
    gx, gy = np.gradient(top)
    light = np.clip((gx * 0.5 - gy * 0.35 + 1.0) / np.sqrt(gx * gx + gy * gy + 1.0) * 0.8, 0.0, 1.0)
    rgb = np.array((0.85, 0.70, 0.46)) * (0.35 + 0.65 * light)[..., None]
    rgb = np.where((top < WATER_M)[..., None], rgb * 0.25 + np.array([0.12, 0.35, 0.65]) * 0.75, rgb)
    image = Image.fromarray((np.clip(rgb, 0, 1) * 255).astype(np.uint8)[::-1])
    image.save(out / "preview.png")
    image.save(out / "preview_debug.png")


def outlines(region: GeoRegion) -> list[np.ndarray]:
    proj = region.game_projection()
    lines = []
    for polygon in region.country_polygons:
        ring = polygon[0]
        if region.bbox is not None:
            lon_min, lat_min, lon_max, lat_max = region.bbox
            if ring[:, 0].max() < lon_min or ring[:, 0].min() > lon_max or ring[:, 1].max() < lat_min or ring[:, 1].min() > lat_max:
                continue
        X, Y = proj.to_game(ring[:, 0], ring[:, 1])
        lines.append(np.stack([X, Y], axis=1))
    return lines


# ── Generacion ──────────────────────────────────────────────────────────────────
def build_model(preset: CountryPreset, max_chunks: int | None, discard: Discard = Discard()):
    region = plan_region(preset, max_chunks)
    dem, coverage = rasters(region)
    cal = e01_calibration(region, dem, coverage, preset.params)
    model = CountryModel(region, dem, coverage, cal.k, preset.seed, preset.params, cal.z_range, preset.terrace())
    named, marked = plan_bridges(model, preset)
    model.bridges = tuple(b for _, b in named)
    proj = region.game_projection()
    start = tuple(float(v) for v in proj.to_game(*preset.start)) if preset.start else endpoint(model, north=False)
    end = tuple(float(v) for v in proj.to_game(*preset.end)) if preset.end else endpoint(model, north=True)
    start, end = snap_inland(model, start), snap_inland(model, end)
    plan = None
    if not preset.is_rally:
        land_bridges(model)
    if preset.is_rally:
        road = route_road(model, start, end, list(model.bridges), preset.road_grade_deg)
        plan = plan_rally(model, road, discard=discard)
        model.bridges = tuple(fit_bridges_to_road(list(model.bridges), plan.main)) + tuple(plan.decks)
        carve = np.ones(len(plan.main.path), dtype=bool)
        for (a, b), tunnel in zip(plan.loops, plan.tunnels):
            carve[a:b + 1] = ~tunnel.covered[:b + 1 - a]
        roads = [Road(plan.main.path, plan.main.profile, carve), *plan.branches]
        carve_road(model, roads, preset.road_width_m, preset.road_shoulder_m)
        for centre, tangent in plan.rivers:
            carve_river(model, centre, tangent)
        settle_decks(model, plan)
        model.density = RallyDensity(model, plan, preset.seed)
        links = [((b.a[1], b.a[0]), (b.b[1], b.b[0])) for b in model.bridges]
        marked = unlinked_islands(model, links, preset.min_island_m2)       # los rios tallados pueden partir la tierra
    return region, cal, model, named, marked, start, end, plan


def land_bridges(model: CountryModel, pad_m: float = 5.0, blend_m: float = 6.0) -> None:
    """TcT: cada puente a la cota media de sus dos orillas, con un rellano a esa cota en cada estribo (las terrazas
    dejan orillas a cotas distintas y el puente saldria empinado)."""
    from terrain_vol.density import smooth
    axis = MAP_MIN_M + (np.arange(model.height.shape[0]) + 0.5) * 0.5
    bridges = []
    for b in model.bridges:
        h = lambda X, Y: float(model.ground_height(np.array([X]), np.array([Y]))[0])     # noqa: E731
        z = 0.5 * (h(b.a[1], b.a[0]) + h(b.b[1], b.b[0]))
        for (e, n) in (b.a, b.b):
            i0, i1 = np.searchsorted(axis, [n - 20, n + 20])
            j0, j1 = np.searchsorted(axis, [e - 20, e + 20])
            X, Y = np.meshgrid(axis[i0:i1], axis[j0:j1], indexing="ij")
            w = 1.0 - smooth(pad_m, pad_m + blend_m, np.hypot(X - n, Y - e))
            block = model.height[i0:i1, j0:j1]
            land = block > WATER_M
            model.height[i0:i1, j0:j1] = np.where(land, block + (z - block) * w, block)
        bridges.append(dataclasses.replace(b, z_a=z, z_b=z))
    model.bridges = tuple(bridges)


def unlinked_islands(model: CountryModel, links, min_area_m2: float) -> list[tuple[float, float]]:
    """Masas de tierra (>= min_area_m2) que no se unen al territorio principal por los enlaces dados ((X, Y), (X, Y))."""
    from scipy import ndimage
    from terrain_geo.country import LAND_M, raster_index, raster_point
    land = model.height > LAND_M
    labels, count = ndimage.label(land)
    if count == 0:
        return []
    areas = ndimage.sum(land, labels, index=np.arange(1, count + 1)) * 0.25
    parent = list(range(count + 1))

    def root(k: int) -> int:
        while parent[k] != k:
            k = parent[k]
        return k

    def label_near(X: float, Y: float) -> int:
        i, j = raster_index(X, Y)
        win = labels[max(0, i - 12):i + 13, max(0, j - 12):j + 13]
        vals = win[win > 0]
        return int(np.bincount(vals).argmax()) if len(vals) else 0

    for a, b in links:
        la, lb = label_near(*a), label_near(*b)
        if la and lb:
            parent[root(la)] = root(lb)
    main = root(int(np.argmax(areas)) + 1)
    out = []
    for k in range(1, count + 1):
        if areas[k - 1] >= min_area_m2 and root(k) != main:
            depth = ndimage.distance_transform_edt(labels == k)
            i, j = np.unravel_index(int(np.argmax(depth)), depth.shape)
            out.append(raster_point(int(i), int(j)))
    return out


def settle_decks(model: CountryModel, plan) -> None:
    """Los puentes (de estrechos y de valle) toman la cota final de la calzada tallada en sus extremos (sin escalon
    al subir al puente)."""
    def h(X: float, Y: float) -> float:
        return float(model.ground_height(np.array([X]), np.array([Y]))[0])

    model.bridges = tuple(dataclasses.replace(b, z_a=h(b.a[1], b.a[0]), z_b=h(b.b[1], b.b[0])) for b in model.bridges)
    plan.decks = list(model.bridges[len(model.bridges) - len(plan.decks):])


def rally_checks(model: CountryModel, plan, top: np.ndarray, corner, start, end) -> RallyChecks:
    """Comprobaciones de la regla de validez de Rally (terrain_geo/validity.py) sobre el mapa ya voxelizado."""
    s_ij, e_ij = to_rect(start, corner), to_rect(end, corner)
    tunnels = tuple(check_tunnel(model.density, t) for t in plan.tunnels)
    links = [(to_rect(t.pts[0], corner), to_rect(t.pts[-1], corner)) for t, c in zip(plan.tunnels, tunnels) if c["ok"]]
    is_open, linked = road_reach(top, s_ij, e_ij, links)
    loops = tuple(loop_ok(top, branch.path - np.array(corner)) for branch in plan.branches)
    bridges = sum(1 for b in model.bridges if _on_road(b, plan))          # los de valle estan en model.bridges
    return RallyChecks(is_open, linked, tunnels, loops, bridges, plan.discarded_loops, plan.discarded_tunnels)


def road_profile(plan, top: np.ndarray, corner) -> tuple[np.ndarray, np.ndarray]:
    """(distancia, cota) en m de la calzada principal sobre el mapa final: la cota superior y, bajo roca, el suelo
    del tunel."""
    path = plan.main.path
    ij = np.clip(path - np.array(corner), 0, np.array(top.shape) - 1)
    z = top[ij[:, 0], ij[:, 1]].astype(np.float64)
    for (a, b), tunnel in zip(plan.loops, plan.tunnels):
        n = b + 1 - a
        z[a:b + 1] = np.where(tunnel.covered[:n], tunnel.floor[:n], z[a:b + 1])
    arc = np.concatenate([[0.0], np.cumsum(np.hypot(*np.diff(path, axis=0).T))])
    return arc, z


def evaluate_country(preset, named, marked, start, end, plan, top, corner, out, rally: RallyChecks | None) -> dict:
    s_ij, e_ij = to_rect(start, corner), to_rect(end, corner)
    marked_ij = tuple(to_rect(p, corner) for p in marked)
    report = evaluate_map(top, out, s_ij, e_ij if preset.is_rally else None, marked=marked_ij,
                          min_island_m2=preset.min_island_m2)
    checks: dict = {}
    lost = report["unreachable_islands"]
    if lost and all(isl["area_m2"] < 0.02 * top.size for isl in lost):
        # Islas pequeñas sin puente (la costa real las deja sueltas): se marcan como inalcanzables a proposito.
        extra = tuple(isl["center"] for isl in lost)
        report = evaluate_map(top, out, s_ij, e_ij if preset.is_rally else None, marked=marked_ij + extra,
                              min_island_m2=preset.min_island_m2)
        checks["islas_marcadas_al_validar"] = len(extra)
        marked.extend((MAP_MIN_M + corner[0] + i, MAP_MIN_M + corner[1] + j) for i, j in extra)
    if rally is not None:
        verdict = rally_verdict(report["ok"], rally)
        if not verdict["calzada"]:
            verdict["cortes"] = [(round(MAP_MIN_M + corner[0] + i), round(MAP_MIN_M + corner[1] + j))
                                 for i, j in road_breaks(top, plan.main.path - np.array(corner))]
        verdict["calzada_pendiente_max_deg"] = round(float(sustained_grade_deg(*road_profile(plan, top, corner)).max()), 1)
        report["ok"] = verdict.pop("ok")
        checks.update(verdict)
    else:
        failed = [name for name, b in named
                  if not corridor_ok(top, *[to_rect((p[1], p[0]), corner) for p in b.inner_points(2.0)], 3.0, 20.0)]
        checks["puentes_fallidos"] = failed
        report["ok"] = bool(report["ok"] and not failed)
    report["checks"] = checks
    return report


def _on_road(bridge, plan) -> bool:
    pts = plan.main.path + MAP_MIN_M
    d = min(np.hypot(pts[:, 0] - bridge.a[1], pts[:, 1] - bridge.a[0]).min(),
            np.hypot(pts[:, 0] - bridge.b[1], pts[:, 1] - bridge.b[0]).min())
    return bool(d < bridge.width_m)


def build_fitting(preset: CountryPreset, budget: MeshBudget, attempts: int = 6):
    """Modelo y trozos del pais dentro del presupuesto de triangulos (un 20 % menos de trozos cada vez que se pasa)
    y, en Rally, sin lazos rotos ni tuneles fallidos: lo que falla se descarta y se regenera sin ello."""
    max_chunks = preset.max_chunks or CHUNKS[preset.mode]
    discard = Discard()
    for _attempt in range(attempts):
        built = build_model(preset, max_chunks, discard)
        region, model, plan = built[0], built[2], built[7]
        chunks = {cell: build_chunk(model, *cell) for cell in region.cells()}
        top, corner = rect_top(chunks, region)
        if sum(len(c.triangles) for c in chunks.values()) > budget.max_triangles:
            max_chunks, discard, rally = max(2, int(len(chunks) * 0.8)), Discard(), None
            continue
        rally = rally_checks(model, plan, top, corner, built[5], built[6]) if plan is not None else None
        more = new_discards(rally, plan.loop_ids, plan.tunnel_ids) if rally is not None else Discard()
        if not more:
            break
        discard = discard.merged(more)
    if rally is None and plan is not None:                    # se agotaron los intentos pasado de presupuesto
        rally = rally_checks(model, plan, top, corner, built[5], built[6])
    return built, chunks, top, corner, rally


def geo_info(region: GeoRegion, cal) -> dict:
    proj = region.game_projection()
    _, _, cols, rows = region.rect()
    return {"projection": proj.kind, "proj4": proj.proj4, "rotation_deg": round(region.rotation_deg, 2),
            "squash_e": region.squash_e, "ground_m_per_game_m": round(cal.ground_m_per_game_m, 1),
            "exaggeration": round(cal.exaggeration, 2), "k": cal.k, "exaggeration_capped": cal.capped,
            "cells": [cols, rows], "z_range": [cal.z_range.z_min_m, cal.z_range.levels, cal.z_range.step_m]}


def geo_counts(report: dict, plan, named) -> dict:
    """Lo que se documenta de cada mapa junto a la exageracion: pendientes, tuneles, puentes y lazos."""
    checks, slope = report["checks"], report["slope_deg"]
    out = {"slope_deg": {"p50": round(slope["p50"], 1), "p90": round(slope["p90"], 1)}, "bridges": len(named),
           "tunnels": 0, "loops": 0, "discarded_loops": 0}
    if plan is not None:
        out.update({"bridges": checks["puentes_en_calzada"], "tunnels": len(plan.tunnels), "loops": len(plan.branches),
                    "discarded_loops": plan.discarded_loops, "discarded_tunnels": plan.discarded_tunnels,
                    "road_max_slope_deg": checks["calzada_pendiente_max_deg"]})
    return out


def write_variant(out: Path, key: str, preset: CountryPreset, region: GeoRegion, chunks: dict, top, corner, start, end,
                  plan, named, marked, info: dict) -> None:
    """Trozos, manifest, vista previa y CREDITS de la variante."""
    manifest = {"description": preset.description, "mode": preset.mode, "lang": preset.lang, "geo": info,
                "kill_boxes_uu": rect_kill_box(region),
                "bridges": [{"name": n, "a_uu": [b.a[1] * 100, b.a[0] * 100], "b_uu": [b.b[1] * 100, b.b[0] * 100],
                             "width_m": b.width_m} for n, b in named],
                "unreachable_ok_uu": [[p[0] * 100, p[1] * 100] for p in marked]}
    if plan is not None:
        manifest["road_uu"] = [[round((MAP_MIN_M + float(i)) * 100), round((MAP_MIN_M + float(j)) * 100), round(float(z) * 100)]
                               for (i, j), z in zip(plan.main.path[::10], plan.main.profile[::10])]
        manifest["tunnels"] = [{"from_uu": (t.pts[0] * 100).tolist(), "to_uu": (t.pts[-1] * 100).tolist(),
                                "covered_m": t.length} for t in plan.tunnels]
        manifest["valley_bridges_uu"] = [[(d.a[1] + d.b[1]) * 50, (d.a[0] + d.b[0]) * 50] for d in plan.decks]
    grid = region.grid
    size = grid * (CELL_SAMPLES - 1) + 1
    zones = {z: (np.ones((size, size)) if z == "cliffs" else np.zeros((size, size)))
             for z in ("cliffs", "canyon", "marsh", "algae", "beach")}
    route = (plan.main.path + MAP_MIN_M) if plan is not None else np.array([start, end])
    write_map(out, key, preset.seed, chunks, (*start, float(top[to_rect(start, corner)])),
              (*end, float(top[to_rect(end, corner)])), zones, route, style=None, extra_manifest=manifest, grid=grid)
    write_rect_preview(out, top)
    rule = (f"Exageración vertical: escala de E01 ({E01_K * 1000:.2f} m de juego por km de cota), con tope en la "
            f"exageración efectiva de E01 ({E01_EXAGGERATION}x) y en el techo del volumen: aquí {info['exaggeration']}x.")
    write_credits(out, region.credits((rule,)))


def build_country(key: str, register: bool = True, sheet: bool = True, variants: Path = VARIANTS) -> dict:
    t0 = time.time()
    preset = COUNTRIES[key]
    built, chunks, top, corner, rally = build_fitting(preset, MeshBudget.for_mode(preset.mode, 6))
    region, cal, _model, named, marked, start, end, plan = built
    np.savez_compressed(Path(__file__).resolve().parents[1] / "Saved" / f"_top_{key}.npz", top=top, corner=corner,
                        path=plan.main.path if plan is not None else np.zeros((0, 2)))          # para depurar
    out = variants / key
    info = geo_info(region, cal)
    write_variant(out, key, preset, region, chunks, top, corner, start, end, plan, named, marked, info)
    report = evaluate_country(preset, named, marked, start, end, plan, top, corner, out, rally)
    info.update(geo_counts(report, plan, named))
    data = json.loads((out / "manifest.json").read_text(encoding="utf-8"))
    data.update({"recorrible": report["ok"], "geo": info, "validation": dict(report["checks"]),
                 "unreachable_ok_uu": [[p[0] * 100, p[1] * 100] for p in marked]})
    (out / "manifest.json").write_text(json.dumps(data, indent=1, default=_plain), encoding="utf-8")
    size_mb = dir_size_mb(out)
    if register and variants == VARIANTS:
        update_index(key, preset.seed, report["ok"], size_mb, preset.description, {"mode": preset.mode, "lang": preset.lang})
    result = {"name": key, "size_mb": size_mb, "time_s": round(time.time() - t0, 1), "info": info, **report,
              "cols_rows": tuple(info["cells"]), "bridges": [n for n, _ in named], "marked": len(marked)}
    if sheet:
        result["sheet_bytes"] = draw_sheet(key, preset, region, top, corner, start, end, plan, named, report, info, size_mb)
    return result


def sheet_subtitle(preset: CountryPreset, report: dict, info: dict, size_mb: float) -> str:
    """Subtitulo de la lamina: tamaño, escala, exageracion, pendientes, obras de la calzada, malla y veredicto."""
    def thousands(n: int) -> str:
        return f"{n:,}".replace(",", ".")

    b = report["budget"]
    cols, rows = info["cells"]
    slope = info["slope_deg"]
    parts = [f"{cols * 100} x {rows * 100} m ({cols} x {rows} trozos)",
             f"1 m de juego = {info['ground_m_per_game_m'] / 1000:.2f} km", f"exageración {info['exaggeration']}x",
             f"pendiente p50/p90 {slope['p50']:.0f}°/{slope['p90']:.0f}°"]
    if preset.is_rally:
        parts += [f"calzada máx. {info['road_max_slope_deg']:.0f}°",
                  f"{info['bridges']} puentes, {info['tunnels']} túneles, {info['loops']} lazos "
                  f"({info['discarded_loops']} descartados)"]
    else:
        parts.append(f"{info['bridges']} puentes")
    parts += [f"{thousands(b['triangles'])} triángulos (tope {thousands(b['budget_triangles'])})", f"{size_mb:.2f} MB",
              "válido" if report["ok"] else "NO válido"]
    return f"{preset.description}\n" + "; ".join(parts) + "."


def draw_sheet(key, preset, region, top, corner, start, end, plan, named, report, info, size_mb) -> int:
    from terrain_vol.sheet import render_sheet
    origin = (MAP_MIN_M + corner[0], MAP_MIN_M + corner[1])
    marks = {}
    for k, (name, b) in enumerate(named):
        marks[f"P{k + 1} {name}"] = to_rect(((b.a[1] + b.b[1]) / 2, (b.a[0] + b.b[0]) / 2), corner)
    route = profile = None
    if plan is not None:
        for k, d in enumerate(plan.decks):
            marks[f"PV{k + 1} puente de valle"] = to_rect(((d.a[1] + d.b[1]) / 2, (d.a[0] + d.b[0]) / 2), corner)
        for k, t in enumerate(plan.tunnels):
            marks[f"T{k + 1} túnel {t.length:.0f} m"] = to_rect(t.pts[len(t.pts) // 2], corner)
        for k, branch in enumerate(plan.branches):
            marks[f"L{k + 1} lazo"] = to_rect(branch.path[len(branch.path) // 2] + MAP_MIN_M, corner)
        route = [plan.main.path - np.array(corner)] + [b.path - np.array(corner) for b in plan.branches]
        arc, z = road_profile(plan, top, corner)
        profile = (arc, z - WATER_M)
    return render_sheet(top, SHEETS / f"{key}.png", f"{key} ({preset.mode}, {preset.lang})",
                        sheet_subtitle(preset, report, info, size_mb), to_rect(start, corner),
                        to_rect(end, corner) if preset.is_rally else None, marks, origin=origin,
                        outlines=outlines(region), route=route, profile=profile)


def _plain(value):
    """numpy -> tipos de JSON."""
    return value.item() if hasattr(value, "item") else (value.tolist() if hasattr(value, "tolist") else str(value))


def register_existing(key: str) -> None:
    """Añade a index.json (leer, modificar, escribir) una variante ya generada, con su manifest."""
    preset = COUNTRIES[key]
    manifest = json.loads((VARIANTS / key / "manifest.json").read_text(encoding="utf-8"))
    update_index(key, preset.seed, bool(manifest["recorrible"]), dir_size_mb(VARIANTS / key), preset.description,
                 {"mode": preset.mode, "lang": preset.lang})


def summary(r: dict) -> str:
    b = r["budget"]
    return (f"{r['name']}: {'OK' if r['ok'] else 'NO VALIDO'} {r['time_s']} s; {r['cols_rows']} trozos; 1 m = "
            f"{r['info']['ground_m_per_game_m']} m; exag {r['info']['exaggeration']}x; {b['triangles']} tri; {r['size_mb']} MB; "
            f"final {r['end_reached']}; islas sin marcar {len(r['unreachable_islands'])}, marcadas {r['marked']}; "
            f"puentes {r['bridges']}; checks {r['checks']}; costuras {r['seams']['ok']}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Genera paises enteros miniaturizados.")
    parser.add_argument("keys", nargs="*", help=f"ids ({', '.join(COUNTRIES)})")
    parser.add_argument("--no-register", action="store_true")
    parser.add_argument("--no-sheet", action="store_true")
    parser.add_argument("--register-only", action="store_true", help="solo añade a index.json lo ya generado")
    args = parser.parse_args()
    if args.register_only:
        for key in args.keys:
            register_existing(key)
        return
    unknown = [key for key in args.keys if key not in COUNTRIES]
    if unknown:
        parser.error(f"ids desconocidos: {', '.join(unknown)}")
    failed = []
    for key in args.keys or list(COUNTRIES):
        try:
            print(summary(build_country(key, not args.no_register, not args.no_sheet)), flush=True)
        except RuntimeError as error:                 # sin ruta del inicio al final: el pais no sale, los demas si
            failed.append(key)
            print(f"{key}: NO GENERADO: {error}", flush=True)
    if failed:
        raise SystemExit(f"sin generar: {', '.join(failed)}")


if __name__ == "__main__":
    main()

"""Genera mapas de MUESTRA del sistema volumetrico: mismo generador que Mapa01, distinta
semilla y MapStyle (terrain_vol.variants.VARIANTS), para que los disenadores elijan un
look. No son mapas para usar tal cual.

Se ejecuta FUERA del editor, en paralelo (varios procesos):
    uv run --with numpy --with scipy --with pillow --with scikit-image \
        python Scripts/gen_terrain_variants.py [nombres...] [--workers N]

Sin nombres genera las 30 del catalogo; con nombres, solo esas (deben existir en
terrain_vol.variants.VARIANTS). Escribe Scripts/terrain_volumes/Variants/<nombre>/
(Chunks/*.bin en TNTM2, manifest.json con el estilo y la descripcion, preview.png,
preview_debug.png), Scripts/terrain_volumes/Variants/index.json y contact_sheet.png.
"""

from __future__ import annotations

import argparse
import json
import time
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path

from gen_terrain_volume import build_all, global_standable, ground_level, walk, world_index, zone_map
from terrain_vol.density import MapModel
from terrain_vol.export import global_top, write_map
from terrain_vol.variants import ALL_SPECS, VARIANTS, VariantSpec

OUTPUT_ROOT = Path(__file__).resolve().parent / "terrain_volumes" / "Variants"
THUMB = 220


def _build_one(spec: VariantSpec) -> dict:
    t0 = time.time()
    model = MapModel(spec.seed, style=spec.style)
    chunks = build_all(model)
    standable = global_standable(chunks)
    start_ij, end_ij = world_index(model.route.points[0]), world_index(model.route.points[-1])
    start = (*start_ij, ground_level(standable, *start_ij))
    end = (*end_ij, ground_level(standable, *end_ij))
    ok = bool(start[2] >= 0 and end[2] >= 0 and walk(standable, start)[end])
    top = global_top(chunks)
    start_w = (model.route.points[0][0], model.route.points[0][1], float(top[start_ij]))
    end_w = (model.route.points[-1][0], model.route.points[-1][1], float(top[end_ij]))
    out = OUTPUT_ROOT / spec.name
    write_map(out, spec.name, spec.seed, chunks, start_w, end_w, zone_map(model, chunks), model.route.points,
             style=spec.style, extra_manifest={"description": spec.description, "recorrible": ok})
    size_mb = sum(f.stat().st_size for f in out.rglob("*") if f.is_file()) / (1024 * 1024)
    return {
        "name": spec.name, "seed": spec.seed, "description": spec.description, "ok": ok,
        "triangles": sum(len(c.triangles) for c in chunks.values()),
        "tunnels": len(model.maze_tunnels), "arches": len(model.arches) + len(model.bridges),
        "rivers": spec.style.river_count,
        "size_mb": size_mb, "time_s": time.time() - t0,
    }


def _contact_sheet(names: list[str]) -> None:
    from PIL import Image, ImageDraw

    cols, rows = 6, 5
    pad, label_h = 6, 18
    cell_w, cell_h = THUMB + pad, THUMB + pad + label_h
    sheet = Image.new("RGB", (cols * cell_w, rows * cell_h), (24, 24, 24))
    draw = ImageDraw.Draw(sheet)
    for i, name in enumerate(names):
        path = OUTPUT_ROOT / name / "preview.png"
        img = Image.open(path).convert("RGB").resize((THUMB, THUMB)) if path.exists() \
            else Image.new("RGB", (THUMB, THUMB), (60, 20, 20))
        cx, cy = (i % cols) * cell_w, (i // cols) * cell_h
        sheet.paste(img, (cx, cy))
        draw.text((cx + 2, cy + THUMB + 2), name, fill=(230, 230, 230))
    sheet.save(OUTPUT_ROOT / "contact_sheet.png")


def main() -> None:
    parser = argparse.ArgumentParser(description="Genera variantes de muestra del mapa volumetrico.")
    parser.add_argument("names", nargs="*", help="Nombres del catalogo a generar (por defecto, las 30).")
    parser.add_argument("--workers", type=int, default=5)
    args = parser.parse_args()

    by_name = {v.name: v for v in ALL_SPECS}
    unknown = [n for n in args.names if n not in by_name]
    if unknown:
        raise SystemExit(f"nombres no encontrados en el catalogo: {unknown}")
    wanted = [by_name[n] for n in args.names] if args.names else list(VARIANTS)
    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)

    results = []
    t0 = time.time()
    with ProcessPoolExecutor(max_workers=args.workers) as pool:
        futures = {pool.submit(_build_one, spec): spec for spec in wanted}
        for future in as_completed(futures):
            spec = futures[future]
            try:
                r = future.result()
            except Exception as exc:                              # noqa: BLE001 (se informa y se sigue)
                r = {"name": spec.name, "seed": spec.seed, "description": spec.description, "ok": False,
                     "error": str(exc)}
            results.append(r)
            status = "OK" if r.get("ok") else "FALLO"
            extra = f" ERROR: {r['error']}" if "error" in r else ""
            print(f"{r['name']}: {status} {r.get('time_s', 0):.1f}s {r.get('triangles', 0)} tris "
                  f"{r.get('size_mb', 0):.2f} MB tuneles={r.get('tunnels', '-')} puentes={r.get('arches', '-')} "
                  f"rios={r.get('rivers', '-')}{extra}", flush=True)

    # Un indice previo (regenerar solo unos nombres no debe borrar el resto del catalogo).
    index_path = OUTPUT_ROOT / "index.json"
    previous = {e["name"]: e for e in json.loads(index_path.read_text(encoding="utf-8"))} if index_path.exists() else {}
    for r in results:
        previous[r["name"]] = {"name": r["name"], "seed": r["seed"], "description": r["description"],
                               "recorrible": r.get("ok", False), "size_mb": round(r.get("size_mb", 0.0), 2)}
    order = {v.name: i for i, v in enumerate(ALL_SPECS)}
    index = sorted(previous.values(), key=lambda e: order.get(e["name"], 999))
    index_path.write_text(json.dumps(index, indent=1, ensure_ascii=False), encoding="utf-8")
    if any(r["name"] in {v.name for v in VARIANTS} for r in results) and \
            all((OUTPUT_ROOT / v.name / "preview.png").exists() for v in VARIANTS):
        _contact_sheet([v.name for v in VARIANTS])

    total_mb = sum(r.get("size_mb", 0.0) for r in results)
    fails = [r["name"] for r in results if not r.get("ok")]
    print(f"\n{len(results)} variantes generadas en {time.time() - t0:.1f} s; {total_mb:.1f} MB en total.")
    if fails:
        print(f"NO recorribles o con error: {fails}")


if __name__ == "__main__":
    main()

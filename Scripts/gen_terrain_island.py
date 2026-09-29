"""Genera mapas de islas en Scripts/terrain_volumes/Variants/<nombre>/ y los añade a index.json:

  - ficticias parametricas (terrain_shapes/archipelago.py): archipielago de N islas con volcan central, puentes
    naturales tallados entre islas y lagunas, con semilla; o una especificacion del catalogo (--catalog I01);
  - reales (terrain_geo, --preset): una GeoRegion con la costa como borde.

    uv run --with numpy --with scipy --with pillow --with scikit-image python Scripts/gen_terrain_island.py --catalog I01
    ... gen_terrain_island.py --seed 7 --islands 5 --lagoons 1 [--grid 3] [--no-volcano] [--name I_prueba]
    ... gen_terrain_island.py --preset I07_taal          (necesita --with pyproj)
    --sheet dibuja la lamina en Docs/Mapas/<nombre>.png (necesita --with matplotlib); --no-register no toca el indice.
"""

from __future__ import annotations

import argparse

from terrain_shapes.archipelago import ArchipelagoSpec, build_archipelago, philippines_rare, random_archipelago
from terrain_shapes.writer import summary, write_shape_map

CATALOG = {"I01": philippines_rare}


def build_island(spec: ArchipelagoSpec, sheet: bool = False, register: bool = True) -> dict:
    return write_shape_map(build_archipelago(spec), sheet=sheet, register=register)


def main() -> None:
    parser = argparse.ArgumentParser(description="Genera mapas de islas (ficticias o reales).")
    parser.add_argument("--catalog", choices=sorted(CATALOG), help="isla del catalogo de mapas")
    parser.add_argument("--preset", help="isla real: clave de terrain_geo/presets.py")
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--islands", type=int, default=5)
    parser.add_argument("--lagoons", type=int, default=1)
    parser.add_argument("--grid", type=int, default=3)
    parser.add_argument("--no-volcano", action="store_true")
    parser.add_argument("--name")
    parser.add_argument("--sheet", action="store_true")
    parser.add_argument("--no-register", action="store_true")
    args = parser.parse_args()
    if args.preset:
        from gen_terrain_geo import build_geo
        from gen_terrain_geo import summary as geo_summary
        print(geo_summary(build_geo(args.preset, args.name, sheet=args.sheet, register=not args.no_register)))
        return
    if args.catalog:
        spec = CATALOG[args.catalog]()
    else:
        spec = random_archipelago(args.seed, args.islands, args.grid, volcano=not args.no_volcano, lagoons=args.lagoons,
                                  name=args.name)
    print(summary(build_island(spec, args.sheet, not args.no_register)))


if __name__ == "__main__":
    main()

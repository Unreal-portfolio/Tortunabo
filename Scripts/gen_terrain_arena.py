"""Genera arenas abstractas de Todos contra Todos (terrain_shapes/arena.py) en
Scripts/terrain_volumes/Variants/<nombre>/ y las añade a index.json.

    uv run --with numpy --with scipy --with pillow --with scikit-image python Scripts/gen_terrain_arena.py --catalog A01
    ... gen_terrain_arena.py --shape diana --seed 7 --rings "0,8,10;8,24,7;24,40,4;40,56,2" [--moat 4] [--stagger 30]
    ... gen_terrain_arena.py --shape donut --seed 3 [--r-in 25 --r-out 65 --height 4]
    ... gen_terrain_arena.py --shape espiral --seed 3 [--turns 2.5 --pitch 16 --width 12 --top 14]
    ... gen_terrain_arena.py --shape tablero --seed 5 [--cells 5 --size 20 --gap 9]
    --grid N (trozos de 100 m por lado, 2 por defecto), --name, --sheet (lamina en Docs/Mapas, necesita
    --with matplotlib), --no-register (no toca el indice).
"""

from __future__ import annotations

import argparse

from terrain_shapes.arena import (BoardSpec, DianaSpec, DonutSpec, SpiralSpec, build_board, build_diana, build_donut,
                                  build_spiral, diana_a01)
from terrain_shapes.writer import summary, write_shape_map

CATALOG = {
    "A01": lambda: build_diana(diana_a01()),
    "A02": lambda: build_donut(DonutSpec(
        "A02_donut", 3002, 2, description="Dónut (Todos contra Todos): anillo de 130 m con un hueco de agua de 50 m que "
        "cruzan dos puentes naturales; se corre en círculo y caer al agua es la muerte.")),
    "A03": lambda: build_spiral(SpiralSpec(
        "A03_espiral", 3003, 2, description="Espiral (Todos contra Todos): rampa de 2,5 vueltas que sube de +0,8 a +14 m "
        "hacia el centro entre fosos de agua.")),
    "A05": lambda: build_board(BoardSpec(
        "A05_tablero", 3005, 2, description="Tablero de mesetas (Todos contra Todos): 5 x 5 mesetas de 20 m a +3, +6 y "
        "+9 m unidas por rampas naturales de 9 m.")),
}


def parse_rings(text: str) -> tuple[tuple[float, float, float], ...]:
    return tuple(tuple(float(v) for v in ring.split(",")) for ring in text.split(";"))


def build_from_args(args: argparse.Namespace):
    name = args.name or f"A_{args.shape}_{args.seed}"
    if args.shape == "diana":
        rings = parse_rings(args.rings) if args.rings else diana_a01().rings
        return build_diana(DianaSpec(name, args.seed, args.grid, rings, moat_m=args.moat, stagger_deg=args.stagger,
                                     description=f"Diana de {len(rings)} anillos (semilla {args.seed})."))
    if args.shape == "donut":
        return build_donut(DonutSpec(name, args.seed, args.grid, args.r_in, args.r_out, args.height))
    if args.shape == "espiral":
        return build_spiral(SpiralSpec(name, args.seed, args.grid, turns=args.turns, pitch_m=args.pitch,
                                       width_m=args.width, h_top=args.top))
    total = args.cells * args.cells
    counts = (total - 2 * (total // 3), total // 3, total // 3)            # reparto casi a partes iguales
    return build_board(BoardSpec(name, args.seed, args.grid, cells=args.cells, size_m=args.size, gap_m=args.gap,
                                 counts=counts, links=2 * args.cells * (args.cells - 1)))


def main() -> None:
    parser = argparse.ArgumentParser(description="Genera arenas de Todos contra Todos.")
    parser.add_argument("--catalog", choices=sorted(CATALOG))
    parser.add_argument("--shape", choices=("diana", "donut", "espiral", "tablero"), default="diana")
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--grid", type=int, default=2)
    parser.add_argument("--name")
    parser.add_argument("--rings", help='anillos "r_in,r_out,cota;..." en m, del centro hacia fuera')
    parser.add_argument("--moat", type=float, default=0.0, help="foso de agua entre anillos (m); >0 = puentes")
    parser.add_argument("--stagger", type=float, default=0.0, help="giro al azar de las rampas por escalon (grados)")
    parser.add_argument("--r-in", type=float, default=25.0)
    parser.add_argument("--r-out", type=float, default=65.0)
    parser.add_argument("--height", type=float, default=4.0)
    parser.add_argument("--turns", type=float, default=2.5)
    parser.add_argument("--pitch", type=float, default=16.0)
    parser.add_argument("--width", type=float, default=12.0)
    parser.add_argument("--top", type=float, default=14.0)
    parser.add_argument("--cells", type=int, default=5)
    parser.add_argument("--size", type=float, default=20.0)
    parser.add_argument("--gap", type=float, default=9.0)
    parser.add_argument("--sheet", action="store_true")
    parser.add_argument("--no-register", action="store_true")
    args = parser.parse_args()
    shape = CATALOG[args.catalog]() if args.catalog else build_from_args(args)
    print(summary(write_shape_map(shape, sheet=args.sheet, register=not args.no_register)))


if __name__ == "__main__":
    main()

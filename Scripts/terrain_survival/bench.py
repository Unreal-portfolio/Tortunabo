"""Banco de métricas del mapa de Supervivencia: varias semillas y dificultades medidas contra la especificación.
Escribe `<nombre>.json`, `<nombre>.md` y, con --hojas, un PNG por mapa.

    uv run python -m terrain_survival.bench --carpeta <dir con semilla_dificultad.npz> --nombre coop ...
    uv run python -m terrain_survival.bench --algoritmo referencia --semillas 5

Un generador en C++ escribe sus mapas con `SurvivalMap.save` (mismo formato, ver mapa.py) con nombre
`<semilla>_<dificultad>.npz` y el banco los lee con --carpeta. Se ejecuta desde Scripts/."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from . import spec
from .adapters import ADAPTERS
from .mapa import SurvivalMap
from .metrics import difficulty_rank, evaluate, variety


def collect(algorithm: str | None, folder: Path | None, seeds: list[int], difficulties: list[int]) -> list[SurvivalMap]:
    maps = []
    for d in difficulties:
        for s in seeds:
            if folder is not None:
                path = folder / f"{s}_{d}.npz"
                if not path.exists():
                    print(f"falta {path}", file=sys.stderr)
                    continue
                maps.append(SurvivalMap.load(path))
            else:
                maps.append(ADAPTERS[algorithm](s, d))
    return maps


def summarize(maps: list[SurvivalMap]) -> dict:
    reports = [evaluate(m) for m in maps]
    by_difficulty: dict[int, list[SurvivalMap]] = {}
    for m in maps:
        by_difficulty.setdefault(m.difficulty, []).append(m)
    varieties = {d: variety([m.top for m in ms]) for d, ms in by_difficulty.items()}
    valid = [r for r in reports if r["valid"]]
    times = sorted(r["gen_seconds"] for r in reports)
    rank = difficulty_rank(reports)
    verdict = {
        "valid_share": len(valid) / max(len(reports), 1),
        "gen_seconds_median": times[len(times) // 2] if times else None,
        "gen_seconds_max": times[-1] if times else None,
        "difficulty_rank": rank,
        "variety": varieties,
        "difficulty_ok": rank is not None and rank >= spec.MIN_DIFFICULTY_RANK,
        "variety_ok": bool(varieties) and all(v >= spec.MIN_VARIETY for v in varieties.values()),
    }
    verdict["accepted"] = bool(verdict["valid_share"] == 1.0 and verdict["difficulty_ok"] and verdict["variety_ok"])
    return {"reports": reports, "summary": verdict}


def markdown(name: str, result: dict) -> str:
    s = result["summary"]
    lines = [f"## {name}", "",
             f"- Mapas válidos: {s['valid_share']:.0%}",
             f"- Generación: mediana {_fmt(s['gen_seconds_median'], 's')}, máxima {_fmt(s['gen_seconds_max'], 's')} "
             f"(tope {spec.MAX_GEN_SECONDS} s)",
             f"- Dificultad → reto (Spearman): {_fmt(s['difficulty_rank'])} (mínimo {spec.MIN_DIFFICULTY_RANK})",
             f"- Variedad entre semillas: " + ", ".join(f"d{d}={v:.2f}" for d, v in sorted(s["variety"].items()))
             + f" (mínimo {spec.MIN_VARIETY})",
             f"- Aceptado por el banco: {'sí' if s['accepted'] else 'no'}", "",
             "| Semilla | Dif. | Válido | Camino (m) | Camino/recta | Pend. >25° | Fuera del camino | Triáng. | s |",
             "|---|---|---|---|---|---|---|---|---|"]
    for r in result["reports"]:
        lines.append(f"| {r['seed']} | {r['difficulty']} | {'sí' if r['valid'] else 'no'} | {_fmt(r['route_m'])} | "
                     f"{_fmt(r['route_ratio'])} | {_fmt(r['steep_share'])} | {_fmt(r['off_route_share'])} | "
                     f"{r['triangles']} | {r['gen_seconds']:.2f} |")
    return "\n".join(lines) + "\n"


def _fmt(value, unit: str = "") -> str:
    return "—" if value is None else (f"{value:.2f}{unit}" if isinstance(value, float) else f"{value}{unit}")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--algoritmo", choices=sorted(ADAPTERS), help="generador Python a ejecutar")
    ap.add_argument("--carpeta", type=Path, help="mapas .npz ya generados (p. ej. el export de C++)")
    ap.add_argument("--nombre", help="nombre en el informe (por defecto, el algoritmo)")
    ap.add_argument("--semillas", type=int, default=5, help="semillas 1..N")
    ap.add_argument("--dificultades", default=",".join(map(str, spec.DIFFICULTIES)))
    ap.add_argument("--salida", type=Path, default=Path("../Docs/Mapas/Supervivencia"))
    ap.add_argument("--hojas", action="store_true", help="un PNG de vista previa por mapa")
    args = ap.parse_args()
    if bool(args.algoritmo) == bool(args.carpeta):
        ap.error("usa --algoritmo o --carpeta")
    name = args.nombre or args.algoritmo
    maps = collect(args.algoritmo, args.carpeta, list(range(1, args.semillas + 1)),
                   [int(x) for x in args.dificultades.split(",")])
    result = summarize(maps)
    args.salida.mkdir(parents=True, exist_ok=True)
    (args.salida / f"{name}.json").write_text(json.dumps(result, indent=1, ensure_ascii=False), encoding="utf-8")
    (args.salida / f"{name}.md").write_text(markdown(name, result), encoding="utf-8")
    if args.hojas:
        from terrain_vol.sheet import render_sheet
        for m, r in zip(maps, result["reports"]):
            render_sheet(m.top, args.salida / f"{name}_s{m.seed}_d{m.difficulty}.png",
                         f"{name} · semilla {m.seed} · dificultad {m.difficulty}",
                         f"{'válido' if r['valid'] else 'NO válido'} · {m.gen_seconds:.2f} s · {r['triangles']} triángulos",
                         start=m.start, end=m.goal, origin=(0.0, 0.0))
    print((args.salida / f"{name}.md").read_text(encoding="utf-8"))


if __name__ == "__main__":
    main()

"""Catalogo de 30 mapas "camino primero" (C02-C31) para elegir con los disenadores: el mismo
sistema que C01 con otra semilla y, en la mayoria, algun parametro distinto (lazos, cruces,
puentes, anchura, altura de las paredes, relieve, agua...). No son mapas para usar tal cual."""

from __future__ import annotations

from dataclasses import dataclass, replace

from .style import C01_STYLE, PathStyle

BASE_SEED = 61000


@dataclass(frozen=True)
class PathVariant:
    name: str
    seed: int
    style: PathStyle
    description: str


def _v(n: int, slug: str, description: str, **changes) -> PathVariant:
    name = f"C{n:02d}_{slug}"
    style = replace(C01_STYLE, name=name, description=description, **changes)
    return PathVariant(name, BASE_SEED + n, style, description)


PATH_VARIANTS: tuple[PathVariant, ...] = (
    _v(2, "base_a", "Mismos parametros que C01, otra semilla."),
    _v(3, "base_b", "Mismos parametros que C01, otra semilla."),
    _v(4, "base_c", "Mismos parametros que C01, otra semilla."),
    _v(5, "muchos_lazos", "Nueve lazos, tres de ellos dentro de otros: muchas alternativas.",
       loops=9, nested_loops=3),
    _v(6, "pocos_lazos", "Solo cuatro lazos: recorrido mas directo.", loops=4, nested_loops=1),
    _v(7, "sin_anidados", "Siete lazos, todos del camino principal.", nested_loops=0),
    _v(8, "cruces", "Tres cruces con puente fino: los lazos pasan por encima o por debajo.",
       crossings=3),
    _v(9, "muchos_puentes", "Cinco arcos finos sobre el camino y dos cruces.", arches=5, crossings=2),
    _v(10, "sin_puentes", "Sin arcos ni cruces: todo al aire libre.", arches=0, crossings=0),
    _v(11, "tuneles", "Tres tuneles de cerro extra.", extra_hill_tunnels=3),
    _v(12, "camino_ancho", "Camino ancho (semiancho 5-12 m): mas espacio para colocar cosas.",
       width_m=(5.0, 7.0, 12.0)),
    _v(13, "camino_estrecho", "Camino estrecho (semiancho 2,5-6 m): mas encajonado.",
       width_m=(2.5, 3.5, 6.0)),
    _v(14, "paredes_altas", "Paredes altas (acantilado 7-13 m): canones profundos.",
       rim_cliffs_m=(7.0, 13.0), rim_dunes_m=(5.0, 8.0)),
    _v(15, "paredes_bajas", "Paredes bajas: se ve mas paisaje desde el camino.",
       rim_cliffs_m=(3.5, 6.0), rim_dunes_m=(3.0, 4.5), rim_water_m=(3.0, 4.0)),
    _v(16, "cimas_movidas", "Cimas con mucho relieve (lomas de hasta 6 m).", crest_relief_m=6.0),
    _v(17, "cimas_suaves", "Cimas suaves, casi sin lomas.", crest_relief_m=1.0),
    _v(18, "mucho_agua", "Tramo de rio largo y lagos grandes en las vistas.",
       biome_shares=(0.28, 0.32, 0.25, 0.15), vista_pond_m=3.4),
    _v(19, "poca_agua", "Rio corto y pocos lagos: mas arena.",
       biome_shares=(0.4, 0.1, 0.35, 0.15), vista_pond_m=1.6),
    _v(20, "rio_ancho", "Rio ancho con muchas islas.", river_half_width_m=(7.0, 11.0), island_per_100m=9.0),
    _v(21, "mucho_acantilado", "Mas de la mitad del camino entre acantilados.",
       biome_shares=(0.55, 0.15, 0.18, 0.12)),
    _v(22, "mucha_duna", "Mas de la mitad del camino entre dunas.",
       biome_shares=(0.2, 0.15, 0.5, 0.15)),
    _v(23, "largo", "Camino principal largo (850-950 m): mas serpenteo.", main_length_m=(850.0, 950.0)),
    _v(24, "corto", "Camino principal corto (620-720 m).", main_length_m=(620.0, 720.0)),
    _v(25, "lazos_grandes", "Lazos grandes que se alejan mucho del principal.",
       loop_span_m=(80.0, 200.0), loop_reach_m=(50.0, 80.0)),
    _v(26, "lazos_pequenos", "Lazos pequenos y cercanos al principal.",
       loop_span_m=(40.0, 100.0), loop_reach_m=(36.0, 48.0)),
    _v(27, "empinado", "Mas cuestas: cambios de altura frecuentes y rampas del 30 %.",
       steep_chance=0.35),
    _v(28, "dunas_altas", "Vistas con dunas altas y tupidas.", vista_dune_amp_m=(1.8, 4.5)),
    _v(29, "mezcla_a", "Mezcla: mas lazos, cruces, puentes y agua.",
       loops=8, crossings=3, arches=4, biome_shares=(0.3, 0.28, 0.27, 0.15)),
    _v(30, "mezcla_b", "Mezcla: camino ancho, paredes altas y cimas movidas.",
       width_m=(4.5, 6.5, 11.0), rim_cliffs_m=(6.0, 11.0), crest_relief_m=5.0),
    _v(31, "mezcla_c", "Mezcla: pocos lazos grandes, rio ancho y tuneles.",
       loops=5, loop_span_m=(80.0, 180.0), river_half_width_m=(6.0, 10.0), extra_hill_tunnels=2),
)

assert len(PATH_VARIANTS) == 30 and len({v.name for v in PATH_VARIANTS}) == 30

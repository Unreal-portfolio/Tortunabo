"""Catalogo de 30 mapas de muestra: mismo sistema volumetrico (terrain_vol), distinta
semilla y MapStyle, para que los disenadores elijan un look (no son mapas para usar tal
cual). Cubre el espacio de disenio pedido: laberintos densos/abiertos, arbol puro/muchos
lazos, muchos/pocos/ningun tunel, muchos puentes, mucha/poca agua, 0-3 rios al mar, mar
grande/pequeno, dunas altas/planas, acantilados altos/bajos, pasillos anchos/estrechos y
mezclas intermedias. Todas las semillas son distintas de la de Mapa01 (20260925).
"""

from __future__ import annotations

from dataclasses import dataclass

from .style import MapStyle

BASE_SEED = 40000


@dataclass(frozen=True)
class VariantSpec:
    name: str
    seed: int
    style: MapStyle
    description: str


def _v(n: int, name: str, description: str, **kwargs) -> VariantSpec:
    return VariantSpec(name, BASE_SEED + n, MapStyle(name=name, description=description, **kwargs), description)


VARIANTS: tuple[VariantSpec, ...] = (
    _v(1, "01_base",
       "Reparto parecido a Mapa01: referencia para comparar el resto."),
    _v(2, "02_laberinto_denso",
       "Acantilados y canon muy tupidos: pasillos cada 11 m y muchos lazos.",
       cliffs_spacing_m=11.0, cliffs_loop_share=0.35, canyon_maze_spacing_m=10.0, canyon_maze_loop_share=0.4),
    _v(3, "03_laberinto_abierto",
       "Laberinto muy abierto: pasillos cada 24 m, casi sin lazos, facil de leer.",
       cliffs_spacing_m=24.0, cliffs_loop_share=0.05, canyon_maze_spacing_m=24.0, canyon_maze_loop_share=0.05),
    _v(1004, "04_arbol_puro",
       "Sin lazos: el laberinto es un arbol puro, un unico camino bueno por rama.",
       cliffs_loop_share=0.0, canyon_maze_loop_share=0.0),
    _v(5, "05_muchos_lazos",
       "Laberinto con muchisimos lazos: varias rutas alternativas validas.",
       cliffs_loop_share=0.55, canyon_maze_loop_share=0.55, cliffs_level_count=4, canyon_maze_level_count=4),
    _v(6, "06_sin_tuneles",
       "Todo el recorrido al aire libre: sin tuneles excavados en el laberinto.",
       tunnels_cliffs=(0, 0), tunnels_canyon=(0, 0)),
    _v(7, "07_muchos_tuneles",
       "Laberinto muy horadado: muchos tuneles pasantes y varias cuevas.",
       tunnels_cliffs=(6, 3), tunnels_canyon=(9, 3)),
    _v(8, "08_muchos_puentes",
       "Muchos puentes de roca (tunel abajo, tablero andable arriba).",
       arches_cliffs=5, arches_canyon=7),
    _v(9, "09_sin_puentes",
       "Sin puentes de roca: los laberintos se cruzan solo por los pasillos.",
       arches_cliffs=0, arches_canyon=0),
    _v(10, "10_mucha_agua",
        "Zona de agua muy extendida: charcas profundas y crestas mas estrechas.",
        water_share=0.34, cliffs_share=0.20, canyon_share=0.12, dunes_share=0.18, beach_share=0.16,
        pond_depth_m=4.6),
    _v(11, "11_poca_agua",
        "Zona de agua reducida a un paso corto: casi todo tierra firme.",
        water_share=0.08, cliffs_share=0.28, canyon_share=0.18, dunes_share=0.30, beach_share=0.16),
    _v(12, "12_rio_cero",
        "Agua de charcas y crestas, sin rio hasta el mar.",
        river_count=0, water_share=0.24),
    _v(13, "13_un_rio",
        "Un rio serpenteante llega desde la zona de agua hasta el mar.",
        river_count=1, water_share=0.24, island_density=0.4),
    _v(14, "14_dos_rios",
        "Dos rios independientes llegan al mar, con islitas para saltar entre ellos.",
        river_count=2, water_share=0.26, island_density=0.55),
    _v(15, "15_tres_rios",
        "Tres rios llegan al mar: la zona de agua queda muy fragmentada en islitas.",
        river_count=3, water_share=0.30, island_density=0.75, canyon_share=0.12),
    _v(16, "16_mar_grande",
        "Franja de mar y playa muy ancha antes de llegar a tierra firme.",
        beach_share=0.26, sea_margin_m=110.0, water_share=0.16),
    _v(17, "17_mar_pequeno",
        "Playa corta: se llega al mar enseguida tras salir del agua.",
        beach_share=0.08, sea_margin_m=40.0, water_share=0.22),
    _v(18, "18_dunas_altas",
        "Zona de dunas con relieve marcado: crestas de arena bien altas.",
        dunes_share=0.30, dune_amplitude_m=3.2),
    _v(19, "19_dunas_planas",
        "Zona de dunas casi llana: apenas ondula.",
        dunes_share=0.26, dune_amplitude_m=0.35),
    _v(20, "20_acantilados_altos",
        "Acantilados y canon muy altos: paredes imponentes.",
        cliff_height_m=(13.0, 24.0), canyon_height_m=(12.0, 26.0)),
    _v(21, "21_acantilados_bajos",
        "Acantilados y canon bajos: paredes suaves, mas colinas que muros.",
        cliff_height_m=(4.0, 8.0), canyon_height_m=(4.0, 9.0)),
    _v(22, "22_pasillos_anchos",
        "Pasillos del laberinto muy anchos: se cruzan sin apreturas.",
        corridor_width_m=4.6),
    _v(23, "23_pasillos_estrechos",
        "Pasillos del laberinto estrechos: obliga a ir con cuidado.",
        corridor_width_m=2.3),
    _v(24, "24_solo_laberintos",
        "Casi todo acantilados y canon: agua, dunas y playa reducidas al minimo.",
        cliffs_share=0.42, canyon_share=0.34, water_share=0.06, dunes_share=0.06, beach_share=0.12,
        arches_cliffs=3, tunnels_cliffs=(4, 1)),
    _v(1025, "25_solo_agua_y_dunas",
        "Casi sin laberinto: predominan el agua y las dunas de playa.",
        cliffs_share=0.08, canyon_share=0.06, water_share=0.34, dunes_share=0.36, beach_share=0.16,
        river_count=1),
    _v(26, "26_mezcla_equilibrada",
        "Los cinco tramos a partes iguales: ninguno domina el recorrido.",
        cliffs_share=0.20, canyon_share=0.20, water_share=0.20, dunes_share=0.20, beach_share=0.20),
    _v(27, "27_canon_protagonista",
        "El canon ocupa la mayor parte del laberinto; los acantilados son un tramo corto.",
        cliffs_share=0.12, canyon_share=0.32, canyon_maze_spacing_m=13.0, canyon_maze_loop_share=0.3),
    _v(28, "28_acantilados_protagonista",
        "Los acantilados ocupan la mayor parte del laberinto; el canon es un tramo corto.",
        cliffs_share=0.34, canyon_share=0.10, cliffs_spacing_m=13.0, cliffs_loop_share=0.3),
    _v(29, "29_todo_extremo",
        "Laberinto denso, muchos tuneles y puentes, mucha agua con 2 rios, dunas altas: la muestra mas cargada.",
        cliffs_spacing_m=12.0, cliffs_loop_share=0.4, canyon_maze_spacing_m=12.0, canyon_maze_loop_share=0.4,
        tunnels_cliffs=(5, 2), tunnels_canyon=(7, 2), arches_cliffs=4, arches_canyon=5, water_share=0.28,
        river_count=2, island_density=0.6, dune_amplitude_m=3.0, cliff_height_m=(11.0, 21.0)),
    _v(30, "30_minimalista",
        "Laberinto abierto, sin tuneles ni puentes, poca agua, dunas planas: la muestra mas ligera.",
        cliffs_spacing_m=22.0, cliffs_loop_share=0.05, canyon_maze_spacing_m=22.0, canyon_maze_loop_share=0.05,
        tunnels_cliffs=(0, 0), tunnels_canyon=(0, 0), arches_cliffs=1, arches_canyon=1, water_share=0.12,
        river_count=0, dune_amplitude_m=0.6, cliff_height_m=(5.0, 9.0), corridor_width_m=4.0),
)

assert len({v.name for v in VARIANTS}) == len(VARIANTS) == 30, "nombres de variante duplicados o cuenta distinta de 30"
assert len({v.seed for v in VARIANTS}) == 30, "semillas de variante duplicadas"

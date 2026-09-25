"""Estilo parametrico del mapa volumetrico: todo lo que antes eran constantes de diseno
sueltas en route.py/density.py y que conviene variar entre muestras (fracciones de zona,
laberintos, tuneles, puentes, agua, dunas...), reunido en un unico dataclass congelado.

MAPA01_STYLE reproduce el reparto y los parametros de red que ya se sorteaban a mano en
density.py antes de esta refactorizacion: con el mismo seed, la secuencia de numeros
aleatorios que se consume ANTES del corte "extra = default_rng(seed + 7)" es identica
byte a byte (misma ruta, mismos tuneles del canon, mismo dune_angle: ver
test_la_semilla_aprobada_da_el_mismo_mapa). Todo lo que este dataclass anade DESPUES de
ese corte (agua, dunas, puentes, tuneles del laberinto, rugosidad del borde) no formaba
parte de la secuencia original y no puede romper esa huella.

Una fraccion de zona a 0.0 no elimina la zona de la ruta (rompería ZoneField y las redes
que dependen de ella): se sustituye por un suelo minimo (FLOOR_SHARE) y el resto de
fracciones se renormaliza para sumar 1. El resultado es una franja casi invisible (unos
pocos metros de arco) en vez de un crash: "el codigo debe aguantar zonas ausentes".
"""

from __future__ import annotations

from dataclasses import dataclass, field

FLOOR_SHARE = 1e-4   # fraccion minima de una zona "a 0": banda casi invisible, nunca cero real


@dataclass(frozen=True)
class MapStyle:
    name: str = "mapa01"
    description: str = "Recorrido aprobado 2026-09-25: acantilados -> canon -> agua -> dunas -> playa."

    # ── Reparto de la ruta por zona (fraccion de longitud de arco) y alcance lateral (m) ──
    cliffs_share: float = 0.25
    cliffs_reach_m: float = 75.0
    canyon_share: float = 0.16
    canyon_reach_m: float = 40.0
    water_share: float = 0.20        # zona "marsh": dunas inundadas, laguitos, rios al mar
    water_reach_m: float = 85.0
    dunes_share: float = 0.24        # zona "algae": dunas de playa lisas, sin follaje ni verde
    dunes_reach_m: float = 80.0
    beach_share: float = 0.15
    beach_reach_m: float = 95.0

    # ── Laberintos (cliffs y canyon_maze): espaciado Poisson, lazos, niveles, anchura ──
    cliffs_spacing_m: float = 17.0
    cliffs_loop_share: float = 0.15
    cliffs_level_step_m: float = 4.5
    cliffs_level_count: int = 3
    canyon_maze_spacing_m: float = 16.0
    canyon_maze_loop_share: float = 0.2
    canyon_maze_level_count: int = 3
    canyon_maze_reach_m: float = 70.0     # antes CANYON_REACH_M
    corridor_width_m: float = 3.2         # semiancho base de los pasillos de roca (hw)

    # ── Relieve de acantilados (m sobre el suelo del laberinto) ──
    cliff_height_m: tuple[float, float] = (8.0, 17.0)
    canyon_height_m: tuple[float, float] = (7.0, 19.0)

    # ── Tuneles y puentes del laberinto (excavados en la roca) ──
    tunnels_cliffs: tuple[int, int] = (3, 1)      # (pasantes, cuevas)
    tunnels_canyon: tuple[int, int] = (6, 1)
    arches_cliffs: int = 2                        # puentes de roca (tunel + tablero arriba)
    arches_canyon: int = 3

    # ── Agua (zona "marsh"): charcas, rios que llegan al mar, islitas para saltar ──
    pond_depth_m: float = 3.0                     # amplitud del relieve de las charcas (antes 3.0 fijo)
    river_count: int = 0                          # 0-3: rios que serpentean hasta el mar
    river_width_m: float = 7.0
    river_depth_m: float = 2.5
    island_density: float = 0.5                   # 0..1: tamano de las islitas-peldano del rio

    # ── Dunas y transicion a la playa/mar ──
    dune_amplitude_m: float = 1.2                 # amplitud de las dunas de la zona "algae" (antes 1.2 fijo)
    sea_margin_m: float = 70.0                    # anchura de playa antes de que empiece el mar

    # ── Serpenteo de la ruta dentro de los laberintos (amplitud m, longitud de onda m) ──
    route_meander_cliffs_m: tuple[float, float] = (16.0, 60.0)
    route_meander_dunes_m: tuple[float, float] = (14.0, 55.0)

    # ── Borde del mapa: rugosidad de la cresta exterior (siempre activa, nunca 0 puro) ──
    border_roughness_m: float = 10.0

    # ── Prototipo 2026-09-25 (tarde). Todo desactivado por defecto: Mapa01 y las 30 variantes
    # salen igual que antes. ──
    dune_wave_m: tuple[float, float] = (34.0, 46.0)      # longitud de onda de las dos familias de dunas
    dune_rise: float = 0.7                               # fraccion de barlovento (el resto, cara de avalancha)
    dune_amp_range_m: tuple[float, float] = (2.5, 13.0)  # amplitud de las dunas de playa (de zona llana a alta)
    ground_flatten: float = 0.0                          # 0..1: aplana el relieve de fondo fuera de los laberintos
    trails: bool = False                                 # camino principal marcado, trenzado en varios ramales
    trail_strands: tuple[int, int] = (2, 4)              # caminos en cada tramo trenzado (se separan y se juntan)
    trail_half_width_m: float = 2.4
    river_rapids: bool = False                           # islitas alternando de orilla, como un rapido
    land_bridges: tuple[int, int] = (0, 0)               # (acantilados, canon): pasarelas sobre un canon transversal
    landmarks: bool = False                              # salida y meta reconocibles (nido de salida, castillo final)
    human_marks: int = 0                                 # castillos de arena, fosas y zanjas junto a los caminos

    def zones(self) -> tuple[tuple[str, float, float], ...]:
        """(nombre, fraccion normalizada, alcance) para route.build_route/ZoneField. Una
        fraccion pedida a 0 queda como una banda minima (FLOOR_SHARE) tras renormalizar."""
        raw = (
            ("cliffs", self.cliffs_share, self.cliffs_reach_m),
            ("canyon", self.canyon_share, self.canyon_reach_m),
            ("marsh", self.water_share, self.water_reach_m),
            ("algae", self.dunes_share, self.dunes_reach_m),
            ("beach", self.beach_share, self.beach_reach_m),
        )
        floored = tuple((n, max(s, FLOOR_SHARE), r) for n, s, r in raw)
        total = sum(s for _, s, _ in floored)
        return tuple((n, s / total, r) for n, s, r in floored)

    def meander(self) -> dict[str, tuple[float, float]]:
        """Zonas de laberinto en las que la ruta serpentea, si de verdad estan presentes."""
        cfg: dict[str, tuple[float, float]] = {}
        if self.cliffs_share > FLOOR_SHARE:
            cfg["cliffs"] = self.route_meander_cliffs_m
        if self.dunes_share > FLOOR_SHARE:
            cfg["algae"] = self.route_meander_dunes_m
        return cfg


MAPA01_STYLE = MapStyle()

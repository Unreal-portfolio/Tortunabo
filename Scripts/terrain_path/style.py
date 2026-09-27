"""Parametros del mapa "camino primero" (Docs/Diseno_Terreno_CaminoPrimero.md, seccion 5)."""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class PathStyle:
    name: str = "C01_camino"
    description: str = ("Camino primero: un camino, lazos que vuelven, puentes naturales y tuneles, "
                        "rio con islas y final abierto al mar.")
    # Grafo
    main_length_m: tuple[float, float] = (700.0, 900.0)
    path_separation_m: float = 34.0          # distancia minima entre dos caminos que no se unen
    loops: int = 7
    nested_loops: int = 2                    # cuantos de ellos cuelgan de otro lazo
    crossings: tuple[int, int] = (1, 4)      # lazos que cruzan a su padre (rango por mapa)
    bridge_share: float = 0.5                # parte de los cruces con puente fino (el resto, tunel)
    loop_span_m: tuple[float, float] = (40.0, 160.0)
    loop_reach_m: tuple[float, float] = (38.0, 65.0)
    backtrack_chance: float = 0.3            # el lazo sale hacia atras antes de avanzar
    hill_tunnels: int = 2                    # tramos del principal que atraviesan un cerro
    # Anchura (semiancho, m) y cota
    width_m: tuple[float, float, float] = (4.0, 6.5, 10.5)       # minimo, moda, maximo
    river_half_width_m: tuple[float, float] = (5.0, 9.0)
    max_grade: float = 0.2
    steep_grade: float = 0.3
    steep_chance: float = 0.15
    cross_clearance_m: float = 8.5
    # Biomas a lo largo del principal: acantilado, agua, dunas, playa
    biome_shares: tuple[float, float, float, float] = (0.35, 0.2, 0.3, 0.15)
    rim_cliffs_m: tuple[float, float] = (4.0, 9.0)
    rim_water_m: tuple[float, float] = (3.0, 5.0)
    rim_dunes_m: tuple[float, float] = (3.0, 6.0)
    rim_beach_m: tuple[float, float] = (2.5, 4.0)
    wall_angle_deg: tuple[float, float, float, float] = (78.0, 60.0, 64.0, 62.0)
    block_band_m: tuple[float, float] = (1.0, 6.0)    # ancho de lo alto de la pared antes de caer (varia)
    # Rio
    island_per_100m: float = 5.0
    # Fondo de vistas
    vista_dune_wave_m: tuple[float, float] = (15.0, 20.0)
    vista_dune_amp_m: tuple[float, float] = (0.8, 2.6)
    vista_pond_m: float = 2.6
    # Castillos de arena (2026-09-26: fuera; los ponen los disenadores)
    castles: int = 0
    # Anadidos 2026-09-26 (generador de azar aparte: el grafo del mapa no cambia)
    arches: int = 3                          # puentes naturales: arco de roca sobre el camino
    extra_hill_tunnels: int = 1              # tuneles de cerro ademas de los del plan
    crest_relief_m: float = 3.0              # lomas sobre las cimas (no mesetas)
    # v2 (Docs/2026-09-26-Terreno-CaminoPrimero-v2-Design.md)
    hill_amp_m: tuple[float, float] = (1.0, 14.0)     # lomas junto al camino: altura, variable a lo largo
    crest_roughness: float = 1.0             # picos y rugosidad de las lomas
    canyon: str = "deadly"                   # "none" | "deadly" | "walkable"
    canyon_width_m: tuple[float, float] = (20.0, 35.0)
    canyon_depth_m: tuple[float, float] = (8.0, 14.0)
    wall_color_mix: float = 1.0              # mezcla del color de pared segun la inclinacion
    # Escalones de medusa (terrain_path/steps.py): subida que solo se salva rebotando en una medusa
    jump_steps: int = 2
    jump_step_m: float = 4.5
    jump_ledge_m: tuple[float, float] = (12.0, 22.0)
    # Variedad (2026-09-27). Los valores por defecto dejan C01 como estaba.
    biome_order: str = "fixed"               # "fixed" (acantilado, agua, dunas, playa) | "random" (playa al final)
    canyon_count: tuple[int, int] = (1, 1)   # barrancos por mapa (rango)
    canyon_shape: str = "straight"           # "straight" (perpendicular, poco serpenteo) | "free" (angulo y curva al azar)
    lagoon_chance: float = 0.0               # probabilidad de que un tramo de agua sea laguna ancha con islitas
    streams: tuple[int, int] = (0, 0)        # lazos que son un arroyo vadeable (bifurcacion por el agua)


C01_STYLE = PathStyle()
C01_SEED = 60026          # v2: cruce en puente, cruce en tunel y barranco interior (antes 60001, 60013)

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
    crossings: int = 2                       # lazos que cruzan a su padre (puente o tunel)
    loop_span_m: tuple[float, float] = (40.0, 160.0)
    loop_reach_m: tuple[float, float] = (26.0, 60.0)
    backtrack_chance: float = 0.3            # el lazo sale hacia atras antes de avanzar
    hill_tunnels: int = 2                    # tramos del principal que atraviesan un cerro
    # Anchura (semiancho, m) y cota
    width_m: tuple[float, float, float] = (2.5, 4.0, 8.0)       # minimo, moda, maximo
    river_half_width_m: tuple[float, float] = (4.0, 8.0)
    max_grade: float = 0.2
    steep_grade: float = 0.3
    steep_chance: float = 0.15
    cross_clearance_m: float = 8.5
    # Biomas a lo largo del principal: acantilado, agua, dunas, playa
    biome_shares: tuple[float, float, float, float] = (0.35, 0.2, 0.3, 0.15)
    rim_cliffs_m: tuple[float, float] = (4.0, 9.0)
    rim_water_m: tuple[float, float] = (3.0, 5.0)
    rim_dunes_m: tuple[float, float] = (3.0, 6.0)
    rim_beach_m: tuple[float, float] = (0.0, 1.5)
    wall_angle_deg: tuple[float, float, float, float] = (78.0, 55.0, 62.0, 30.0)
    block_band_m: float = 8.0
    # Rio
    island_per_100m: float = 5.0
    # Fondo de vistas
    vista_dune_wave_m: tuple[float, float] = (15.0, 20.0)
    vista_dune_amp_m: tuple[float, float] = (0.8, 2.6)
    vista_pond_m: float = 2.6
    # Castillos de arena
    castles: int = 3


C01_STYLE = PathStyle()

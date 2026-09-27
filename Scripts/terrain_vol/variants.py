"""Prototipo del sistema volumetrico antiguo (P01), usado por tests/test_terrain_proto.py. El
catalogo de 30 muestras y su generador se retiraron el 2026-09-27: los reemplaza el catalogo
"camino primero" (terrain_path/variants.py)."""

from __future__ import annotations

from dataclasses import dataclass

from .style import MapStyle


@dataclass(frozen=True)
class VariantSpec:
    name: str
    seed: int
    style: MapStyle
    description: str


# Prototipos (2026-09-25, tarde): un mapa en el que se prueba la siguiente vuelta del sistema
# antes de sacar variaciones. Van primero en el desplegable de LVL_MapVariants.
PROTOTYPES: tuple[VariantSpec, ...] = (
    VariantSpec("P01_prototipo", 50001, MapStyle(
        name="P01_prototipo",
        description="Prototipo: camino principal marcado y trenzado, pasarelas sobre los canones, "
                    "rios como rapidos, dunas bajas y tupidas, salida y meta reconocibles.",
        cliffs_share=0.24, canyon_share=0.16, water_share=0.24, dunes_share=0.22, beach_share=0.14,
        cliffs_level_count=3, canyon_maze_level_count=3, cliffs_loop_share=0.3, canyon_maze_loop_share=0.3,
        river_count=2, island_density=0.55, river_rapids=True, pond_depth_m=2.6,
        dune_wave_m=(15.0, 20.0), dune_rise=0.62, dune_amp_range_m=(0.8, 2.6), dune_amplitude_m=1.0,
        ground_flatten=0.6, trails=True, trail_strands=(2, 3), land_bridges=(2, 2), landmarks=True,
        human_marks=6),
        "Prototipo: camino principal marcado y trenzado, pasarelas sobre los canones, rios como rapidos, "
        "dunas bajas y tupidas, salida y meta reconocibles."),
)

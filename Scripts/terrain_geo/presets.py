"""Regiones con nombre para los mapas geograficos. Cada entrada es una GeoRegion y los parametros de su mapa;
gen_terrain_geo.py y gen_terrain_island.py las generan con --preset <clave>.

Añadir un mapa del catalogo (Docs/Catalogo-Mapas-*.md) es añadir aqui una entrada: region (caja o pais, borde),
pendiente objetivo de la calibracion y tamaño del volumen (grid, trozos de 100 m).
"""

from __future__ import annotations

from dataclasses import dataclass, field

from .geomodel import ReliefParams
from .layout import SPAIN_REGION
from .region import GeoRegion


@dataclass(frozen=True)
class GeoPreset:
    region: GeoRegion
    description: str
    mode: str                                        # "rally", "tct", "coop", "carrera"
    target_slope_deg: float = 14.0                   # pendiente del p90 de la tierra tras calibrar
    exaggeration_bounds: tuple[float, float] = (3.0, 6.0)
    params: ReliefParams = field(default_factory=ReliefParams)
    start: tuple[float, float] | None = None         # (lon, lat); None = el punto mas lejos de la costa
    end: tuple[float, float] | None = None
    seed: int = 20260929
    unreachable_ok: tuple[tuple[float, float], ...] = ()     # (lon, lat) de islas que no se alcanzan a pie a proposito


SPAIN = SPAIN_REGION           # E01: Web Mercator, 2800 m Mercator por m de juego (terrain_geo/layout.py)

PRESETS: dict[str, GeoPreset] = {
    # L10 (Catalogo de mapas §1 y §4.2, piloto geo de Rally): de la costa de Fuji (bahia de Suruga) al crater.
    # El catalogo pide 1 400 x 2 700 m a 1 m = 12 m; sin decimado (Plan §2.5) no cabe en 1,2 M triangulos, asi
    # que el piloto es la misma ventana en 600 x 600 m (1 m = 72 m). E del catalogo = 0,15 a 12 m por m: aqui
    # se calibra por pendiente con E entre 0,15 y 6 (a esta escala 3-6x pondria el Fuji a 200 m).
    "L10_japon_fuji": GeoPreset(
        GeoRegion("L10_japon_fuji", bbox=(138.52, 35.06, 138.90, 35.42), edge="coast", grid=6, frame_m=24.0),
        "Japón, monte Fuji (Rally, piloto): de la costa de Fuji en la bahía de Suruga al cráter, con el relieve "
        "real calibrado por pendiente (1 m de juego = 72 m, cima a +47 m); el mar es la muerte.",
        mode="rally", target_slope_deg=20.0, exaggeration_bounds=(0.15, 6.0),
        params=ReliefParams(frame_taper_m=45.0, detail_low_m=0.1, detail_high_m=0.6, detail_band_real_m=(300.0, 2500.0)),
        start=(138.68, 35.14), end=(138.7274, 35.3606), seed=2010),
    # Isla real (fuera del catalogo, ejemplo de gen_terrain_island.py --preset): la isla del volcan Taal, en un
    # lago dentro de Luzon, con su lago de crater dentro. El agua es la del lago Taal (water_level_m = 6 m reales).
    "I07_taal": GeoPreset(
        GeoRegion("I07_taal", bbox=(120.955, 13.975, 121.035, 14.045), edge="coast", grid=3, frame_m=14.0,
                  water_level_m=6.0, min_island_m2=150.0, keep_largest=True),
        "Filipinas, isla del volcán Taal: una isla en un lago dentro de Luzón, con su propio lago de cráter; "
        "Todos contra Todos en el anillo del cráter, caer al agua es la muerte.",
        mode="tct", target_slope_deg=22.0, exaggeration_bounds=(0.5, 6.0),
        params=ReliefParams(land_base_m=0.8, max_slope=1.2, detail_low_m=0.1, detail_high_m=0.4,
                            detail_band_real_m=(20.0, 250.0))),
}

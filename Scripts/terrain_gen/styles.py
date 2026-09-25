"""Estilos de modulo por bioma (rangos que sortea el disenador de interiores)."""

from __future__ import annotations

# ── Estilos (rangos por modulo; "weight" = peso del sorteo dentro de su bioma) ───────
# maze: el pasillo es un laberinto de caminos estrechos por todo el modulo.
# dunes: (amplitud m, longitud de onda m) del campo de dunas del suelo; None = sin dunas.
# warp / warp_wave: serpenteo del pasillo y su longitud de onda, en m (diseno de 400 m; se
# escalan por K).
# Monolitos desactivados por defecto (2026-09-23: "no pintan nada"); el rasgo sigue
# disponible subiendo "monoliths" en un estilo.
_BASE_FEATURES = {"cliff_prob": 0.25, "monoliths": (0, 0), "sunken_prob": 0.25, "tunnel_prob": 0.25,
                  "arch_prob": 0.3, "foliage": 0.0, "puddles": False, "causeway": False, "fort": False,
                  "maze": False, "dunes": None, "warp": (35.0, 70.0), "warp_wave": 180.0,
                  "trails": (0, 0), "pond_prob": 0.0}
STYLES: dict[str, dict] = {
    # Arena: desolada (dunas, paredes bajas), cañon, montaña o laberinto de cañones.
    "desert":      {**_BASE_FEATURES, "biome": "sand", "weight": 30, "wall_h": (4.5, 6.8), "bank": (4.5, 6.0),
                    "hills": (6.0, 12.0), "corridor_hw": (22.0, 34.0), "rocks": (0.3, 0.8),
                    "rock_threshold": (0.62, 0.8), "pits": (0, 1), "cliff_prob": 0.1, "tunnel_prob": 0.25,
                    "arch_prob": 0.2, "dunes": ((2.0, 4.0), (24.0, 40.0)), "warp": (45.0, 85.0), "trails": (2, 4), "pond_prob": 0.3},
    "canyon":      {**_BASE_FEATURES, "biome": "sand", "weight": 30, "wall_h": (6.0, 10.5), "bank": (6.0, 9.0),
                    "hills": (12.0, 30.0), "corridor_hw": (18.0, 30.0), "rocks": (1.5, 3.0),
                    "rock_threshold": (0.58, 0.76), "pits": (0, 2), "cliff_prob": 0.35, "tunnel_prob": 0.5,
                    "dunes": ((0.6, 1.4), (18.0, 28.0)), "trails": (2, 5), "pond_prob": 0.25},
    "mountain":    {**_BASE_FEATURES, "biome": "sand", "weight": 20, "wall_h": (7.5, 12.0), "bank": (5.0, 7.0),
                    "hills": (20.0, 38.0), "corridor_hw": (16.0, 26.0), "rocks": (2.5, 4.5),
                    "rock_threshold": (0.48, 0.66), "pits": (1, 3), "cliff_prob": 0.6, "tunnel_prob": 0.55, "trails": (2, 4), "pond_prob": 0.2},
    "maze":        {**_BASE_FEATURES, "biome": "sand", "weight": 20, "wall_h": (3.8, 6.8), "bank": (2.5, 3.5),
                    "hills": (4.0, 10.0), "corridor_hw": (7.0, 10.0), "rocks": (0.3, 0.8),
                    "rock_threshold": (0.62, 0.8), "pits": (0, 0), "maze": True, "cliff_prob": 0.0,
                    "sunken_prob": 0.0, "tunnel_prob": 0.2, "arch_prob": 0.2, "dunes": ((0.5, 1.0), (16.0, 24.0)),
                    "warp": (40.0, 60.0), "warp_wave": 70.0},
    # Agua: la muralla entre el mar poco profundo, o un fuerte con foso en la plaza central.
    "causeway":    {**_BASE_FEATURES, "biome": "water", "weight": 55, "wall_h": (6.0, 9.0), "bank": (6.0, 8.0),
                    "hills": (12.0, 24.0), "corridor_hw": (16.0, 26.0), "rocks": (1.0, 2.0),
                    "rock_threshold": (0.6, 0.78), "pits": (0, 1), "causeway": True, "cliff_prob": 0.0,
                    "tunnel_prob": 0.0, "arch_prob": 0.0},
    "fort":        {**_BASE_FEATURES, "biome": "water", "weight": 45, "wall_h": (3.0, 5.2), "bank": (6.0, 8.0),
                    "hills": (6.0, 12.0), "corridor_hw": (20.0, 32.0), "rocks": (0.8, 1.8),
                    "rock_threshold": (0.6, 0.78), "pits": (0, 1), "puddles": True, "fort": True,
                    "cliff_prob": 0.0, "tunnel_prob": 0.0, "arch_prob": 0.15, "pond_prob": 0.5},
    # Algas: bosque frondoso en tierra, cerrado (cañon), abierto (pradera) o laberinto de
    # setos cubiertos de algas por el que se camina entre la espesura.
    "kelp_forest": {**_BASE_FEATURES, "biome": "algae", "weight": 40, "wall_h": (5.2, 9.0), "bank": (6.0, 8.0),
                    "hills": (10.0, 24.0), "corridor_hw": (18.0, 30.0), "rocks": (0.8, 2.0),
                    "rock_threshold": (0.6, 0.78), "pits": (0, 1), "foliage": 0.85, "tunnel_prob": 0.45, "trails": (2, 5), "pond_prob": 0.3},
    "kelp_meadow": {**_BASE_FEATURES, "biome": "algae", "weight": 30, "wall_h": (3.8, 6.0), "bank": (5.0, 7.0),
                    "hills": (8.0, 16.0), "corridor_hw": (22.0, 34.0), "rocks": (0.5, 1.2),
                    "rock_threshold": (0.62, 0.8), "pits": (0, 1), "foliage": 0.5, "cliff_prob": 0.1,
                    "tunnel_prob": 0.1, "trails": (1, 3), "pond_prob": 0.3},
    "kelp_maze":   {**_BASE_FEATURES, "biome": "algae", "weight": 30, "wall_h": (2.2, 3.4), "bank": (2.5, 3.5),
                    "hills": (1.5, 4.0), "corridor_hw": (7.0, 10.0), "rocks": (0.3, 0.8),
                    "rock_threshold": (0.62, 0.8), "pits": (0, 0), "maze": True, "foliage": 1.0,
                    "cliff_prob": 0.0, "sunken_prob": 0.0, "tunnel_prob": 0.0, "arch_prob": 0.0,
                    "warp": (40.0, 60.0), "warp_wave": 70.0},
}

# Claves que el disenador lee con estos valores si el estilo no las trae; un llamador
# (p. ej. el mapa cosido) las cambia con tweaks= sin tocar la libreria de modulos.
# trail_bank: talud del caminito (m); trail_depth: fraccion hundida hacia el suelo del
# pasillo (min, max a lo largo del caminito); pond_count: laguitos de orilla (min, max),
# None = el charco clasico de pond_prob.
STYLE_TWEAK_DEFAULTS = {"trail_bank": (3.0, 3.0), "trail_depth": (1.0, 1.0), "pond_count": None}

# Variantes de puerta (el tramo del pasillo justo detras de cada boca), independientes del
# estilo: normal, garganta estrecha, abocinada o con un arco encima ("altura de puerta").
GATE_KINDS = {"normal": 40, "gorge": 20, "flare": 20, "arch": 20}

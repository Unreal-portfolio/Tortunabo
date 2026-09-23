"""Estilos de modulo por bioma (rangos que sortea el disenador de interiores)."""

from __future__ import annotations

# ── Estilos (rangos por modulo; "weight" = peso del sorteo dentro de su bioma) ───────
_BASE_FEATURES = {"cliff_prob": 0.25, "monoliths": (0, 2), "sunken_prob": 0.25, "tunnel_prob": 0.25,
                  "arch_prob": 0.3, "foliage": 0.0, "puddles": False, "causeway": False, "fort": False}
STYLES: dict[str, dict] = {
    # Arena: desolada (dunas, paredes bajas) o montanosa (cañon y roca).
    "desert":      {**_BASE_FEATURES, "biome": "sand", "weight": 35, "wall_h": (6.0, 9.0), "bank": (4.5, 6.0),
                    "hills": (8.0, 18.0), "corridor_hw": (22.0, 34.0), "rocks": (0.5, 1.2),
                    "rock_threshold": (0.62, 0.8), "pits": (0, 1), "cliff_prob": 0.1, "monoliths": (1, 3),
                    "tunnel_prob": 0.1, "arch_prob": 0.2},
    "canyon":      {**_BASE_FEATURES, "biome": "sand", "weight": 35, "wall_h": (8.0, 14.0), "bank": (6.0, 9.0),
                    "hills": (12.0, 30.0), "corridor_hw": (18.0, 30.0), "rocks": (1.5, 3.0),
                    "rock_threshold": (0.58, 0.76), "pits": (0, 2), "cliff_prob": 0.35, "tunnel_prob": 0.5},
    "mountain":    {**_BASE_FEATURES, "biome": "sand", "weight": 30, "wall_h": (10.0, 16.0), "bank": (5.0, 7.0),
                    "hills": (20.0, 38.0), "corridor_hw": (16.0, 26.0), "rocks": (2.5, 4.5),
                    "rock_threshold": (0.48, 0.66), "pits": (1, 3), "cliff_prob": 0.6, "monoliths": (0, 3),
                    "tunnel_prob": 0.55},
    # Agua: la muralla entre el mar poco profundo, o un fuerte con foso en la plaza central.
    "causeway":    {**_BASE_FEATURES, "biome": "water", "weight": 55, "wall_h": (8.0, 12.0), "bank": (6.0, 8.0),
                    "hills": (12.0, 24.0), "corridor_hw": (16.0, 26.0), "rocks": (1.0, 2.0),
                    "rock_threshold": (0.6, 0.78), "pits": (0, 1), "causeway": True, "cliff_prob": 0.0,
                    "monoliths": (0, 2), "tunnel_prob": 0.0, "arch_prob": 0.0},
    "fort":        {**_BASE_FEATURES, "biome": "water", "weight": 45, "wall_h": (4.0, 7.0), "bank": (6.0, 8.0),
                    "hills": (6.0, 12.0), "corridor_hw": (20.0, 32.0), "rocks": (0.8, 1.8),
                    "rock_threshold": (0.6, 0.78), "pits": (0, 1), "puddles": True, "fort": True,
                    "cliff_prob": 0.0, "tunnel_prob": 0.0, "arch_prob": 0.15},
    # Algas: bosque frondoso en tierra, cerrado (cañon) o abierto (pradera).
    "kelp_forest": {**_BASE_FEATURES, "biome": "algae", "weight": 55, "wall_h": (7.0, 12.0), "bank": (6.0, 8.0),
                    "hills": (10.0, 24.0), "corridor_hw": (18.0, 30.0), "rocks": (0.8, 2.0),
                    "rock_threshold": (0.6, 0.78), "pits": (0, 1), "foliage": 0.85, "tunnel_prob": 0.45},
    "kelp_meadow": {**_BASE_FEATURES, "biome": "algae", "weight": 45, "wall_h": (5.0, 8.0), "bank": (5.0, 7.0),
                    "hills": (8.0, 16.0), "corridor_hw": (22.0, 34.0), "rocks": (0.5, 1.2),
                    "rock_threshold": (0.62, 0.8), "pits": (0, 1), "foliage": 0.5, "cliff_prob": 0.1,
                    "tunnel_prob": 0.1},
}

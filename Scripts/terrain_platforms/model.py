"""Densidad 3D del mapa de plataformas P01: mesetas de arena de cima plana con acantilado
inclinado sobre un rio, unidas por puentes colgantes (Docs/2026-09-29-Terreno-Plataformas.md).

Es la misma interfaz que terrain_vol.density.MapModel para el resto de la cadena (marching cubes,
color de vertice, exportador TNTM2): chunk_fields(col, row, pad) y density(X, Y, Z, fields).

Densidad (positiva = solido, en metros, ~distancia con signo):
  plataforma  = suavizado_min(distancia al contorno + inclinacion + ruido de pared,  cima - Z)
  lecho       = cota del lecho - Z
  tablero     = min(semiancho - distancia al eje, cara superior - Z, Z - (cara superior - grosor))
  todo        = max(plataformas excavadas por el arco, lecho, tableros)
El arco excava la roca encima de un tablero donde este atraviesa una plataforma mas alta.
"""

from __future__ import annotations

import numpy as np
from scipy import ndimage

from terrain_vol.density import Fields, smooth
from terrain_vol.noise import Fbm2D, ValueNoise3D

from .bridges import Bridge, extract_bridges
from .layout import (CELL_M, CELL_SAMPLES, GRID, MAP_MIN_M, PLATFORM_ABOVE_WATER_M, PX_M, RIVER_DEPTH_M, STEP_XY_M, WATER_M,
                     load_layout, pad_layout)

SEED = 20260929
SD_SMOOTH_PX = 2.5              # suavizado del contorno dibujado (pixeles del layout)
LEAN = 0.15                     # el acantilado se abre 0,15 m por metro de caida (unos 9 grados)
WALL_NOISE_M = (1.3, 0.5)       # amplitud del ruido de pared: grande (7 m) y fino (2,8 m)
LIP_K_M = 0.9                   # radio de redondeo del borde de la cima
TOP_AMP_M = 0.45                # ondulacion de la cima
BED_AMP_M = 0.3                 # ondulacion del lecho del rio
DECK_THICK_M = 1.6              # grosor del tablero
ARCH_SIDE_M = 0.9               # holgura lateral del arco respecto al tablero
ARCH_HEIGHT_M = 4.0             # altura de la clave del arco sobre el tablero
ENTRY_FLAT_M = (1.0, 8.0)       # la cima es plana hasta 1 m del tablero y se ondula del todo a 8 m
TRAIL_WOOD = (0.30, 0.19, 0.09)  # color lineal de los tablones

ZONES = ("cliffs", "canyon", "marsh", "algae", "beach")


def smin(a: np.ndarray, b: np.ndarray, k: float) -> np.ndarray:
    """Minimo suave polinomico: como min(a, b) lejos del cruce, redondeado en un radio k."""
    h = np.clip(0.5 + 0.5 * (b - a) / k, 0.0, 1.0)
    return b + (a - b) * h - k * h * (1.0 - h)


def signed_distance(mask: np.ndarray) -> np.ndarray:
    """Distancia con signo (m) al contorno de una mascara, positiva dentro, suavizada."""
    inside = ndimage.distance_transform_edt(mask) * PX_M - 0.5 * PX_M
    outside = ndimage.distance_transform_edt(~mask) * PX_M - 0.5 * PX_M
    return ndimage.gaussian_filter(np.where(mask, inside, -outside), SD_SMOOTH_PX, mode="nearest")


class PlatformModel:
    def __init__(self, seed: int = SEED, layout: np.ndarray | None = None):
        self.seed = seed
        self.layout = load_layout() if layout is None else layout
        rng = np.random.default_rng(seed)
        self.n_wall = ValueNoise3D(rng, 7.0)
        self.n_wall_fine = ValueNoise3D(rng, 2.8)
        self.n_top = Fbm2D(rng, 24.0, octaves=3)
        self.n_bed = Fbm2D(rng, 28.0, octaves=2)
        padded = pad_layout(self.layout)                    # rejilla de todo el volumen (rio alrededor del plano)
        self.sd = {cls: signed_distance(padded == cls) for cls in PLATFORM_ABOVE_WATER_M}
        self.bridges: list[Bridge] = extract_bridges(self.layout)
        self.high_tint = 0.3
        self.wall_color_mix = 0.75
        self.trail_color = TRAIL_WOOD
        self.trail_strength = 0.9

    # ── Rejillas y campos 2D ─────────────────────────────────────────────────────
    def _sample(self, raster: np.ndarray, X: np.ndarray, Y: np.ndarray) -> np.ndarray:
        coords = [(X - MAP_MIN_M) / PX_M - 0.5, (Y - MAP_MIN_M) / PX_M - 0.5]
        return ndimage.map_coordinates(raster, coords, order=1, mode="nearest")

    def bed_height(self, X: np.ndarray, Y: np.ndarray) -> np.ndarray:
        return WATER_M - RIVER_DEPTH_M + BED_AMP_M * self.n_bed(X, Y)

    def entry_gate(self, X: np.ndarray, Y: np.ndarray) -> np.ndarray:
        """0 sobre un tablero, 1 a partir de ENTRY_FLAT_M[1] de su borde: la cima no se ondula donde
        entra un puente, asi el tablero y la meseta quedan a ras."""
        gate = np.ones(X.shape)
        for bridge in self.bridges:
            lat, idx = bridge.query(X, Y)
            gate = np.minimum(gate, smooth(*ENTRY_FLAT_M, lat - bridge.half_width[idx]))
        return gate

    def top_height(self, cls: int, X: np.ndarray, Y: np.ndarray, gate: np.ndarray) -> np.ndarray:
        return WATER_M + PLATFORM_ABOVE_WATER_M[cls] + TOP_AMP_M * self.n_top(X, Y) * gate

    # ── Interfaz de MapModel ─────────────────────────────────────────────────────
    def chunk_fields(self, col: int, row: int, pad: int = 0) -> tuple[Fields, np.ndarray, np.ndarray]:
        n = CELL_SAMPLES + 2 * pad
        x0 = row * CELL_M - CELL_M / 2.0 - pad * STEP_XY_M
        y0 = col * CELL_M - CELL_M / 2.0 - pad * STEP_XY_M
        X, Y = np.meshgrid(x0 + STEP_XY_M * np.arange(n), y0 + STEP_XY_M * np.arange(n), indexing="ij")
        zeros = np.zeros(X.shape)
        weights = {zone: (np.ones(X.shape) if zone == "cliffs" else zeros.copy()) for zone in ZONES}
        fields = Fields(weights=weights, d_route=zeros, s_route=zeros, height=zeros, floor=self.bed_height(X, Y),
                        wall_band=zeros, foliage=zeros, tunnel=zeros, path=zeros)
        return fields, X, Y

    def color_weights(self, x: np.ndarray, y: np.ndarray) -> dict[str, np.ndarray]:
        return {zone: (np.ones(len(x)) if zone == "cliffs" else np.zeros(len(x))) for zone in ZONES}

    def trail_mask(self, x: np.ndarray, y: np.ndarray) -> np.ndarray:
        return np.zeros(len(x))

    def plaza_mask(self, x: np.ndarray, y: np.ndarray) -> np.ndarray:
        return np.zeros(len(x))

    def trail_mask_3d(self, x: np.ndarray, y: np.ndarray, z: np.ndarray) -> np.ndarray:
        """1 sobre el tablero de un puente (y el suelo del arco que lo continua), 0 en el resto."""
        mask = np.zeros(len(x))
        for bridge in self.bridges:
            lat, idx = bridge.query(x, y)
            top = bridge.deck_top(bridge.arc[idx])
            across = smooth(bridge.half_width[idx] + 0.3, bridge.half_width[idx] - 0.3, lat)
            mask = np.maximum(mask, across * smooth(1.8, 0.9, np.abs(z - top)))
        return mask

    # ── Densidad 3D ──────────────────────────────────────────────────────────────
    def _platforms(self, X, Y, Z3, gate) -> np.ndarray:
        X3, Y3 = X[..., None], Y[..., None]
        noise = WALL_NOISE_M[0] * self.n_wall(X3, Y3, Z3) + WALL_NOISE_M[1] * self.n_wall_fine(X3, Y3, Z3)
        out = np.full(X.shape + (Z3.shape[-1],), -1e3)
        for cls in PLATFORM_ABOVE_WATER_M:
            top = self.top_height(cls, X, Y, gate)[..., None]
            wall = self._sample(self.sd[cls], X, Y)[..., None] + LEAN * (top - Z3) + noise
            out = np.maximum(out, smin(wall, top - Z3, LIP_K_M))
        return out

    def _bridge_terms(self, bridge: Bridge, X, Y, Z3):
        """(tablero, excavacion del arco) de un puente, ambos (nx, ny, nz)."""
        lat, idx = bridge.query(X, Y)
        half = bridge.half_width[idx]
        top = bridge.deck_top(bridge.arc[idx])[..., None]
        lat3, half3 = lat[..., None], half[..., None]
        deck = np.minimum(half3 - lat3, np.minimum(top - Z3, Z3 - (top - DECK_THICK_M)))
        u = np.clip(lat3 / (half3 + ARCH_SIDE_M), 0.0, 1.0)
        arch = np.minimum(np.minimum(Z3 - (top - 0.15), ARCH_HEIGHT_M * np.sqrt(1.0 - u * u) - (Z3 - top)),
                          half3 + ARCH_SIDE_M - lat3)
        return deck, arch

    def density(self, X: np.ndarray, Y: np.ndarray, Z: np.ndarray, fields: Fields | None = None) -> np.ndarray:
        Z3 = Z[None, None, :]
        gate = self.entry_gate(X, Y)
        solid = self._platforms(X, Y, Z3, gate)
        decks = np.full(solid.shape, -1e3)
        for bridge in self.bridges:
            deck, arch = self._bridge_terms(bridge, X, Y, Z3)
            solid = np.minimum(solid, -arch)
            decks = np.maximum(decks, deck)
        bed = self.bed_height(X, Y)[..., None] - Z3
        return np.maximum(np.maximum(solid, bed), decks)


__all__ = ["PlatformModel", "GRID"]

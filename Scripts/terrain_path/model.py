"""Modelo del mapa "camino primero": campos 2D del mapa entero a partir del plan del camino y
densidad 3D por trozo, con la interfaz que usan terrain_vol.mesh y terrain_vol.export."""

from __future__ import annotations

from types import SimpleNamespace

import numpy as np
from scipy.spatial import cKDTree

from terrain_vol.density import Fields, smooth
from terrain_vol.noise import Fbm2D, ValueNoise3D

from . import field
from .curves import normals, resample
from .layout import CELL_M, CELL_SAMPLES, GRID_PAD, MAP_MAX_M, MAP_MIN_M, STEP_XY_M
from .profile import build_plan
from .style import PathStyle

SAMPLE_STEP_M = 0.5
ZONE_KEYS = ("cliffs", "marsh", "algae", "beach")      # paletas de terrain_vol.mesh por bioma


class PathModel:
    def __init__(self, seed: int, style: PathStyle):
        self.seed, self.style = seed, style
        rng = np.random.default_rng(seed)
        self.plan = build_plan(rng, style)
        self.n_floor = Fbm2D(rng, 9.0, 2)
        self.n_rim = Fbm2D(rng, 45.0, 3)
        self.n_top = Fbm2D(rng, 14.0, 3)
        self.n_pond = Fbm2D(rng, 34.0, 3)
        self.n_big = Fbm2D(rng, 160.0, 2)
        self.n_edge = Fbm2D(rng, 45.0, 3)
        self.n_dune_warp = Fbm2D(rng, 70.0, 2)
        self.n_dune_mix = Fbm2D(rng, 120.0, 2)
        self.n_dune_amp = Fbm2D(rng, 90.0, 2)
        self.n_wall3d = ValueNoise3D(rng, 6.5)
        self.n_tunnel = Fbm2D(rng, 9.0, 2)
        self.dune_angle = float(rng.uniform(0.0, np.pi))
        self.dune_angle_2 = self.dune_angle + float(rng.uniform(0.6, 1.1))
        self._build_samples()
        self.extra_rng = rng                       # rio y castillos siguen la misma secuencia
        self._plan_extras()
        count = int(round((MAP_MAX_M - MAP_MIN_M) / STEP_XY_M)) + 1
        self.axis = MAP_MIN_M + STEP_XY_M * np.arange(-GRID_PAD, count + GRID_PAD)
        X, Y = np.meshgrid(self.axis, self.axis, indexing="ij")
        self.grid = self._fields(X, Y)
        main = self.plan.graph.main
        self.route = SimpleNamespace(points=main.points)
        self.start, self.end = main.points[0], main.points[-1]
        self.zones = self

    # -- muestras de todos los caminos -------------------------------------------------------
    def _build_samples(self) -> None:
        cols = {k: [] for k in ("p", "n", "z", "w", "biome", "line", "s", "open")}
        for line in self.plan.graph.lines:
            prof = self.plan.profiles[line.id]
            pts, s = resample(line.points, SAMPLE_STEP_M)
            k = np.clip(np.searchsorted(line.arc, s), 0, len(line.arc) - 1)
            cols["p"].append(pts)
            cols["n"].append(normals(pts))
            cols["z"].append(np.interp(s, line.arc, prof.z))
            cols["w"].append(np.interp(s, line.arc, prof.half_width))
            cols["biome"].append(prof.biome[k])
            cols["line"].append(np.full(len(s), line.id))
            cols["s"].append(s)
            cols["open"].append(~prof.tunnel[k])
        self.S = {k: np.concatenate(v) for k, v in cols.items()}
        self.open_idx = np.nonzero(self.S["open"])[0]
        self.open_tree = cKDTree(self.S["p"][self.open_idx])

    def samples(self) -> dict[str, np.ndarray]:
        return self.S

    def _plan_extras(self) -> None:
        """Rio y castillos."""
        from .river import plan_river
        self.river = plan_river(self.extra_rng, self)
        self.castles = []

    def near_junction(self, p, radius: float) -> bool:
        g = self.plan.graph
        pts = [line.points[0] for line in g.loops()] + [line.points[-1] for line in g.loops()]
        pts += [c.point for c in self.plan.crossings]
        return any(np.hypot(*(np.asarray(p) - q)) < radius for q in pts)

    # -- consultas ----------------------------------------------------------------------------
    def _nearest(self, x, y, k: int = 1):
        d, i = self.open_tree.query(np.stack([np.ravel(x), np.ravel(y)], axis=1), k=k)
        return d, self.open_idx[i]

    def weights(self, x, y) -> dict[str, np.ndarray]:
        bw = self._biome_weights(x, y)
        out = {key: bw[..., b] for b, key in enumerate(ZONE_KEYS)}
        out["canyon"] = np.zeros(np.shape(x))
        return out

    def _biome_weights(self, x, y):
        d, i = self._nearest(x, y, k=24)
        wgt = np.exp(-0.5 * ((d - d[:, :1]) / 15.0) ** 2)
        onehot = np.eye(4)[self.S["biome"][i]]
        bw = (onehot * wgt[..., None]).sum(axis=1) / wgt.sum(axis=1)[:, None]
        return bw.reshape(np.shape(x) + (4,))

    def trail_mask(self, x, y) -> np.ndarray:
        d, i = self._nearest(x, y)
        e = d - self.S["w"][i]
        dry = self.S["biome"][i] != 1
        return ((1.0 - smooth(-1.2, 0.2, e)) * dry).reshape(np.shape(x))

    def plaza_mask(self, x, y) -> np.ndarray:
        a = self.plan.graph.main.points[0]
        return 1.0 - smooth(6.0, 8.0, np.hypot(np.asarray(x) - a[0], np.asarray(y) - a[1]))

    # -- campos 2D ----------------------------------------------------------------------------
    def _fields(self, X, Y) -> Fields:
        shape = X.shape
        d1, i1 = self._nearest(X, Y)
        d, i = d1.reshape(shape), i1.reshape(shape)
        zf, w = self.S["z"][i], self.S["w"][i]
        dk, ik = self._nearest(X, Y, k=24)
        wgt = np.exp(-0.5 * ((dk - dk[:, :1]) / 10.0) ** 2)
        z_soft = ((self.S["z"][ik] * wgt).sum(axis=1) / wgt.sum(axis=1)).reshape(shape)
        wb = np.exp(-0.5 * ((dk - dk[:, :1]) / 15.0) ** 2)
        bw = (np.eye(4)[self.S["biome"][ik]] * wb[..., None]).sum(axis=1)
        bw = (bw / bw.sum(axis=1, keepdims=True)).reshape(shape + (4,))
        e = d - w
        height, H, crest_e = field.section(e, zf, z_soft, w, bw, self.n_rim.unit(X, Y), self.n_top(X, Y),
                                           self.n_floor(X, Y), self.style)
        height = self._inside_corridor(X, Y, height, e, i, zf, w, bw)
        v = field.vista(self, X, Y)
        band = self.style.block_band_m
        t = smooth(crest_e + band, crest_e + band + 14.0, e)
        outer = H + (v - H) * t
        height = np.where(e < crest_e + band, height, outer)
        height = self._stamps(X, Y, height, e, i, zf, w)
        protect = (1.0 - bw[..., 3]) * (1.0 - t)
        height, coast = field.shore(self, X, Y, height, protect)
        region = np.where(e < 0.0, 0, np.where(e < crest_e + band + 14.0, 1, 2))
        region = np.where(coast > 0.5, 3, region)
        if shape == (len(getattr(self, "axis", [])),) * 2:
            self.region = region
        wall_band = (1.0 - smooth(0.0, 3.0, np.abs(e - 0.5 * crest_e))) * bw[..., 0]
        tunnel = self._tunnel_zone(X, Y)
        path = 1.0 - smooth(-1.0, 0.5, e)
        weights = {key: bw[..., b] for b, key in enumerate(ZONE_KEYS)}
        weights["canyon"] = np.zeros(shape)
        zeros = np.zeros(shape)
        return Fields(weights, d, zeros, height, zf, np.clip(wall_band, 0, 1), zeros, tunnel, np.clip(path, 0, 1))

    def _inside_corridor(self, X, Y, height, e, i, zf, w, bw):
        """Suelo del camino segun el tramo: en el agua lo sustituye el rio."""
        if self.river is None:
            return height
        from .river import river_floor
        floor, weight = river_floor(self, X, Y, i, zf, w)
        inside = 1.0 - smooth(-0.5, 0.0, e)
        mix = weight * bw[..., 1] * inside
        return height * (1.0 - mix) + floor * mix

    def _stamps(self, X, Y, height, e, i, zf, w):
        """Tuneles de cerro (tarea 8) y castillos (tarea 9)."""
        return height

    def _tunnel_zone(self, X, Y):
        return np.zeros(X.shape)

    # -- acceso por trozo y 3D ----------------------------------------------------------------
    def chunk_fields(self, col: int, row: int, pad: int = 0):
        i0 = int(round((row * CELL_M - CELL_M / 2.0 - MAP_MIN_M) / STEP_XY_M)) + GRID_PAD - pad
        j0 = int(round((col * CELL_M - CELL_M / 2.0 - MAP_MIN_M) / STEP_XY_M)) + GRID_PAD - pad
        n = CELL_SAMPLES + 2 * pad
        X, Y = np.meshgrid(self.axis[i0:i0 + n], self.axis[j0:j0 + n], indexing="ij")
        return self.grid.window(i0, i0 + n, j0, j0 + n), X, Y

    def density(self, X, Y, Z, f: Fields) -> np.ndarray:
        X3, Y3, Z3 = X[..., None], Y[..., None], Z[None, None, :]
        D = f.height[..., None] - Z3
        band = f.wall_band[..., None]
        if np.any(band > 0.0):
            above = smooth(0.8, 2.5, Z3 - f.floor[..., None])
            D = D + 0.45 * band * above * self.n_wall3d(X3, Y3, Z3)
        return D

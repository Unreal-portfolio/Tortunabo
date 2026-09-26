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
from .layout import CELL_M, CELL_SAMPLES, GRID_PAD, MAP_MAX_M, MAP_MIN_M, STEP_XY_M, WATER_M
from .profile import build_plan
from .style import PathStyle

SAMPLE_STEP_M = 0.5
HIGH_TINT = 0.25         # oscurecimiento por altura de la paleta (terrain_vol): casi nada aqui
WEDGE_M = 6.0            # cuna entre dos caminos mas fina que esto: se rellena de suelo (nariz roma)
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
        # Anadidos sin tocar el mapa: su azar sale de otro generador (misma semilla, mismo grafo).
        extra = np.random.default_rng(seed + 17)
        self.n_crest = Fbm2D(extra, 40.0, 3)
        self.n_wall2d = Fbm2D(extra, 11.0, 3)
        self.arch_ranges = self._plan_arches(extra)
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
        self.high_tint = HIGH_TINT
        self.trail_strength = 0.85

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
        # El final entra en el mar: el abanico sigue 50 m en la direccion de llegada y baja bajo
        # el agua, asi que el camino llega a la orilla por si mismo (la costa de las vistas no
        # toca el camino y no se puede salir por la playa a las vistas).
        main, prof = self.plan.graph.main, self.plan.profiles[0]
        t = main.tangent_at(main.length)
        ext = np.arange(SAMPLE_STEP_M, 40.0 + SAMPLE_STEP_M, SAMPLE_STEP_M)
        pts = main.points[-1][None, :] + ext[:, None] * t[None, :]
        # Se abre en bahia hacia el mar (las paredes divergen), no en un canal recto.
        fan = prof.half_width[-1] + 30.0 * (ext / 40.0) ** 1.5
        cols_ext = {"p": pts, "n": np.tile([-t[1], t[0]], (len(ext), 1)),
                    "z": np.interp(ext, [0.0, 40.0], [prof.z[-1], WATER_M - 1.5]),
                    "w": fan, "biome": np.full(len(ext), 3),
                    "line": np.zeros(len(ext), dtype=int), "s": main.length + ext, "open": np.ones(len(ext), bool)}
        for key, value in cols_ext.items():
            cols[key].append(value)
        self.S = {k: np.concatenate(v) for k, v in cols.items()}
        self.open_idx = np.nonzero(self.S["open"])[0]
        self.open_tree = cKDTree(self.S["p"][self.open_idx])
        self.line_trees = {}
        for line_id in np.unique(self.S["line"][self.open_idx]):
            idx = self.open_idx[self.S["line"][self.open_idx] == line_id]
            self.line_trees[int(line_id)] = (cKDTree(self.S["p"][idx]), idx)
        # Tramos cubiertos (camino de abajo de un cruce, tuneles de cerro), con 2,5 m de boca.
        self.tunnels = []
        for line in self.plan.graph.lines:
            prof = self.plan.profiles[line.id]
            if not prof.tunnel.any():
                continue
            pts, s = resample(line.points, SAMPLE_STEP_M)
            k = np.clip(np.searchsorted(line.arc, s), 0, len(line.arc) - 1)
            covered = prof.tunnel[k]
            for run in np.split(np.arange(len(s)), np.nonzero(np.diff(covered.astype(int)))[0] + 1):
                if not covered[run[0]]:
                    continue
                sel = np.arange(max(run[0] - 5, 0), min(run[-1] + 5, len(s) - 1) + 1)
                t = (s[sel] - s[run[0]]) / max(s[run[-1]] - s[run[0]], 1e-9)
                s_mid = 0.5 * (s[run[0]] + s[run[-1]])
                arch = any(line_id == line.id and a <= s_mid <= b for line_id, a, b in self.arch_ranges)
                bridge = self._bridge_clearance(line.id, s_mid)
                # (alcance lateral del cerro, alto del hueco, tablero sobre el suelo)
                if bridge is not None:          # cruce: puente fino, el de abajo va al aire libre
                    kind_reach, kind_height, kind_deck = -1.0, bridge - 1.5, 0.0
                elif arch:                      # arco fino sobre el camino
                    kind_reach, kind_height, kind_deck = 3.0, 4.8, 6.3
                else:                           # tunel de cerro
                    kind_reach, kind_height, kind_deck = 10.0, 5.0, 8.0
                self.tunnels.append({"pts": pts[sel], "floor": np.interp(s[sel], line.arc, prof.z),
                                     "half": np.clip(np.interp(s[sel], line.arc, prof.half_width), 3.0, 4.5),
                                     "t": t, "covered": covered[sel].astype(float),
                                     "reach": np.full(len(sel), kind_reach),
                                     "height": np.full(len(sel), kind_height),
                                     "deck": np.full(len(sel), kind_deck)})
        if self.tunnels:
            self.tunnel_tree = cKDTree(np.vstack([tu["pts"] for tu in self.tunnels]))
            self.tunnel_cat = {k: np.concatenate([tu[k] for tu in self.tunnels])
                               for k in ("floor", "half", "t", "covered", "reach", "height", "deck")}
        else:
            self.tunnel_tree = None

    def _bridge_clearance(self, line_id: int, s: float) -> float | None:
        """Hueco bajo el puente si (line_id, s) es el camino de abajo de un cruce, si no None."""
        for c in self.plan.crossings:
            if c.lower == line_id and abs(c.s_lower - s) < 25.0:
                up = self.plan.graph.lines[c.upper]
                lo = self.plan.graph.lines[c.lower]
                return float(np.interp(c.s_upper, up.arc, self.plan.profiles[c.upper].z)
                             - np.interp(c.s_lower, lo.arc, self.plan.profiles[c.lower].z))
        return None

    def _plan_arches(self, rng: np.random.Generator) -> list[tuple[int, float, float]]:
        """Puentes naturales (arco de roca de 8-12 m sobre el camino, que pasa por debajo) y
        tuneles de cerro extra. Se marcan como tramos cubiertos del perfil, lejos de uniones,
        cruces, tuneles, el rio, la playa y la salida. Devuelve (linea, s0, s1) de los arcos."""
        from .profile import _mark
        plan, style = self.plan, self.style
        joins = [c.point for c in plan.crossings]
        joins += [line.points[k] for line in plan.graph.loops() for k in (0, -1)]
        taken: list[tuple[int, float, float]] = []
        arches: list[tuple[int, float, float]] = []
        for kind, count, (lo, hi) in (("arch", style.arches, (4.0, 6.0)),
                                      ("hill", style.extra_hill_tunnels, (20.0, 32.0))):
            placed = 0
            for _ in range(400):
                if placed >= count:
                    break
                # Mitad en el camino principal (el que siempre se recorre); el resto, en los lazos.
                on_main = rng.random() < 0.5
                line = plan.graph.lines[0] if on_main else plan.graph.lines[int(rng.integers(1, len(plan.graph.lines)))]
                prof = plan.profiles[line.id]
                length = float(rng.uniform(lo, hi))
                if line.length < length + 60.0:
                    continue
                s0 = float(rng.uniform(30.0, line.length - length - 30.0))
                s1 = s0 + length
                k0, k1 = int(np.searchsorted(line.arc, s0 - 20.0)), int(np.searchsorted(line.arc, s1 + 20.0))
                if prof.tunnel[k0:k1 + 1].any() or np.isin(prof.biome[k0:k1 + 1], (1, 3)).any():
                    continue
                if kind == "arch" and prof.half_width[k0:k1 + 1].max() > 7.5:
                    continue                         # en un ensanche grande no hay arco que cierre
                mid = line.point_at(0.5 * (s0 + s1))
                if any(np.hypot(*(mid - q)) < 18.0 + 0.5 * length for q in joins):
                    continue
                if any(lid == line.id and not (s1 + 30.0 < a or s0 - 30.0 > b) for lid, a, b in taken):
                    continue
                plan.profiles[line.id] = _mark(prof, line, s0, s1)
                taken.append((line.id, s0, s1))
                if kind == "arch":
                    arches.append((line.id, s0, s1))
                else:
                    plan.hill_tunnels.append((line.id, s0, s1))
                placed += 1
        return arches

    def samples(self) -> dict[str, np.ndarray]:
        return self.S

    def _plan_extras(self) -> None:
        """Rio y castillos."""
        from .river import plan_river
        self.river = plan_river(self.extra_rng, self)
        from .castles import plan_castles
        self.castles = plan_castles(self.extra_rng, self)

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
        return ((1.0 - smooth(-0.4, 0.6, e)) * dry).reshape(np.shape(x))

    def plaza_mask(self, x, y) -> np.ndarray:
        a = self.plan.graph.main.points[0]
        return 1.0 - smooth(6.0, 8.0, np.hypot(np.asarray(x) - a[0], np.asarray(y) - a[1]))

    # -- campos 2D ----------------------------------------------------------------------------
    def _fields(self, X, Y) -> Fields:
        shape = X.shape
        dk, ik = self._nearest(X, Y, k=24)
        # Camino que cubre el punto: el de menor (distancia - semiancho) entre los cercanos, no el
        # mas cercano (con anchos distintos, el mas cercano puede ser un tramo estrecho).
        cover = dk - self.S["w"][ik]
        order = np.argsort(cover, axis=1)
        rows = np.arange(len(order))
        best = order[:, 0]
        d, i = dk[rows, best].reshape(shape), ik[rows, best].reshape(shape)
        zf, w = self.S["z"][i], self.S["w"][i]
        # Segundo camino que cubre el punto (de otra linea): donde los dos estan casi igual de
        # cerca se mezcla su relieve y no queda un surco en la bisectriz.
        other = self.S["line"][ik] != self.S["line"][ik[rows, best]][:, None]
        cover2 = np.where(other, cover, np.inf)
        second = np.argmin(cover2, axis=1)
        has2 = np.isfinite(cover2[rows, second])
        d2 = np.where(has2, dk[rows, second], d.ravel()).reshape(shape)
        i2 = np.where(has2, ik[rows, second], i.ravel()).reshape(shape)
        gap = np.where(has2, cover2[rows, second] - cover[rows, best], np.inf).reshape(shape)
        z_soft, bw = self._soft_levels(X, Y)
        # Cimas con lomas y ondas de duna (no mesetas): solo suben la cresta.
        relief = self.style.crest_relief_m * smooth(0.25, 0.9, self.n_crest.unit(X, Y)) \
            + 0.8 * field.dune_field(self, X, Y, self.dune_angle, 17.0)
        z_soft = z_soft + relief
        e = d - w
        e = self._blunt_wedges(X, Y, e, i)
        n_rim, n_top, n_floor = self.n_rim.unit(X, Y), self.n_top(X, Y), self.n_floor(X, Y)
        n_wall = self.n_wall2d(X, Y)
        guard = self._guard
        height, H, crest_e = field.section(e, zf, z_soft, w, bw, n_rim, n_top, n_floor, self.style, guard, n_wall)
        zf2, w2 = self.S["z"][i2], self.S["w"][i2]
        e2 = self._blunt_wedges(X, Y, d2 - w2, i2)
        height2, _, _ = field.section(e2, zf2, z_soft, w2, bw, n_rim, n_top, n_floor, self.style, guard, n_wall)
        # Solo entre caminos a cota parecida: con cotas distintas, promediar la pared de uno con el
        # suelo del otro dejaba una rampa por la que se salia a las vistas.
        mix = 0.5 * (1.0 - smooth(0.0, 3.0, gap)) * (e > 0.0) * (e2 > 0.0) * (1.0 - smooth(0.5, 1.0, np.abs(zf - zf2)))
        height = height * (1.0 - mix) + height2 * mix
        height = self._inside_corridor(X, Y, height, e, i, zf, w, bw)
        v = field.vista(self, X, Y)
        band = self.style.block_band_m
        # Bajada a las vistas proporcional al desnivel (~27 grados): una pared alta que caia de
        # golpe por detras quedaba como una aleta fina vista desde fuera.
        run = np.maximum(14.0, 2.0 * (H - v))
        t = smooth(crest_e + band, crest_e + band + run, e)
        outer = H + (v - H) * t
        # Transicion suave de la cresta a la bajada (un corte seco dejaba lineas de un paso donde
        # cambia el camino mas cercano).
        to_outer = smooth(crest_e + band - 1.5, crest_e + band + 1.5, e)
        height = height * (1.0 - to_outer) + outer * to_outer
        height = self._stamps(X, Y, height, e, i, zf, w)
        protect = 1.0 - t
        height, coast = field.shore(self, X, Y, height, protect)
        height = field.clip_spikes(height)
        region = np.where(e < 0.0, 0, np.where(e < crest_e + band + run, 1, 2))
        region = np.where(coast > 0.5, 3, region)
        if shape == (len(getattr(self, "axis", [])),) * 2:
            self.region = region
        # Voladizos y huecos suaves en todas las paredes (antes solo en el acantilado).
        wall_band = (1.0 - smooth(0.0, 3.0, np.abs(e - 0.5 * crest_e))) * (0.5 + 0.5 * bw[..., 0])
        tunnel = self._tunnel_zone(X, Y)
        path = 1.0 - smooth(-1.0, 0.5, e)
        weights = {key: bw[..., b] for b, key in enumerate(ZONE_KEYS)}
        weights["canyon"] = np.zeros(shape)
        zeros = np.zeros(shape)
        return Fields(weights, d, zeros, height, zf, np.clip(wall_band, 0, 1), zeros, tunnel, np.clip(path, 0, 1))

    def _soft_levels(self, X, Y):
        """(cota suavizada, pesos de bioma) mezclando el punto mas cercano de CADA camino con un
        peso que cae con la distancia: continuos tambien en las bisectrices entre caminos (con las
        24 muestras mas cercanas saltaban, porque todas eran del mismo camino)."""
        pts = np.stack([np.ravel(X), np.ravel(Y)], axis=1)
        dists, zs, biomes = [], [], []
        reach = self.style.block_band_m
        guard = np.full(len(pts), -1e3)
        for tree, idx in self.line_trees.values():
            d, k = tree.query(pts)
            dists.append(d)
            zs.append(self.S["z"][idx[k]])
            biomes.append(self.S["biome"][idx[k]])
            # Suelo del camino mas alto de los cercanos: la cresta no baja de el (continuo).
            near = 1.0 - smooth(reach + 10.0, reach + 20.0, d - self.S["w"][idx[k]])
            guard = np.maximum(guard, self.S["z"][idx[k]] - 100.0 * (1.0 - near))
        d = np.stack(dists, axis=1)
        wz = np.exp(-0.5 * ((d - d.min(axis=1, keepdims=True)) / 10.0) ** 2)
        z_soft = (np.stack(zs, axis=1) * wz).sum(axis=1) / wz.sum(axis=1)
        wb = np.exp(-0.5 * ((d - d.min(axis=1, keepdims=True)) / 15.0) ** 2)
        bw = (np.eye(4)[np.stack(biomes, axis=1)] * wb[..., None]).sum(axis=1)
        bw = bw / bw.sum(axis=1, keepdims=True)
        self._guard = guard.reshape(X.shape)
        return z_soft.reshape(X.shape), bw.reshape(X.shape + (4,))

    def _blunt_wedges(self, X, Y, e, i):
        """Donde dos caminos se separan (union de un lazo, cruce), la pared entre ambos nace como
        una cuna que sube despacio a lo largo de la bisectriz y hace de rampa hasta la cresta.
        Se rellena de suelo hasta que la cuna tiene WEDGE_M de grosor: la pared empieza con una
        cara roma que no se puede subir."""
        pts = np.stack([np.ravel(X), np.ravel(Y)], axis=1)
        own = self.S["line"][i].ravel()
        z_own = self.S["z"][i].ravel()
        e_other = np.full(len(pts), np.inf)
        for line_id, (tree, idx) in self.line_trees.items():
            d, k = tree.query(pts)
            # Solo entre caminos a la misma cota (uniones); en un cruce no se rellena.
            level = np.abs(self.S["z"][idx[k]] - z_own) < 1.0
            cand = np.where(level, d - self.S["w"][idx[k]], np.inf)
            e_other = np.where(own != line_id, np.minimum(e_other, cand), e_other)
        e_other = e_other.reshape(X.shape)
        wedge = (e > 0.0) & (e_other > 0.0) & (e + e_other < WEDGE_M)
        return np.where(wedge, -0.5, e)

    def _inside_corridor(self, X, Y, height, e, i, zf, w, bw):
        """Suelo del camino segun el tramo: en el agua lo sustituye el rio."""
        if self.river is None:
            return height
        from .river import river_floor
        floor, weight = river_floor(self, X, Y, i, zf, w)
        inside = 1.0 - smooth(-0.5, 0.0, e)
        mix = weight * bw[..., 1] * inside
        return height * (1.0 - mix) + floor * mix

    def _tunnel_query(self, X, Y):
        d, k = self.tunnel_tree.query(np.stack([np.ravel(X), np.ravel(Y)], axis=1))
        c = self.tunnel_cat
        return (d.reshape(X.shape), c["t"][k].reshape(X.shape), c["floor"][k].reshape(X.shape),
                c["half"][k].reshape(X.shape), c["covered"][k].reshape(X.shape), c["reach"][k].reshape(X.shape),
                c["height"][k].reshape(X.shape), c["deck"][k].reshape(X.shape))

    def _tunnel_zone(self, X, Y):
        if self.tunnel_tree is None:
            return np.zeros(X.shape)
        u, _, _, half, _, _, _, _ = self._tunnel_query(X, Y)
        return 1.0 - smooth(half + 1.5, half + 5.0, u)

    def _stamps(self, X, Y, height, e, i, zf, w):
        """Cerro sobre cada tramo cubierto: roca >= 8 m sobre el suelo del tunel (el camino de
        arriba de un cruce ya queda 8,5 m por encima); solo en el tramo cubierto, no en las bocas."""
        if self.tunnel_tree is not None:
            u, t, floor, half, covered, reach, _, deck = self._tunnel_query(X, Y)
            # Nunca sobre un camino abierto (el tablero de un cruce, la union de un lazo). Los
            # puentes naturales (reach corto) son un arco estrecho, no un cerro.
            # Un puente es corto: su centro sigue a menos de un semiancho del camino abierto, asi
            # que se levanta aunque "este en el camino" (el paso queda en el hueco 3D de debajo).
            over_path = np.maximum(smooth(0.0, 2.0, e), ((reach > 0.0) & (reach < 5.0)).astype(float))
            hill = (1.0 - smooth(half + 2.0, half + np.maximum(reach, 2.1), u)) * covered * (reach > 0.0) \
                * smooth(-0.1, 0.05, t) * smooth(-1.1, -0.95, -t) * over_path
            top = floor + deck + np.where(deck > 7.0, 1.5 * self.n_top.unit(X, Y), 0.3 * self.n_top(X, Y))
            height = np.maximum(height, top * hill + height * (1.0 - hill))
        from .castles import castle_stamp
        for castle in self.castles:
            height = castle_stamp(X, Y, height, castle)
        return height

    def _carve(self, X, Y, Z3):
        """> 0 dentro del hueco (misma seccion que terrain_vol.density.MapModel._tunnel_carve)."""
        u, t, floor, half, _, _, gap, _ = self._tunnel_query(X, Y)
        half = half * (1.0 + 0.22 * self.n_tunnel(X, Y))
        # Hueco a la medida: tunel de 5 m, arco fino de 4,8 m y, bajo un puente, todo el hueco
        # hasta 1,5 m por debajo del tablero (en un puente la variacion no puede comerse el tablero).
        height = gap * (1.0 + np.where(gap > 5.5, 0.0, 0.12) * self.n_tunnel(Y, X))
        v = Z3 - floor[..., None]
        center, radius_v = 0.42 * height[..., None], 0.62 * height[..., None]
        ellipse = 1.0 - np.sqrt((u[..., None] / half[..., None]) ** 2 + ((v - center) / radius_v) ** 2)
        inside = np.minimum(ellipse * half[..., None], v + 0.3)
        active = (t > -0.2) & (t < 1.2)
        return np.where(active[..., None], inside, -1.0), v

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
            # Solo por encima del pie de la pared: el pie sigue liso y vertical (cierra el paso).
            above = smooth(field.FOOT_M, field.FOOT_M + 1.5, Z3 - f.floor[..., None])
            D = D + 0.9 * band * above * self.n_wall3d(X3, Y3, Z3)
        if self.tunnel_tree is not None and np.any(f.tunnel > 0.0):
            carve, v = self._carve(X, Y, Z3)
            # Paredes y techo rugosos; el suelo del tunel, llano.
            carve = carve + 0.5 * self.n_wall3d(X3 * 1.7, Y3 * 1.7, Z3 * 1.7) * (carve > -0.5) * smooth(0.3, 1.2, v)
            D = np.minimum(D, -carve)
        return D


WALKABLE_SLOPE = 1.0     # tan(45 grados), como el suelo andable de CharacterMovement (44,76 grados)


def walkable(standable: np.ndarray, height: np.ndarray, z_levels: np.ndarray) -> np.ndarray:
    """Quita de 'standable' la superficie de arriba donde la pendiente pasa de 45 grados: la
    busqueda de recorrido sube por escalones de 1 m y, en diagonal, trepaba paredes de 78 grados
    que el personaje no puede andar. Los suelos de dentro de los tuneles (por debajo de la
    superficie) no se tocan. 'height' es la rejilla 2D del mapa sin el margen (misma forma que las
    dos primeras dimensiones de 'standable')."""
    gx, gy = np.gradient(height)
    steep = np.hypot(gx, gy) > WALKABLE_SLOPE
    near_top = np.abs(z_levels[None, None, :] - height[..., None]) < 1.0
    return standable & ~(steep[..., None] & near_top)

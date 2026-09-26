"""Modelo del mapa "camino primero": campos 2D del mapa entero a partir del plan del camino y
densidad 3D por trozo, con la interfaz que usan terrain_vol.mesh y terrain_vol.export."""

from __future__ import annotations

from dataclasses import replace
from types import SimpleNamespace

import numpy as np
from scipy import ndimage
from scipy.spatial import cKDTree

from terrain_vol.density import Fields, smooth
from terrain_vol.noise import Fbm2D, ValueNoise3D

from . import bridge, canyon, field
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
        self.n_band = Fbm2D(extra, 80.0, 2)
        self.deck_cuts: list[tuple[int, float, float]] = []
        decks = self._plan_bridges()
        self.arch_ranges = self._plan_arches(extra)
        # Cima natural (v2): despues de los arcos, para no mover su colocacion.
        self.n_crest_big = Fbm2D(extra, 60.0, 2)
        self.n_crest_mid = Fbm2D(extra, 18.0, 3)
        self.n_crest_fine = Fbm2D(extra, 6.0, 2)
        self.n_run = Fbm2D(extra, 50.0, 2)
        decks += [bridge.arch_deck(self.plan.graph.lines[lid], self.plan.profiles[lid], 0.5 * (a + b))
                  for lid, a, b in self.arch_ranges]
        self._build_samples()
        # Barranco (v2): generador aparte; su puente sustituye el tramo del principal que lo cruza.
        self.canyon = canyon.plan_canyon(self, np.random.default_rng(seed + 29))
        self.canyon_field = None
        if self.canyon is not None:
            self.plan.profiles[0] = canyon.raise_main(self.plan.profiles[0], self.plan.graph.main.arc, self.canyon,
                                                      self.style.max_grade)
            for line_id, s_c in ((0, self.canyon.s_main),) + tuple(self.canyon.crossings):
                deck, cut = canyon.canyon_deck(self, self.canyon, line_id, s_c)
                prof = self.plan.profiles[line_id]
                arc = self.plan.graph.lines[line_id].arc
                self.plan.profiles[line_id] = replace(prof, half_width=bridge.narrow(prof.half_width, arc, s_c, cut[2] - s_c))
                self.deck_cuts.append(cut)
                decks.append(deck)
            self._build_samples()
            self.canyon_field = canyon.CanyonField(self.canyon, self.n_floor)
        self.decks = bridge.DeckSet(decks, self.n_wall3d)
        self.extra_rng = rng                       # rio y castillos siguen la misma secuencia
        self._plan_extras()
        count = int(round((MAP_MAX_M - MAP_MIN_M) / STEP_XY_M)) + 1
        self.axis = MAP_MIN_M + STEP_XY_M * np.arange(-GRID_PAD, count + GRID_PAD)
        X, Y = np.meshgrid(self.axis, self.axis, indexing="ij")
        self._fade_grid = self._fade_field(X, Y)
        self.grid = self._fields(X, Y)
        main = self.plan.graph.main
        self.route = SimpleNamespace(points=main.points)
        self.start, self.end = main.points[0], main.points[-1]
        self.zones = self
        self.high_tint = HIGH_TINT
        self.trail_strength = 0.85
        self.trail_color = (0.34, 0.24, 0.13)     # arena pisada, mas oscura que la de fuera
        # Suelo y pared distintos (v2): la pared toma su color entero a partir de ~28-46 grados y
        # lleva vetas horizontales.
        self.cliff_band = (0.12, 0.3)
        self.wall_color_mix = style.wall_color_mix
        self.wall_strata = 0.12
        self.wall_noise = 0.45         # rugosidad 3D de las paredes (voladizos y huecos)

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
            cols["open"].append(~prof.tunnel[k] & ~self._in_cut(line.id, s))
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
                # Tunel de cerro (o cruce de tipo tunel): (alcance lateral del cerro, alto del hueco,
                # techo sobre el suelo). Los puentes y arcos no son tramos cubiertos: bridge.py.
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

    def _plan_bridges(self) -> list[bridge.Deck]:
        """Tablero de cada cruce "bridge": el camino de arriba se estrecha al ancho del tablero y,
        en la luz, deja de ser camino abierto (el corredor de abajo lo corta; lo cruza la losa)."""
        decks = []
        for c in self.plan.crossings:
            if c.kind != "bridge":
                continue
            deck, cut = bridge.crossing_deck(self, c)
            line = self.plan.graph.lines[c.upper]
            prof = self.plan.profiles[c.upper]
            clear = cut[2] - c.s_upper
            self.plan.profiles[c.upper] = replace(prof, half_width=bridge.narrow(prof.half_width, line.arc,
                                                                                 c.s_upper, clear))
            self.deck_cuts.append(cut)
            decks.append(deck)
        return decks

    def _in_cut(self, line_id: int, s: np.ndarray) -> np.ndarray:
        out = np.zeros(len(s), dtype=bool)
        for lid, a, b in self.deck_cuts:
            if lid == line_id:
                out |= (s >= a) & (s <= b)
        return out

    def _plan_arches(self, rng: np.random.Generator) -> list[tuple[int, float, float]]:
        """Arcos naturales (losa de roca de pared a pared, bridge.arch_deck; el camino pasa por debajo
        al aire libre) y tuneles de cerro extra (tramos cubiertos), lejos de uniones, puentes,
        cruces, tuneles, el rio, la playa y la salida. Devuelve (linea, s0, s1) de los arcos."""
        from .profile import _mark
        plan, style = self.plan, self.style
        joins = [c.point for c in plan.crossings]
        joins += [line.points[k] for line in plan.graph.loops() for k in (0, -1)]
        taken: list[tuple[int, float, float]] = list(self.deck_cuts)
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
                if kind == "hill":
                    plan.profiles[line.id] = _mark(prof, line, s0, s1)
                taken.append((line.id, s0, s1))
                if kind == "arch":
                    arches.append((line.id, s0, s1))
                else:
                    plan.hill_tunnels.append((line.id, s0, s1))
                placed += 1
        return arches

    def remove_deadly(self, standable: np.ndarray, z_levels: np.ndarray) -> np.ndarray:
        """Quita de 'standable' (rejilla del mapa sin margen) el fondo del barranco mortal: caer ahi
        es morir, asi que no cuenta como sitio por el que se pasa."""
        if self.canyon_field is None or self.canyon.mode != "deadly":
            return standable
        X, Y = np.meshgrid(self.axis[1:-1], self.axis[1:-1], indexing="ij")
        inside = self.canyon_field.inside(X, Y)
        low = z_levels < canyon.KILL_TOP_M
        return standable & ~(inside[..., None] & low[None, None, :])

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
        # Mismo criterio que el relieve (el camino que cubre el punto, no la muestra mas
        # cercana): si no, la franja oscura no casaba con el suelo donde cambia el ancho.
        dk, ik = self._nearest(x, y, k=24)
        cover = dk - self.S["w"][ik]
        best = np.argmin(cover, axis=1)
        rows = np.arange(len(best))
        e = cover[rows, best]
        dry = self.S["biome"][ik[rows, best]] != 1
        return ((1.0 - smooth(-0.4, 0.6, e)) * dry).reshape(np.shape(x))

    def plaza_mask(self, x, y) -> np.ndarray:
        """Sin disco de color en la salida (quedaba como una mancha clara): la salida se lee por
        el ensanche y el camino marcado."""
        return np.zeros(np.shape(x))

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
        e = d - w
        e = self._blunt_wedges(X, Y, e, i)
        n_rim, n_top, n_floor = self.n_rim.unit(X, Y), self.n_top(X, Y), self.n_floor(X, Y)
        n_wall = self.n_wall2d(X, Y)
        guard = self._guard
        # Relieve natural de fuera (v2): dunas y lagos de las vistas mas lomas cuya altura cambia a
        # lo largo del camino y se apagan lejos de el. No depende de la distancia al camino, asi que
        # la pared acaba donde acaba cada loma (no a una distancia fija) y la cima no es meseta.
        v = field.vista(self, X, Y)
        # Junto al camino, el terreno parte de su cota (el camino va entre lomas, no en una calzada
        # elevada sobre las dunas) y se funde con las vistas a una distancia que cambia.
        at = [(X - self.axis[0]) / STEP_XY_M, (Y - self.axis[0]) / STEP_XY_M]
        fade = ndimage.map_coordinates(self._fade_grid, at, order=1, mode="nearest")
        base = ndimage.map_coordinates(self._zsoft_grid, at, order=1, mode="nearest")
        natural = v + (np.maximum(base - v, 0.0) + self._hills(X, Y)) * fade
        lo_b, hi_b = self.style.block_band_m
        rim_top = lo_b + (hi_b - lo_b) * self.n_band.unit(X, Y)
        # Remate de la pared de TODOS los caminos cercanos (maximo continuo, sin saltos en las
        # bisectrices): pie + algo de roca sobre su suelo y caida variable hacia el relieve de fuera.
        outer = np.maximum(natural, self._rim_envelope(X, Y, rim_top, n_top))
        # Sin aristas ni pinchos: el maximo de dos laderas deja una cresta en cuchilla donde se
        # cruzan; se redondea (el pie vertical sale de la seccion, no de aqui).
        outer = ndimage.gaussian_filter(outer, 1.2, mode="nearest")
        height, H, crest_e = field.section(e, zf, z_soft, w, bw, n_rim, n_top, n_floor, self.style, guard, n_wall,
                                          H_in=outer)
        zf2, w2 = self.S["z"][i2], self.S["w"][i2]
        e2 = self._blunt_wedges(X, Y, d2 - w2, i2)
        height2, _, _ = field.section(e2, zf2, z_soft, w2, bw, n_rim, n_top, n_floor, self.style, guard, n_wall,
                                      H_in=outer)
        # Solo entre caminos a cota parecida: con cotas distintas, promediar la pared de uno con el
        # suelo del otro dejaba una rampa por la que se salia a las vistas.
        mix = 0.5 * (1.0 - smooth(0.0, 3.0, gap)) * (e > 0.0) * (e2 > 0.0) * (1.0 - smooth(0.5, 1.0, np.abs(zf - zf2)))
        height = height * (1.0 - mix) + height2 * mix
        height = self._inside_corridor(X, Y, height, e, i, zf, w, bw)
        # Pasado lo alto de la pared, el relieve natural (nunca dentro del camino ni en el pie).
        t = smooth(crest_e + rim_top + 2.0, crest_e + rim_top + 12.0, e)
        height = self._stamps(X, Y, height, e, i, zf, w)
        if self.canyon_field is not None:
            height = self.canyon_field.carve(X, Y, height, np.zeros(shape))
        protect = 1.0 - t
        height, coast = field.shore(self, X, Y, height, protect)
        height = field.clip_spikes(height)
        region = np.where(e < 0.0, 0, np.where(e < crest_e + rim_top + 4.0, 1, 2))
        region = np.where(coast > 0.5, 3, region)
        region = np.where(self.decks.near(X, Y), 0, region)          # tableros: camino
        tunnel = self._tunnel_zone(X, Y)
        region = np.where(tunnel > 0.5, 0, region)                  # suelo de los tuneles: camino
        if shape == (len(getattr(self, "axis", [])),) * 2:
            self.region = region
        # Voladizos y huecos suaves en todas las paredes (antes solo en el acantilado).
        wall_band = (1.0 - smooth(0.0, 3.0, np.abs(e - 0.5 * crest_e))) * (0.5 + 0.5 * bw[..., 0])
        # Sin huecos donde la pared es fina (dos caminos a menos de 16 m, uno junto al otro) ni junto
        # a los puentes: ahi el ruido perforaba la pared y dejaba dientes.
        thick = smooth(10.0, 16.0, np.where(np.isfinite(gap), e + e2, 99.0))
        wall_band = wall_band * thick * (1.0 - self.decks.near(X, Y, 10.0))
        path = 1.0 - smooth(-1.0, 0.5, e)
        weights = {key: bw[..., b] for b, key in enumerate(ZONE_KEYS)}
        weights["canyon"] = np.zeros(shape)
        zeros = np.zeros(shape)
        return Fields(weights, d, zeros, height, zf, np.clip(wall_band, 0, 1), zeros, tunnel, np.clip(path, 0, 1))

    def _fade_field(self, X, Y):
        """Peso 0..1 de las lomas junto al camino en la rejilla del mapa: se apagan a una distancia
        del borde que cambia (8-43 m). Suavizado: la distancia al camino tiene aristas en las
        bisectrices y, sin suavizar, marcaba lineas rectas en el relieve."""
        z_soft, _ = self._soft_levels(X, Y)
        reach = 8.0 + 35.0 * self.n_run.unit(X, Y)
        # La cota del camino mas cercano tambien salta donde un mismo camino se curva sobre si mismo.
        self._zsoft_grid = ndimage.gaussian_filter(z_soft, 6.0, mode="nearest")
        return ndimage.gaussian_filter(1.0 - smooth(reach, reach + 25.0, self._cover), 5.0, mode="nearest")

    def _rim_envelope(self, X, Y, rim_top, n_top):
        """Maximo, entre todos los caminos, de su remate: su suelo + 4-6 m hasta rim_top m pasado el
        pie y, despues, caida de 25-60 grados. Continuo (maximo de funciones continuas)."""
        pts = np.stack([np.ravel(X), np.ravel(Y)], axis=1)
        rim_h = (field.FOOT_M + 1.0 + 2.0 * self.n_rim.unit(X, Y)).ravel()
        lo_a, hi_a = field.RIM_FALL_DEG
        tan = np.tan(np.radians(lo_a + (hi_a - lo_a) * (0.5 + 0.5 * np.ravel(n_top))))
        reach = 1.5 + np.ravel(rim_top)
        out = np.full(len(pts), -1e3)
        for tree, idx in self.line_trees.values():
            d, k = tree.query(pts)
            c = d - self.S["w"][idx[k]]
            # Lo alto no es un rellano: baja ya un 30 % desde el borde y, pasado reach, cae entero.
            drop = 0.3 * np.clip(c, 0.0, reach) + np.maximum(c - reach, 0.0) * tan
            out = np.maximum(out, self.S["z"][idx[k]] + rim_h - drop)
        return out.reshape(X.shape)

    def _hills(self, X, Y):
        """Lomas junto al camino: altura de escala grande (a veces casi nada, a veces alta), picos
        de escala media y rugosidad fina. Sin forma fija."""
        lo, hi = self.style.hill_amp_m
        rough = self.style.crest_roughness
        amp = lo + (hi - lo) * smooth(0.3, 0.75, self.n_crest_big.unit(X, Y))
        shape = 0.25 + 0.75 * smooth(0.2, 0.9, self.n_crest_mid.unit(X, Y))
        return amp * shape + 0.9 * rough * self.n_crest_fine.unit(X, Y)

    def _soft_levels(self, X, Y):
        """(cota suavizada, pesos de bioma) mezclando el punto mas cercano de CADA camino con un
        peso que cae con la distancia: continuos tambien en las bisectrices entre caminos (con las
        24 muestras mas cercanas saltaban, porque todas eran del mismo camino)."""
        pts = np.stack([np.ravel(X), np.ravel(Y)], axis=1)
        dists, zs, biomes = [], [], []
        reach = self.style.block_band_m[1]
        guard = np.full(len(pts), -1e3)
        cover = np.full(len(pts), np.inf)          # distancia al borde del camino mas cercano (continua)
        for tree, idx in self.line_trees.values():
            d, k = tree.query(pts)
            dists.append(d)
            zs.append(self.S["z"][idx[k]])
            biomes.append(self.S["biome"][idx[k]])
            # Suelo del camino mas alto de los cercanos: la cresta no baja de el (continuo). Solo muy
            # cerca (v2): mas lejos creaba una banda de altura fija alrededor de cada camino.
            near = 1.0 - smooth(reach, reach + 6.0, d - self.S["w"][idx[k]])
            guard = np.maximum(guard, self.S["z"][idx[k]] - 100.0 * (1.0 - near))
            cover = np.minimum(cover, d - self.S["w"][idx[k]])
        d = np.stack(dists, axis=1)
        wz = np.exp(-0.5 * ((d - d.min(axis=1, keepdims=True)) / 10.0) ** 2)
        z_soft = (np.stack(zs, axis=1) * wz).sum(axis=1) / wz.sum(axis=1)
        wb = np.exp(-0.5 * ((d - d.min(axis=1, keepdims=True)) / 15.0) ** 2)
        bw = (np.eye(4)[np.stack(biomes, axis=1)] * wb[..., None]).sum(axis=1)
        bw = bw / bw.sum(axis=1, keepdims=True)
        self._guard = guard.reshape(X.shape)
        self._cover = cover.reshape(X.shape)
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
            below_top = smooth(1.0, 3.0, f.height[..., None] - Z3)
            D = D + self.wall_noise * band * above * below_top * self.n_wall3d(X3, Y3, Z3)
        if self.tunnel_tree is not None and np.any(f.tunnel > 0.0):
            carve, v = self._carve(X, Y, Z3)
            # Paredes y techo rugosos; el suelo del tunel, llano.
            carve = carve + 0.5 * self.n_wall3d(X3 * 1.7, Y3 * 1.7, Z3 * 1.7) * (carve > -0.5) * smooth(0.3, 1.2, v)
            D = np.minimum(D, -carve)
        # Tableros despues de excavar: el hueco no se come la losa, y se funden con el terreno.
        return bridge.fuse(D, self.decks.density(X, Y, Z3))


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

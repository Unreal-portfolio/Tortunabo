"""Tableros 3D: puentes finos de los cruces y arcos naturales sobre el camino.

Un tablero es una losa de roca que sigue un eje (polilinea): cara de arriba a la cota 'top',
grosor 'thick' en el centro de la luz y, pasada la luz, baja hasta el suelo como estribo. Se suma
a la densidad DESPUES de excavar (los huecos de los tuneles ya no se lo comen) con un maximo suave,
asi que se funde con las paredes y no se lee como una pieza pegada.

- Puente de un cruce "bridge": el eje es el camino de arriba. El camino de abajo queda al aire
  libre (su corredor corta el del de arriba) y el de arriba se estrecha hasta el ancho del tablero.
- Arco natural: el eje cruza el camino de pared a pared; el camino sigue por debajo.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np
from scipy.spatial import cKDTree

from terrain_vol.density import smooth

from .curves import resample

DECK_HALF_M = 1.75           # semiancho del tablero (3,5 m de ancho)
DECK_THICK_M = 1.7           # grosor de la losa en el centro de la luz
APPROACH_M = 6.0             # tramo del camino de arriba que se estrecha antes de la luz
ABUTMENT_M = 5.0             # losa que se mete en la pared a cada lado (estribo)
ARCH_TOP_M = 7.0             # cara de arriba de un arco natural sobre el suelo del camino
FUSE_K = 1.5                 # anchura del maximo suave con el terreno


@dataclass(frozen=True)
class Deck:
    pts: np.ndarray          # (N, 2) eje, cada 0,5 m
    along: np.ndarray        # (N,) distancia con signo al centro de la luz, sobre el eje
    top: np.ndarray          # (N,) cota de la cara de arriba
    clear_half: float        # media luz: hasta aqui la losa es fina; despues, estribo
    floor: float             # suelo de debajo (el estribo baja hasta aqui)
    half: float = DECK_HALF_M
    thick: float = DECK_THICK_M


def crossing_deck(model, c) -> tuple[Deck, tuple[int, float, float]]:
    """Tablero de un cruce "bridge" y el tramo (linea, s0, s1) del camino de arriba que sustituye."""
    plan = model.plan
    up, lo = plan.graph.lines[c.upper], plan.graph.lines[c.lower]
    w_lo = float(np.interp(c.s_lower, lo.arc, plan.profiles[c.lower].half_width))
    # Luz = anchura del corredor de abajo medida a lo largo del de arriba (el cruce no es recto).
    t_up, t_lo = up.tangent_at(c.s_upper), lo.tangent_at(c.s_lower)
    sin = max(abs(float(t_up[0] * t_lo[1] - t_up[1] * t_lo[0])), 0.5)
    clear = w_lo / sin + 2.0
    reach = clear + ABUTMENT_M
    s0, s1 = max(c.s_upper - reach, 0.0), min(c.s_upper + reach, up.length)
    sel = (up.arc >= s0) & (up.arc <= s1)
    pts, s = resample(up.points[sel], 0.5)
    s = s + float(up.arc[sel][0])
    top = np.interp(s, up.arc, plan.profiles[c.upper].z)
    floor = float(np.interp(c.s_lower, lo.arc, plan.profiles[c.lower].z))
    deck = Deck(pts, s - c.s_upper, top, clear, floor)
    return deck, (c.upper, c.s_upper - clear, c.s_upper + clear)


def arch_deck(line, prof, s: float) -> Deck:
    """Arco natural: losa de pared a pared, perpendicular al camino en s."""
    w = float(np.interp(s, line.arc, prof.half_width))
    floor = float(np.interp(s, line.arc, prof.z))
    reach = w + 2.0 + ABUTMENT_M
    a = np.arange(-reach, reach + 0.25, 0.5)
    pts = line.point_at(s)[None, :] + a[:, None] * line.normal_at(s)[None, :]
    return Deck(pts, a, np.full(len(a), floor + ARCH_TOP_M), w + 2.0, floor, half=2.0, thick=1.9)


def narrow(half_width: np.ndarray, arc: np.ndarray, s_mid: float, clear: float) -> np.ndarray:
    """Semiancho del camino de arriba: baja al del tablero en la luz y en APPROACH_M antes."""
    d = np.abs(arc - s_mid)
    k = 1.0 - smooth(clear, clear + APPROACH_M, d)
    return half_width * (1.0 - k) + DECK_HALF_M * k


class DeckSet:
    """Consulta de todos los tableros de un mapa."""

    def __init__(self, decks: list[Deck], noise):
        self.decks = decks
        self.noise = noise
        if decks:
            self.tree = cKDTree(np.vstack([d.pts for d in decks]))
            owner = np.concatenate([np.full(len(d.pts), k) for k, d in enumerate(decks)])
            self.cat = {"owner": owner,
                        "along": np.concatenate([d.along for d in decks]),
                        "top": np.concatenate([d.top for d in decks])}

    def density(self, X, Y, Z3) -> np.ndarray | None:
        """> 0 dentro de alguna losa; None si ningun punto de la rejilla queda cerca."""
        if not self.decks:
            return None
        pts = np.stack([np.ravel(X), np.ravel(Y)], axis=1)
        u, k = self.tree.query(pts, distance_upper_bound=6.0)
        near = np.isfinite(u)
        if not near.any():
            return None
        k = np.where(near, k, 0)
        owner = self.cat["owner"][k]
        half = np.array([d.half for d in self.decks])[owner]
        thick = np.array([d.thick for d in self.decks])[owner]
        clear = np.array([d.clear_half for d in self.decks])[owner]
        floor = np.array([d.floor for d in self.decks])[owner]
        a = np.abs(self.cat["along"][k])
        top = self.cat["top"][k]
        # Cara de abajo en arco: fina en el centro de la luz, mas gruesa hacia los lados y, pasada
        # la luz, estribo hasta el suelo.
        span = np.maximum(clear, 1.0)
        arch = thick + 1.2 * (a / span) ** 4
        pier = smooth(clear + 0.5, clear + 3.5, a)
        under = top - (arch * (1.0 - pier) + (top - floor + 1.0) * pier)
        shape = X.shape
        u = np.where(near, u, 99.0).reshape(shape)[..., None]
        top3, under3 = top.reshape(shape)[..., None], under.reshape(shape)[..., None]
        half3 = half.reshape(shape)[..., None]
        rough = 0.35 * self.noise(X[..., None] * 1.3, Y[..., None] * 1.3, Z3 * 1.3)
        inside = np.minimum(np.minimum(half3 + rough - u, top3 - Z3), Z3 - under3 + rough)
        return np.where(near.reshape(shape)[..., None], inside, -100.0)


def fuse(D: np.ndarray, deck_D: np.ndarray | None) -> np.ndarray:
    """Maximo suave polinomico: igual al maximo exacto cuando las dos densidades difieren en mas
    de FUSE_K (lejos del tablero el terreno no se mueve); cerca, redondea la union (sin arista)."""
    if deck_D is None:
        return D
    h = np.maximum(FUSE_K - np.abs(D - deck_D), 0.0) / FUSE_K
    return np.maximum(D, deck_D) + h * h * FUSE_K * 0.25

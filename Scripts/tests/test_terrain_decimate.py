"""Decimacion de los trozos (terrain_vol/decimate.py): mide triangulos, bytes TNTM2 y error antes y
despues sobre dos trozos vecinos de C01 y un trozo de la corona, y comprueba que el borde (costura
con el vecino) no se mueve, que no se abren grietas y que el color y la luz se conservan.

Necesita pyfqmr (uv run ... --with pyfqmr); sin el se salta."""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

pytest.importorskip("pyfqmr")

from terrain_vol.decimate import (DEFAULT_MAX_ERROR_M, closest_points, cut_along, decimate_chunks,  # noqa: E402
                                  decimate_mesh, hausdorff, transfer)
from terrain_vol.export import read_chunk, write_chunk  # noqa: E402
from terrain_vol.layout import UU_PER_M  # noqa: E402

HALF_UU = 5000.0          # medio trozo de 100 m, en uu
QUANT_UU = 0.1            # holgura de la cuantizacion de TNTM2 (16 bits sobre el trozo)


@pytest.fixture(scope="module")
def model():
    from terrain_path.model import PathModel
    from terrain_path.style import C01_SEED, C01_STYLE
    return PathModel(C01_SEED, C01_STYLE)


@pytest.fixture(scope="module")
def pair(model):
    """Trozos (1, 1) y (2, 1) de C01, completos y decimados."""
    from terrain_vol.mesh import build_chunk
    full = {key: build_chunk(model, *key) for key in ((1, 1), (2, 1))}
    return full, decimate_chunks(full, DEFAULT_MAX_ERROR_M)


def _open_edges(tris: np.ndarray) -> np.ndarray:
    e = np.sort(np.concatenate([tris[:, [0, 1]], tris[:, [1, 2]], tris[:, [2, 0]]]), axis=1).astype(np.int64)
    u, count = np.unique(e, axis=0, return_counts=True)
    return u[count == 1]


def _on_plane(v: np.ndarray) -> np.ndarray:
    return (np.abs(np.abs(v[:, 0]) - HALF_UU) < 1e-2) | (np.abs(np.abs(v[:, 1]) - HALF_UU) < 1e-2)


def test_cut_along_abre_solo_las_aristas_de_costura():
    # Cuadrado de dos triangulos con la diagonal (0, 2) como costura: queda abierto por ella.
    faces = np.array([[0, 1, 2], [0, 2, 3]])
    cut, source = cut_along(faces, 4, np.array([[0, 2]]))
    assert len(source) == 6 and sorted(source.tolist()) == [0, 0, 1, 2, 2, 3]
    assert len(_open_edges(cut)) == 6
    same, _ = cut_along(faces, 4, np.zeros((0, 2), dtype=np.int64))
    assert len(_open_edges(same)) == 4


def test_menos_triangulos_con_error_acotado(pair):
    full, dec = pair
    for key in full:
        a, b = full[key], dec[key]
        ratio = len(b.triangles) / len(a.triangles)
        err_cm = hausdorff(a.vertices.astype(np.float64), a.triangles.astype(np.int64),
                           b.vertices.astype(np.float64), b.triangles.astype(np.int64), 3.0)
        print(f"trozo {key}: {len(a.triangles)} -> {len(b.triangles)} triangulos ({ratio:.0%}), error {err_cm:.2f} cm")
        assert ratio <= 0.6, f"trozo {key}: solo baja al {ratio:.0%}"
        assert err_cm <= DEFAULT_MAX_ERROR_M * UU_PER_M + 1e-3


def test_el_borde_del_trozo_no_se_mueve(pair):
    full, dec = pair
    for key in full:
        a, b = full[key], dec[key]
        ka = {tuple(p) for p in a.vertices[_on_plane(a.vertices)].tolist()}
        kb = {tuple(p) for p in b.vertices[_on_plane(b.vertices)].tolist()}
        assert ka == kb, f"trozo {key}: {len(ka ^ kb)} vertices de borde distintos"


def test_la_costura_con_el_vecino_sigue_exacta(pair):
    _, dec = pair
    left, right = dec[(1, 1)], dec[(2, 1)]            # (col, fila): vecinos en Y (Este)
    sa = np.isclose(left.vertices[:, 1], HALF_UU, atol=1e-2)
    sb = np.isclose(right.vertices[:, 1], -HALF_UU, atol=1e-2)
    assert sa.sum() > 0 and sa.sum() == sb.sum()
    a, b = left.vertices[sa], right.vertices[sb]
    oa, ob = np.lexsort((a[:, 2], a[:, 0])), np.lexsort((b[:, 2], b[:, 0]))
    assert np.array_equal(a[oa][:, [0, 2]], b[ob][:, [0, 2]])
    assert np.allclose(left.normals[sa][oa], right.normals[sb][ob], atol=1e-6)
    assert np.array_equal(left.colors[sa][oa], right.colors[sb][ob])


def test_sin_grietas_nuevas(pair):
    """Todo borde abierto tras decimar ya era borde abierto antes (plano del trozo o fondo)."""
    full, dec = pair
    for key in full:
        a, b = full[key], dec[key]
        was = {tuple(p) for p in a.vertices[np.unique(_open_edges(a.triangles.astype(np.int64)))].tolist()}
        now = b.vertices[np.unique(_open_edges(b.triangles.astype(np.int64)))]
        new = [p for p in now.tolist() if tuple(p) not in was]
        assert not new, f"trozo {key}: {len(new)} vertices de borde nuevos (grieta)"


def test_color_y_luz_se_conservan(pair):
    full, dec = pair
    for key in full:
        a, b = full[key], dec[key]
        va, vb = a.vertices.astype(np.float64), b.vertices.astype(np.float64)
        vals = transfer(va, vb, b.triangles.astype(np.int64), np.concatenate([b.normals, b.colors], axis=1), 3.0)
        color = np.abs(vals[:, 3:] - a.colors).max(axis=1)
        n = vals[:, :3] / np.linalg.norm(vals[:, :3], axis=1, keepdims=True)
        angle = np.degrees(np.arccos(np.clip((n * a.normals).sum(axis=1), -1.0, 1.0)))
        print(f"trozo {key}: color p99 {np.percentile(color, 99):.1f} max {color.max():.1f} (0-255), "
              f"normal p99 {np.percentile(angle, 99):.1f} grados")
        assert color.max() <= 16.0 and np.percentile(color, 99) <= 6.0
        assert np.percentile(angle, 99) <= 10.0


def test_las_caras_miran_igual(pair):
    """La decimacion no gira caras: la cara visible de Unreal (opuesta a (B-A)x(C-A)) va con la normal."""
    _, dec = pair
    for key, b in dec.items():
        v, t = b.vertices.astype(np.float64), b.triangles.astype(np.int64)
        face = -np.cross(v[t[:, 1]] - v[t[:, 0]], v[t[:, 2]] - v[t[:, 0]])
        agree = np.einsum("ij,ij->i", face, b.normals[t].mean(axis=1)) > 0.0
        assert agree.mean() >= 0.99, f"trozo {key}: {1 - agree.mean():.1%} caras giradas"


def test_el_fichero_tntm2_pesa_menos_y_se_lee(pair, tmp_path):
    full, dec = pair
    before = after = 0
    for key in full:
        write_chunk(tmp_path / "a.bin", full[key])
        write_chunk(tmp_path / "b.bin", dec[key])
        before += (tmp_path / "a.bin").stat().st_size
        after += (tmp_path / "b.bin").stat().st_size
        back = read_chunk(tmp_path / "b.bin")
        assert len(back["triangles"]) == len(dec[key].triangles)
        assert np.abs(back["vertices"] - dec[key].vertices).max() <= QUANT_UU
    print(f"TNTM2: {before} -> {after} bytes ({after / before:.0%})")
    assert after <= 0.7 * before


def test_la_corona_se_decima_y_casa_en_el_borde(model):
    from terrain_path.layout import MAP_MAX_M, MAP_MIN_M
    from terrain_path.outer import DECIMATE_M, OUTER_M, _cell_mesh
    # Esquina de dunas lejana y trozo del mar al norte (casi llano: se queda en nada).
    for (x0, y0), most in (((MAP_MIN_M - OUTER_M, MAP_MIN_M - OUTER_M), 0.75), ((MAP_MAX_M, MAP_MIN_M), 0.2)):
        _, a = _cell_mesh(model, x0, y0)
        _, b = _cell_mesh(model, x0, y0, decimate_m=DECIMATE_M)
        err_cm = hausdorff(a.vertices.astype(np.float64), a.triangles.astype(np.int64),
                           b.vertices.astype(np.float64), b.triangles.astype(np.int64), 15.0)
        print(f"corona ({x0}, {y0}): {len(a.triangles)} -> {len(b.triangles)} triangulos, error {err_cm:.2f} cm")
        assert len(b.triangles) <= most * len(a.triangles) and err_cm <= DECIMATE_M * UU_PER_M + 1e-3
        edge = np.unique(_open_edges(a.triangles.astype(np.int64)))
        kept = {tuple(p) for p in b.vertices.tolist()}
        assert all(tuple(p) in kept for p in a.vertices[edge].tolist()), "el borde de la corona se ha movido"


def test_closest_points_encuentra_triangulos_grandes():
    """Un triangulo enorme tiene el centro lejos del punto que cubre: la busqueda exacta lo encuentra."""
    rng = np.random.default_rng(0)
    small = rng.uniform(-1.0, 1.0, (300, 3)) + np.array([0.0, 0.0, 5.0])
    v = np.concatenate([np.array([[-100.0, -100.0, 0.0], [100.0, -100.0, 0.0], [0.0, 200.0, 0.0]]), small])
    f = np.concatenate([[[0, 1, 2]], 3 + np.arange(300).reshape(100, 3)])
    d, tri, _ = closest_points(np.array([[90.0, -95.0, 0.01]]), v, f, exact_above=0.05, k=4)
    assert tri[0] == 0 and d[0] == pytest.approx(0.01)


def test_decimate_mesh_sin_recorte_devuelve_lo_mismo():
    v = np.array([[0, 0, 0], [100, 0, 0], [0, 100, 0], [100, 100, 0]], dtype=np.float32)
    t = np.array([[0, 2, 1], [1, 2, 3]], dtype=np.uint32)
    n = np.tile(np.array([[0, 0, 1]], np.float32), (4, 1))
    c = np.full((4, 4), 200, np.uint8)
    out = decimate_mesh(v, n, c, t)
    assert out[0] is v and out[3] is t

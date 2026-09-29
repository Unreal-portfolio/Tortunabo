"""Decimacion de los trozos con error acotado: menos triangulos que dibujar y que cocinar como
colision (el cargador usa la malla entera como colision compleja) sin mover la silueta mas de
max_error_m.

- Colapso de aristas por cuadricas (pyfqmr, Fast Quadric Mesh Simplification) con el borde abierto
  fijo: los vertices del plano de corte del trozo no se mueven y la costura con el vecino sigue
  exacta, vertice a vertice.
- Costuras de color: antes de decimar se abre la malla por las aristas cuyo color de vertice cambia
  mas de color_step (borde del camino, orilla mojada, vetas de pared). Esos vertices quedan como
  borde y no se mueven, asi que la franja del camino no se emborrona al agrandar los triangulos.
  Despues se vuelven a soldar (sus posiciones no han cambiado: la soldadura es exacta).
- El error se mide en los dos sentidos (vertices originales contra la malla nueva y al reves) y la
  proporcion de triangulos se busca por biseccion hasta el mayor recorte que cumple max_error_m.

decimate() trabaja en las unidades que se le den; decimate_mesh() recibe la malla de un trozo tal
como se exporta (uu locales al centro del trozo) y el error en metros.
"""

from __future__ import annotations

import numpy as np
from scipy import sparse
from scipy.sparse.csgraph import connected_components
from scipy.spatial import cKDTree

DEFAULT_MAX_ERROR_M = 0.05          # 5 cm: menos que el radio de la capsula entre dos pasos de la tortuga
DEFAULT_COLOR_STEP = 10             # salto de color (0-255, cualquier canal RGBA) que marca costura
BISECT_STEPS = 7
MIN_RATIO = 0.02


def _closest_on_triangles(p: np.ndarray, a: np.ndarray, b: np.ndarray, c: np.ndarray) -> np.ndarray:
    """Punto de los triangulos (a, b, c) mas cercano a p, vectorizado (Ericson, Real-Time Collision
    Detection 5.1.5)."""
    ab, ac, ap = b - a, c - a, p - a
    d1, d2 = (ab * ap).sum(-1), (ac * ap).sum(-1)
    bp = p - b
    d3, d4 = (ab * bp).sum(-1), (ac * bp).sum(-1)
    cp = p - c
    d5, d6 = (ab * cp).sum(-1), (ac * cp).sum(-1)
    va, vb, vc = d3 * d6 - d5 * d4, d5 * d2 - d1 * d6, d1 * d4 - d3 * d2
    denom = 1.0 / np.maximum(va + vb + vc, 1e-30)
    res = a + ab * (vb * denom)[..., None] + ac * (vc * denom)[..., None]

    def safe(x):
        return np.where(np.abs(x) > 1e-30, x, 1e-30)

    t = d1 / safe(d1 - d3)
    res = np.where(((vc <= 0) & (d1 >= 0) & (d3 <= 0))[..., None], a + ab * t[..., None], res)
    t = d2 / safe(d2 - d6)
    res = np.where(((vb <= 0) & (d2 >= 0) & (d6 <= 0))[..., None], a + ac * t[..., None], res)
    e43, e56 = d4 - d3, d5 - d6
    t = e43 / safe(e43 + e56)
    res = np.where(((va <= 0) & (e43 >= 0) & (e56 >= 0))[..., None], b + (c - b) * t[..., None], res)
    res = np.where(((d1 <= 0) & (d2 <= 0))[..., None], a, res)
    res = np.where(((d3 >= 0) & (d4 <= d3))[..., None], b, res)
    return np.where(((d6 >= 0) & (d5 <= d6))[..., None], c, res)


def closest_points(points: np.ndarray, vertices: np.ndarray, faces: np.ndarray, exact_above: float,
                   k: int = 16, max_exact: int = 400) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """(distancia, triangulo, punto) de la malla mas cercano a cada punto. Candidatos: los k
    triangulos de centro mas cercano, luego 8 k; los que siguen por encima de exact_above se rehacen
    contra todos (un triangulo grande puede tener el centro lejos del punto que cubre), hasta
    max_exact puntos: pasado eso la distancia queda por exceso (nunca por defecto)."""
    n = len(points)
    dist, tri, near = np.zeros(n), np.zeros(n, dtype=np.int64), np.array(points, dtype=np.float64)
    if n == 0:
        return dist, tri, near
    if len(faces) == 0:
        return np.full(n, np.inf), tri, near
    tree = cKDTree(vertices[faces].mean(axis=1))
    todo = np.arange(n)
    for kk in (k, 8 * k):
        if len(todo) == 0:
            break
        _, idx = tree.query(points[todo], k=min(kk, len(faces)))
        idx = idx.reshape(len(todo), -1)
        t = vertices[faces[idx]]
        cp = _closest_on_triangles(points[todo][:, None, :], t[..., 0, :], t[..., 1, :], t[..., 2, :])
        d = np.linalg.norm(cp - points[todo][:, None, :], axis=-1)
        best = d.argmin(axis=1)
        rows = np.arange(len(todo))
        dist[todo], tri[todo], near[todo] = d[rows, best], idx[rows, best], cp[rows, best]
        todo = todo[dist[todo] > exact_above]
    if len(todo) <= max_exact:
        a, b, c = vertices[faces[:, 0]], vertices[faces[:, 1]], vertices[faces[:, 2]]
        for i in todo:
            cp = _closest_on_triangles(points[i][None, :], a, b, c)
            d = np.linalg.norm(cp - points[i], axis=-1)
            j = int(d.argmin())
            dist[i], tri[i], near[i] = d[j], j, cp[j]
    return dist, tri, near


def surface_distance(points: np.ndarray, vertices: np.ndarray, faces: np.ndarray, exact_above: float) -> np.ndarray:
    return closest_points(points, vertices, faces, exact_above)[0]


def barycentric(p: np.ndarray, a: np.ndarray, b: np.ndarray, c: np.ndarray) -> np.ndarray:
    """(N, 3) pesos baricentricos de p (sobre el triangulo) en (a, b, c), recortados a [0, 1]."""
    v0, v1, v2 = b - a, c - a, p - a
    d00, d01, d11 = (v0 * v0).sum(1), (v0 * v1).sum(1), (v1 * v1).sum(1)
    d20, d21 = (v2 * v0).sum(1), (v2 * v1).sum(1)
    den = np.maximum(d00 * d11 - d01 * d01, 1e-20)
    w1 = np.clip((d11 * d20 - d01 * d21) / den, 0.0, 1.0)
    w2 = np.clip((d00 * d21 - d01 * d20) / den, 0.0, 1.0)
    w = np.stack([1.0 - w1 - w2, w1, w2], axis=1).clip(0.0, 1.0)
    return w / np.maximum(w.sum(axis=1, keepdims=True), 1e-20)


def transfer(points: np.ndarray, vertices: np.ndarray, faces: np.ndarray, values: np.ndarray,
             exact_above: float) -> np.ndarray:
    """Valores por vertice de la malla (vertices, faces) interpolados en su punto mas cercano a cada punto."""
    _, tri, near = closest_points(points, vertices, faces, exact_above)
    f = faces[tri]
    w = barycentric(near, vertices[f[:, 0]], vertices[f[:, 1]], vertices[f[:, 2]])
    return (values[f].astype(np.float64) * w[:, :, None]).sum(axis=1)


def hausdorff(v0: np.ndarray, f0: np.ndarray, v1: np.ndarray, f1: np.ndarray, exact_above: float) -> float:
    """Error geometrico maximo entre dos mallas, muestreado en los vertices de ambas."""
    return float(max(surface_distance(v0, v1, f1, exact_above).max(initial=0.0),
                     surface_distance(v1, v0, f0, exact_above).max(initial=0.0)))


def color_seam_edges(faces: np.ndarray, colors: np.ndarray, color_step: int) -> np.ndarray:
    """(E, 2) aristas (ordenadas) cuyos extremos cambian de color mas de color_step en algun canal."""
    edges = np.sort(np.concatenate([faces[:, [0, 1]], faces[:, [1, 2]], faces[:, [2, 0]]]), axis=1)
    edges = np.unique(edges, axis=0)
    c = colors.astype(np.int32)
    jump = np.abs(c[edges[:, 0]] - c[edges[:, 1]]).max(axis=1)
    return edges[jump > color_step]


def cut_along(faces: np.ndarray, count: int, seams: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """Abre la malla por las aristas seams: cada abanico de caras de un vertice separado por costuras
    recibe su propia copia. Devuelve (caras nuevas, vertice original de cada copia)."""
    m = len(faces)
    corner = np.arange(3 * m).reshape(m, 3)            # esquina (cara, k) -> nodo
    # Aristas interiores: la misma arista (ordenada) en dos caras. Sus dos extremos unen las esquinas.
    rows, cols = [], []
    for k0, k1 in ((0, 1), (1, 2), (2, 0)):
        rows.append(np.stack([faces[:, k0], faces[:, k1], np.arange(m), np.full(m, k0), np.full(m, k1)], axis=1))
    e = np.concatenate(rows)
    swap = e[:, 0] > e[:, 1]
    e[swap] = e[swap][:, [1, 0, 2, 4, 3]]
    order = np.lexsort((e[:, 1], e[:, 0]))
    e = e[order]
    same = (e[1:, 0] == e[:-1, 0]) & (e[1:, 1] == e[:-1, 1])
    first, second = e[:-1][same], e[1:][same]
    if len(seams):
        key = seams[:, 0].astype(np.int64) * count + seams[:, 1]
        k = first[:, 0].astype(np.int64) * count + first[:, 1]
        keep = ~np.isin(k, key)
        first, second = first[keep], second[keep]
    # extremo menor: esquinas (cara, k_lo); extremo mayor: esquinas (cara, k_hi)
    rows = np.concatenate([corner[first[:, 2], first[:, 3]], corner[first[:, 2], first[:, 4]]])
    cols = np.concatenate([corner[second[:, 2], second[:, 3]], corner[second[:, 2], second[:, 4]]])
    graph = sparse.coo_matrix((np.ones(len(rows)), (rows, cols)), shape=(3 * m, 3 * m))
    _, label = connected_components(graph, directed=False)
    new_ids, inverse = np.unique(label, return_inverse=True)
    source = np.zeros(len(new_ids), dtype=np.int64)
    source[inverse] = faces.ravel()
    return inverse.reshape(m, 3), source


def _simplify(vertices: np.ndarray, faces: np.ndarray, target: int) -> tuple[np.ndarray, np.ndarray]:
    import pyfqmr
    s = pyfqmr.Simplify()
    s.setMesh(vertices.astype(np.float64), faces.astype(np.int32))
    s.simplify_mesh(target_count=int(target), aggressiveness=7, preserve_border=True, verbose=False)
    v, f, _ = s.getMesh()
    return np.asarray(v, dtype=np.float64), np.asarray(f, dtype=np.int64)


def _weld(vertices: np.ndarray, faces: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """Suelda copias con la misma posicion exacta y quita vertices sin usar y caras degeneradas."""
    uniq, inverse = np.unique(vertices, axis=0, return_inverse=True)
    f = inverse.reshape(-1)[faces]
    f = f[(f[:, 0] != f[:, 1]) & (f[:, 1] != f[:, 2]) & (f[:, 2] != f[:, 0])]
    used = np.unique(f)
    remap = np.full(len(uniq), -1, dtype=np.int64)
    remap[used] = np.arange(len(used))
    return uniq[used], remap[f]


def decimate(vertices: np.ndarray, faces: np.ndarray, colors: np.ndarray | None = None,
             max_error: float = DEFAULT_MAX_ERROR_M, color_step: int = DEFAULT_COLOR_STEP,
             steps: int = BISECT_STEPS) -> tuple[np.ndarray, np.ndarray, float]:
    """Malla con el menor numero de triangulos (busqueda por biseccion) cuyo error maximo respecto a
    la original no pasa de max_error. Devuelve (vertices, caras, error). El borde abierto y las
    costuras de color no se mueven; el sentido de las caras se conserva."""
    v0 = np.asarray(vertices, dtype=np.float64)
    f0 = np.asarray(faces, dtype=np.int64)
    if len(f0) < 16 or max_error <= 0.0:
        return v0, f0, 0.0
    if colors is not None:
        cut_faces, source = cut_along(f0, len(v0), color_seam_edges(f0, colors, color_step))
    else:
        cut_faces, source = f0, np.arange(len(v0))
    cut_verts = v0[source]
    best, lo, hi = (v0, f0, 0.0), MIN_RATIO, 1.0
    for _ in range(steps):
        mid = 0.5 * (lo + hi)
        v, f = _weld(*_simplify(cut_verts, cut_faces, max(4, int(len(f0) * mid))))
        err = hausdorff(v0, f0, v, f, exact_above=0.6 * max_error)
        if err <= max_error:
            hi, best = mid, (v, f, err)
        else:
            lo = mid
    return best


def _spread10(x: np.ndarray) -> np.ndarray:
    x = x & np.uint64(0x3FF)
    for shift, mask in ((16, 0x30000FF), (8, 0x300F00F), (4, 0x30C30C3), (2, 0x9249249)):
        x = (x | (x << np.uint64(shift))) & np.uint64(mask)
    return x


def spatial_order(vertices: np.ndarray, triangles: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """(orden de vertices, triangulos renumerados): triangulos en orden de Morton de su centro y
    vertices por primer uso. Vecinos en el espacio quedan juntos en los arrays: los deltas de indice
    del TNTM2 son pequenos (zlib comprime ~10 % mejor) y la GPU reutiliza mas vertices de su cache."""
    t = np.asarray(triangles, dtype=np.int64)
    c = np.asarray(vertices, dtype=np.float64)[t].mean(axis=1)
    q = ((c - c.min(axis=0)) / (np.ptp(c, axis=0) + 1e-9) * 1023.0).astype(np.uint64)
    code = _spread10(q[:, 0]) | (_spread10(q[:, 1]) << np.uint64(1)) | (_spread10(q[:, 2]) << np.uint64(2))
    t = t[np.argsort(code, kind="stable")]
    flat = t.ravel()
    _, first = np.unique(flat, return_index=True)
    order = flat[np.sort(first)]
    remap = np.full(len(vertices), -1, dtype=np.int64)
    remap[order] = np.arange(len(order))
    return order, remap[t]


def decimate_mesh(vertices: np.ndarray, normals: np.ndarray, colors: np.ndarray, triangles: np.ndarray,
                  max_error_m: float = DEFAULT_MAX_ERROR_M, color_step: int = DEFAULT_COLOR_STEP,
                  uu_per_m: float = 100.0) -> tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    """Decima la malla de un trozo (vertices en uu). Los vertices que no se han movido (borde del
    trozo, costuras de color y los que no se han colapsado) conservan su normal y su color exactos:
    la costura con el vecino sale identica. El resto toma normal y color de la malla completa en su
    punto mas cercano (baricentricas), asi que la luz sigue a la superficie de 1 m."""
    v0 = np.asarray(vertices, dtype=np.float64)
    f0 = np.asarray(triangles, dtype=np.int64)
    max_error = max_error_m * uu_per_m
    v, f, _ = decimate(v0, f0, colors, max_error=max_error, color_step=color_step)
    if len(f) == len(f0):
        return vertices, normals, colors, triangles
    values = transfer(v, v0, f0, np.concatenate([normals, colors], axis=1).astype(np.float64), exact_above=max_error)
    out_n = values[:, :3] / np.maximum(np.linalg.norm(values[:, :3], axis=1, keepdims=True), 1e-9)
    out_c = np.clip(np.rint(values[:, 3:]), 0, 255)
    # Vertices que siguen en su sitio: copia exacta de la original.
    both = np.concatenate([v0, v])
    _, inverse = np.unique(both, axis=0, return_inverse=True)
    inverse = inverse.reshape(-1)
    first = np.full(inverse.max() + 1, -1, dtype=np.int64)
    first[inverse[:len(v0)][::-1]] = np.arange(len(v0))[::-1]
    src = first[inverse[len(v0):]]
    same = src >= 0
    out_n[same] = normals[src[same]]
    out_c[same] = colors[src[same]]
    order, f = spatial_order(v, f)
    return (v[order].astype(np.float32), out_n[order].astype(np.float32), out_c[order].astype(np.uint8),
            f.astype(np.uint32))


def decimate_chunks(chunks: dict, max_error_m: float = DEFAULT_MAX_ERROR_M) -> dict:
    """decimate_mesh() sobre cada trozo (ChunkMesh u objeto con los mismos campos); el resto de
    campos (cota, celdas pisables, campos del modelo) no cambia."""
    from dataclasses import is_dataclass, replace
    out = {}
    for key, chunk in chunks.items():
        v, n, c, t = decimate_mesh(chunk.vertices, chunk.normals, chunk.colors, chunk.triangles, max_error_m)
        out[key] = replace(chunk, vertices=v, normals=n, colors=c, triangles=t) if is_dataclass(chunk) else \
            type(chunk)(**{**vars(chunk), "vertices": v, "normals": n, "colors": c, "triangles": t})
    return out

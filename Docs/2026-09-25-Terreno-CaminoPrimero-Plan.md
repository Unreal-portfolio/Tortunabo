# Terreno «camino primero» — plan de implementación

> **Para agentes:** SUB-SKILL OBLIGATORIA: usa superpowers:subagent-driven-development (recomendado) o superpowers:executing-plans para implementar este plan tarea a tarea. Los pasos usan casillas (`- [ ]`) para el seguimiento.

**Objetivo:** generar el mapa `C01_camino`, de 400 × 400 m (4 × 4 trozos), cuyo terreno sale de un grafo de caminos:

- un camino principal, unos 7 lazos (algunos anidados) y cruces con puente natural o túnel;
- un tramo de río con islas alargadas;
- bordes que cierran el paso y vistas fuera del camino;
- final en abanico hasta el mar;
- agua cartoon.

**Arquitectura:**
- Paquete nuevo `Scripts/terrain_path/`, en capas:
  1. grafo 2D del camino;
  2. perfil de cota y anchura por camino;
  3. relieve 2D por distancia al camino (sección en «cuadrado curvado», borde por bioma, fondo de vistas);
  4. densidad 3D con túneles.
- Reutiliza de `terrain_vol` la malla (`mesh.build_chunk`), el formato TNTM2 y el manifest (`export`), el ruido (`noise`) y la búsqueda de recorrido (`gen_terrain_volume.walk`).
- `terrain_vol` no cambia de comportamiento: Mapa01 y las 30 variantes siguen siendo reproducibles.

**Stack:** Python 3.13 con uv (numpy, scipy, scikit-image, pillow), pytest, Unreal Engine 5.6 (Python de editor y el cargador C++ existente).

**Spec:** `Docs/Diseno_Terreno_CaminoPrimero.md` (aprobada el 2026-09-25).

## Restricciones globales

- Python siempre con uv:
  - `uv run pytest -q` desde la raíz del repo;
  - scripts con `uv run --with numpy --with scipy --with pillow --with scikit-image python <script>`;
  - nunca `python` ni `pip` a pelo.
- Mapa de **400 × 400 m**: `GRID = 4`, trozos de 100 m, `MAP_MIN_M = -50`, `MAP_MAX_M = 350`. X = Norte (el mar está al norte, X máxima) e Y = Este.
- `terrain_vol/` no cambia de resultado. Solo se admite añadir un parámetro opcional `grid` con valor por defecto igual al actual.
- Cota del agua `WATER_M = -4.0` m. Rejilla de 1 m en horizontal y 0,5 m en vertical, cotas entre -10 y 34 m.
- **Sección del camino:** suelo llano y «cuadrado curvado» (esquina inferior de radio `min(1,5 m, 0,2 × semiancho)` y pared de 55-78° según bioma). **Nunca un cuenco.**
- Pendiente del camino ≤ 20 % en general y hasta el 30 % en tramos cortos. Separación vertical en los cruces ≥ 8,5 m.
- Semiancho del camino: 2,5 m / 4 m / 8 m (mínimo / moda / máximo). Río: 4-8 m.
- Todo arena, sin verde. Todo el azar sale de `np.random.default_rng(seed)` del modelo, en un orden fijo: misma semilla, mismo mapa.
- **Salida:** `Scripts/terrain_volumes/Variants/C01_camino/`, con una entrada al principio de `Variants/index.json`. El cargador C++ ya coloca los trozos según `center_uu` del manifest, sea cual sea el tamaño, así que no hay nueva carpeta ni índice propio.
  - Desviación de la spec §3, que proponía `PathMaps/`: se descarta porque obligaría a tocar el C++ sin ganar nada.
- Comentarios de código sin tildes, como el resto de `Scripts/`. Documentación y commits en español con ortografía completa. Commits sin `Co-Authored-By`.
- Commits pequeños, uno por tarea. **Sin push** hasta que Rodrigo valide C01.
- El editor de Unreal debe estar cerrado para los scripts `-run=pythonscript`: preguntar antes de cerrarlo.

## Fallos que los tests no cubren (a vigilar)

Estos son los cinco fallos más probables que ningún test de las tareas ejercita todavía. Cada uno tiene su test en la tarea que lo posee:

1. **Escalón donde se unen dos pasillos** de cotas distintas: la bisectriz entre corridores no puede dejar un escalón intransitable. → Tarea 6, `test_la_union_de_un_lazo_no_tiene_escalon`.
2. **Techo del túnel perforado** en un cruce (el camino de arriba roza al de abajo). → Tarea 8, `test_los_cruces_tienen_techo`.
3. **Un río que corta el camino** y deja la ruta sin paso en seco o saltando. → Tarea 10, `test_se_llega_a_pie_del_inicio_al_final` (recorrido sin pisar agua honda).
4. **Salir del camino a las vistas** por una cresta baja, sobre todo en playa y dunas. → Tarea 10, `test_las_vistas_no_se_pisan`.
5. **Un castillo de arena que tapona un estrechamiento.** → Tarea 9, `test_los_castillos_dejan_paso`.

---

## Mapa de ficheros

| Fichero | Responsabilidad |
|---|---|
| `Scripts/terrain_vol/export.py` (mod.) | `grid` opcional en `write_map`, `global_top` y `write_preview`. |
| `Scripts/gen_terrain_volume.py` (mod.) | `grid` opcional en `build_all`, `global_standable` y `zone_map`. |
| `Scripts/terrain_path/__init__.py` | Paquete vacío. |
| `Scripts/terrain_path/layout.py` | Rejilla 4 × 4. |
| `Scripts/terrain_path/style.py` | `PathStyle` (dataclass congelada) y `C01_STYLE`. |
| `Scripts/terrain_path/curves.py` | Remuestreo, tangentes, normales, ruido por nudos y detector de rectas. |
| `Scripts/terrain_path/graph.py` | `PathLine`, `Crossing`, `PathGraph` y trazado del principal, lazos y cruces. |
| `Scripts/terrain_path/profile.py` | Cota, anchura, bioma y túneles de cada camino (`LineProfile`, `PathPlan`). |
| `Scripts/terrain_path/field.py` | Sección del camino, fondo de vistas, costa y estampas (funciones puras). |
| `Scripts/terrain_path/river.py` | Plan del río (orillas que alternan e islas) y su relieve. |
| `Scripts/terrain_path/castles.py` | Plan y estampa de los castillos de arena. |
| `Scripts/terrain_path/model.py` | `PathModel`: campos 2D del mapa, densidad 3D y la interfaz que usan `mesh` y `export`. |
| `Scripts/gen_terrain_path.py` | CLI: genera `C01_camino` y actualiza `index.json`. |
| `Scripts/tests/test_terrain_path.py` | Tests del paquete. |
| `Scripts/build_grid_demo_assets.py` (mod.) | Guarda de `main()` y franja de arena mojada en `M_GridTerrain`. |
| `Scripts/build_water_toon.py` | Material `M_TortunaboWaterToon` y su montaje en `LVL_MapVariants`. |

Interfaz que `terrain_vol/mesh.py` exige al modelo (ya la cumple `MapModel`, y `PathModel` la replica):
- `chunk_fields(col, row, pad) -> (Fields, X, Y)`;
- `density(X, Y, Z, fields) -> ndarray`;
- `zones.weights(x, y) -> dict[str, ndarray]` con las claves `cliffs`, `canyon`, `marsh`, `algae` y `beach`;
- `trail_mask(x, y)`;
- `plaza_mask(x, y)`.

`Fields` es la dataclass de `terrain_vol/density.py`.

---

### Tarea 1: tamaño de mapa como parámetro en `export` y en las utilidades de recorrido

**Ficheros:**
- Modificar: `Scripts/terrain_vol/export.py` (`write_map`, `global_top`, `write_preview`).
- Modificar: `Scripts/gen_terrain_volume.py` (`build_all`, `global_standable`, `zone_map`).
- Test: `Scripts/tests/test_terrain_path.py` (nuevo).

**Interfaces:**
- Produce:
  - `build_all(model, grid: int = GRID)`;
  - `global_standable(chunks, grid: int = GRID)`;
  - `zone_map(model, chunks, grid: int = GRID)`;
  - `global_top(chunks, grid: int = GRID)`;
  - `write_preview(out, chunks, zone_map, route_points, grid: int = GRID)`;
  - `write_map(..., extra_manifest=None, grid: int = GRID)`, que escribe `"grid": grid`.

- [ ] **Paso 1: escribir el test que falla**

```python
"""Terreno «camino primero» (Docs/Diseno_Terreno_CaminoPrimero.md)."""

from __future__ import annotations

import sys
from pathlib import Path
from types import SimpleNamespace

import numpy as np
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from gen_terrain_volume import global_standable  # noqa: E402
from terrain_vol.export import global_top  # noqa: E402
from terrain_vol.layout import CELL_SAMPLES, Z_SAMPLES  # noqa: E402


def test_las_utilidades_aceptan_un_mapa_de_4x4():
    chunks = {(c, r): SimpleNamespace(standable=np.ones((CELL_SAMPLES, CELL_SAMPLES, Z_SAMPLES), bool),
                                      top=np.full((CELL_SAMPLES, CELL_SAMPLES), float(c + r)))
              for r in range(4) for c in range(4)}
    size = 4 * (CELL_SAMPLES - 1) + 1
    assert global_standable(chunks, grid=4).shape == (size, size, Z_SAMPLES)
    top = global_top(chunks, grid=4)
    assert top.shape == (size, size) and top[-1, -1] == 6.0
```

- [ ] **Paso 2: comprobar que falla**

Ejecutar: `uv run pytest -q Scripts/tests/test_terrain_path.py::test_las_utilidades_aceptan_un_mapa_de_4x4`
Esperado: FAIL con `TypeError: global_standable() got an unexpected keyword argument 'grid'`.

- [ ] **Paso 3: implementar**

En `Scripts/gen_terrain_volume.py`:

```python
def build_all(model: MapModel, grid: int = GRID) -> dict[tuple[int, int], ChunkMesh]:
    return {(col, row): build_chunk(model, col, row) for row in range(grid) for col in range(grid)}


def global_standable(chunks: dict[tuple[int, int], ChunkMesh], grid: int = GRID) -> np.ndarray:
    size = grid * (CELL_SAMPLES - 1) + 1
    out = np.zeros((size, size, Z_SAMPLES), dtype=bool)
    for (col, row), chunk in chunks.items():
        i0, j0 = row * (CELL_SAMPLES - 1), col * (CELL_SAMPLES - 1)
        out[i0:i0 + CELL_SAMPLES, j0:j0 + CELL_SAMPLES] |= chunk.standable
    return out


def zone_map(model: MapModel, chunks, grid: int = GRID) -> dict[str, np.ndarray]:
    size = grid * (CELL_SAMPLES - 1) + 1
    out = {z: np.zeros((size, size)) for z in chunks[(0, 0)].fields.weights}
    for (col, row), chunk in chunks.items():
        i0, j0 = row * (CELL_SAMPLES - 1), col * (CELL_SAMPLES - 1)
        for z, w in chunk.fields.weights.items():
            out[z][i0:i0 + CELL_SAMPLES, j0:j0 + CELL_SAMPLES] = w
    return out
```

En `Scripts/terrain_vol/export.py`:
- en `global_top`, cambiar la firma a `def global_top(chunks, grid: int = GRID) -> np.ndarray:` y usar `size = grid * (CELL_SAMPLES - 1) + 1`;
- en `write_preview`, cambiar la firma a `def write_preview(out, chunks, zone_map, route_points, grid: int = GRID) -> None:` y la primera línea a `top = global_top(chunks, grid)`;
- en `write_map`, añadir el parámetro `grid: int = GRID` al final, poner `"grid": grid` en el manifest y cambiar la última línea a `write_preview(out, chunks, zone_map, route_points, grid)`.

- [ ] **Paso 4: comprobar que pasa, y la suite existente también**

Ejecutar: `uv run pytest -q Scripts/tests/test_terrain_path.py Scripts/tests/test_terrain_volume.py`
Esperado: PASS (16 tests).

- [ ] **Paso 5: commit**

```bash
git add Scripts/terrain_vol/export.py Scripts/gen_terrain_volume.py Scripts/tests/test_terrain_path.py
git commit -m "refactor(tools): tamaño de mapa como parámetro en export y en las utilidades de recorrido"
```

---

### Tarea 2: rejilla 4 × 4, estilo y utilidades de curvas

**Ficheros:**
- Crear: `Scripts/terrain_path/__init__.py` (vacío), `layout.py`, `style.py` y `curves.py`.
- Test: `Scripts/tests/test_terrain_path.py`.

**Interfaces:**
- Produce:
  - `layout.GRID = 4`, `MAP_MIN_M = -50.0`, `MAP_MAX_M = 350.0`, `GRID_PAD = 1`;
  - reexporta `CELL_M`, `CELL_SAMPLES`, `STEP_XY_M`, `WATER_M`, `Z_MIN_M` y `Z_MAX_M`;
  - `style.PathStyle` y `style.C01_STYLE`;
  - `curves.resample(points, step) -> (pts, arc)`;
  - `curves.tangents(points)` y `curves.normals(points)`;
  - `curves.knot_noise(rng, arc, spacing, lo, hi, mode=None) -> ndarray`;
  - `curves.longest_straight(points, window_m=25.0, tol_deg=2.0) -> float`.

- [ ] **Paso 1: escribir los tests que fallan** (añadir a `test_terrain_path.py`)

```python
from terrain_path.curves import knot_noise, longest_straight, resample  # noqa: E402
from terrain_path.layout import GRID, MAP_MAX_M, MAP_MIN_M  # noqa: E402
from terrain_path.style import C01_STYLE  # noqa: E402


def test_el_mapa_mide_400_metros():
    assert GRID == 4 and MAP_MAX_M - MAP_MIN_M == 400.0


def test_remuestreo_a_paso_fijo():
    pts, arc = resample(np.array([[0.0, 0.0], [10.0, 0.0], [10.0, 5.0]]), 1.0)
    assert np.isclose(arc[-1], 15.0) and np.allclose(np.diff(arc), 1.0)


def test_detecta_rectas_largas():
    s = np.arange(0.0, 60.0, 1.0)
    recta = np.stack([s, np.zeros_like(s)], axis=1)
    curva = np.stack([30.0 * np.cos(s / 30.0), 30.0 * np.sin(s / 30.0)], axis=1)
    assert longest_straight(recta) >= 50.0
    assert longest_straight(curva) == 0.0


def test_ruido_por_nudos_en_rango():
    rng = np.random.default_rng(1)
    arc = np.arange(0.0, 300.0, 1.0)
    v = knot_noise(rng, arc, (20.0, 60.0), 2.5, 8.0, mode=4.0)
    assert v.min() >= 2.5 - 1e-9 and v.max() <= 8.0 + 1e-9 and np.ptp(v) > 1.0


def test_estilo_c01():
    assert C01_STYLE.loops == 7 and C01_STYLE.nested_loops >= 1 and C01_STYLE.crossings >= 1
```

- [ ] **Paso 2: comprobar que fallan**

Ejecutar: `uv run pytest -q Scripts/tests/test_terrain_path.py`
Esperado: ERROR `ModuleNotFoundError: No module named 'terrain_path'`.

- [ ] **Paso 3: implementar**

`Scripts/terrain_path/layout.py`:

```python
"""Rejilla del mapa "camino primero": 4 x 4 trozos de 100 m (400 m de lado).

Mismas convenciones que terrain_vol/layout.py (X = Norte, Y = Este; el trozo (col, fila) tiene
su centro en (fila * CELL_M, col * CELL_M)); solo cambia el numero de trozos."""

from __future__ import annotations

from terrain_vol.layout import (CELL_M, CELL_SAMPLES, STEP_XY_M, STEP_Z_M, UU_PER_M, WATER_M,  # noqa: F401
                                Z_MAX_M, Z_MIN_M)

GRID = 4
MAP_MIN_M = -CELL_M / 2.0
MAP_MAX_M = GRID * CELL_M - CELL_M / 2.0
GRID_PAD = 1
```

`Scripts/terrain_path/style.py`:

```python
"""Parametros del mapa "camino primero" (Docs/Diseno_Terreno_CaminoPrimero.md, seccion 5)."""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class PathStyle:
    name: str = "C01_camino"
    description: str = ("Camino primero: un camino, lazos que vuelven, puentes naturales y tuneles, "
                        "rio con islas y final abierto al mar.")
    # Grafo
    main_length_m: tuple[float, float] = (700.0, 900.0)
    path_separation_m: float = 34.0          # distancia minima entre dos caminos que no se unen
    loops: int = 7
    nested_loops: int = 2                    # cuantos de ellos cuelgan de otro lazo
    crossings: int = 2                       # lazos que cruzan a su padre (puente o tunel)
    loop_span_m: tuple[float, float] = (40.0, 160.0)
    loop_reach_m: tuple[float, float] = (26.0, 60.0)
    backtrack_chance: float = 0.3            # el lazo sale hacia atras antes de avanzar
    hill_tunnels: int = 2                    # tramos del principal que atraviesan un cerro
    # Anchura (semiancho, m) y cota
    width_m: tuple[float, float, float] = (2.5, 4.0, 8.0)       # minimo, moda, maximo
    river_half_width_m: tuple[float, float] = (4.0, 8.0)
    max_grade: float = 0.2
    steep_grade: float = 0.3
    steep_chance: float = 0.15
    cross_clearance_m: float = 8.5
    # Biomas a lo largo del principal: acantilado, agua, dunas, playa
    biome_shares: tuple[float, float, float, float] = (0.35, 0.2, 0.3, 0.15)
    rim_cliffs_m: tuple[float, float] = (4.0, 9.0)
    rim_water_m: tuple[float, float] = (3.0, 5.0)
    rim_dunes_m: tuple[float, float] = (3.0, 6.0)
    rim_beach_m: tuple[float, float] = (0.0, 1.5)
    wall_angle_deg: tuple[float, float, float, float] = (78.0, 55.0, 62.0, 30.0)
    block_band_m: float = 8.0
    # Rio
    island_per_100m: float = 5.0
    # Fondo de vistas
    vista_dune_wave_m: tuple[float, float] = (15.0, 20.0)
    vista_dune_amp_m: tuple[float, float] = (0.8, 2.6)
    vista_pond_m: float = 2.6
    # Castillos de arena
    castles: int = 3


C01_STYLE = PathStyle()
```

`Scripts/terrain_path/curves.py`:

```python
"""Utilidades de polilineas 2D (en metros)."""

from __future__ import annotations

import numpy as np
from scipy import ndimage


def resample(points: np.ndarray, step: float) -> tuple[np.ndarray, np.ndarray]:
    """Puntos cada 'step' metros de arco (el ultimo, exacto) y su arco."""
    seg = np.linalg.norm(np.diff(points, axis=0), axis=1)
    arc = np.concatenate(([0.0], np.cumsum(seg)))
    s = np.arange(0.0, arc[-1], step)
    if arc[-1] - s[-1] > 1e-6:
        s = np.append(s, arc[-1])
    out = np.stack([np.interp(s, arc, points[:, 0]), np.interp(s, arc, points[:, 1])], axis=1)
    return out, s


def tangents(points: np.ndarray) -> np.ndarray:
    t = np.gradient(points, axis=0)
    return t / np.maximum(np.linalg.norm(t, axis=1, keepdims=True), 1e-9)


def normals(points: np.ndarray) -> np.ndarray:
    t = tangents(points)
    return np.stack([-t[:, 1], t[:, 0]], axis=1)


def knot_noise(rng: np.random.Generator, arc: np.ndarray, spacing: tuple[float, float], lo: float, hi: float,
               mode: float | None = None) -> np.ndarray:
    """Valor suave a lo largo del arco: nudos cada 'spacing' m con valores en [lo, hi] (triangular
    con moda 'mode' si se da) interpolados y suavizados. Siempre dentro de [lo, hi]."""
    knots = [0.0]
    while knots[-1] < arc[-1]:
        knots.append(knots[-1] + float(rng.uniform(*spacing)))
    if mode is None:
        values = rng.uniform(lo, hi, len(knots))
    else:
        values = rng.triangular(lo, mode, hi, len(knots))
    v = np.interp(arc, knots, values)
    step = max(float(np.median(np.diff(arc))) if len(arc) > 1 else 1.0, 1e-6)
    return np.clip(ndimage.gaussian_filter1d(v, 4.0 / step, mode="nearest"), lo, hi)


def longest_straight(points: np.ndarray, window_m: float = 25.0, tol_deg: float = 2.0) -> float:
    """Longitud (m) del tramo recto mas largo: tramos de window_m cuyo rumbo cambia menos de
    tol_deg. 0 si ninguna ventana de window_m es recta. Espera puntos cada ~1 m."""
    pts, arc = resample(points, 1.0)
    t = tangents(pts)
    heading = np.unwrap(np.arctan2(t[:, 1], t[:, 0]))
    w = int(window_m)
    if len(heading) <= w:
        return 0.0
    windows = np.lib.stride_tricks.sliding_window_view(heading, w + 1)
    straight = np.ptp(windows, axis=1) < np.radians(tol_deg)
    best = run = 0
    for flag in straight:
        run = run + 1 if flag else 0
        best = max(best, run)
    return float(best - 1 + w) if best else 0.0
```

- [ ] **Paso 4: comprobar que pasa**

Ejecutar: `uv run pytest -q Scripts/tests/test_terrain_path.py`
Esperado: PASS (6 tests).

- [ ] **Paso 5: commit**

```bash
git add Scripts/terrain_path/__init__.py Scripts/terrain_path/layout.py Scripts/terrain_path/style.py Scripts/terrain_path/curves.py Scripts/tests/test_terrain_path.py
git commit -m "feat(tools): rejilla 4x4, estilo y utilidades de curvas del terreno camino primero"
```

---

### Tarea 3: camino principal

**Ficheros:**
- Crear: `Scripts/terrain_path/graph.py`.
- Test: `Scripts/tests/test_terrain_path.py`.

**Interfaces:**
- Consume: `curves.*`, `layout.*` y `PathStyle`.
- Produce:
  - `PathLine(id, parent, points, arc, s_out=0.0, s_back=0.0)` con `point_at(s)`, `tangent_at(s)`, `normal_at(s)` y `length`;
  - `Crossing(upper, lower, point, s_upper, s_lower)`;
  - `PathGraph(lines, crossings)` con `main` y `loops()`;
  - `steer_walk(rng, start, heading, waypoints, radii, max_len, noise_deg, wave_m) -> ndarray | None`;
  - `trace_main(rng, style) -> PathLine`;
  - constantes `STEP_M = 1.0` y `EDGE_MARGIN_M = 25.0`.

- [ ] **Paso 1: escribir los tests que fallan**

```python
from terrain_path.graph import EDGE_MARGIN_M, trace_main  # noqa: E402


@pytest.fixture(scope="module")
def main_line():
    return trace_main(np.random.default_rng(60001), C01_STYLE)


def test_el_principal_va_del_sur_al_mar(main_line):
    lo, hi = C01_STYLE.main_length_m
    assert lo <= main_line.length <= hi
    assert main_line.points[0][0] < MAP_MIN_M + 40.0 and main_line.points[-1][0] > MAP_MAX_M - 40.0


def test_el_principal_no_tiene_rectas_ni_giros_bruscos(main_line):
    assert longest_straight(main_line.points) <= 25.0
    t = np.gradient(main_line.points, axis=0)
    h = np.unwrap(np.arctan2(t[:, 1], t[:, 0]))
    assert np.max(np.abs(h[10:] - h[:-10])) <= np.radians(55.0)


def test_el_principal_no_se_acerca_a_si_mismo_ni_al_borde(main_line):
    from scipy.spatial import cKDTree
    pts, arc = main_line.points, main_line.arc
    for i, j in cKDTree(pts).query_pairs(C01_STYLE.path_separation_m):
        assert abs(arc[i] - arc[j]) <= 70.0
    inner = pts[:-5]
    assert inner.min() >= MAP_MIN_M + EDGE_MARGIN_M - 1.0 and inner.max() <= MAP_MAX_M - EDGE_MARGIN_M + 1.0
```

- [ ] **Paso 2: comprobar que fallan**

Ejecutar: `uv run pytest -q Scripts/tests/test_terrain_path.py -k principal`
Esperado: ERROR `ModuleNotFoundError: No module named 'terrain_path.graph'`.

- [ ] **Paso 3: implementar** `Scripts/terrain_path/graph.py`, primera parte:

```python
"""Grafo del camino: principal de inicio a fin, lazos que salen y vuelven a su padre (tambien
de otro lazo) y cruces a distinto nivel. Todo en 2D (metros); la cota va en profile.py."""

from __future__ import annotations

import math
from dataclasses import dataclass, field

import numpy as np
from scipy.spatial import cKDTree

from .curves import longest_straight, resample
from .layout import MAP_MAX_M, MAP_MIN_M
from .style import PathStyle

STEP_M = 1.0
MAX_TURN_DEG_PER_M = 5.0          # 50 grados en 10 m como mucho
EDGE_MARGIN_M = 25.0
SELF_GAP_M = 70.0                 # dos puntos del mismo camino a menos de esto en arco pueden estar cerca


@dataclass
class PathLine:
    id: int
    parent: int | None            # None = camino principal
    points: np.ndarray            # (N, 2), cada STEP_M
    arc: np.ndarray
    s_out: float = 0.0            # arco en el padre donde sale
    s_back: float = 0.0           # arco en el padre donde vuelve

    @property
    def length(self) -> float:
        return float(self.arc[-1])

    def point_at(self, s: float) -> np.ndarray:
        return np.array([np.interp(s, self.arc, self.points[:, 0]), np.interp(s, self.arc, self.points[:, 1])])

    def tangent_at(self, s: float) -> np.ndarray:
        d = self.point_at(min(s + 2.0, self.length)) - self.point_at(max(s - 2.0, 0.0))
        return d / max(float(np.linalg.norm(d)), 1e-9)

    def normal_at(self, s: float) -> np.ndarray:
        t = self.tangent_at(s)
        return np.array([-t[1], t[0]])


@dataclass
class Crossing:
    upper: int                    # camino que pasa por arriba (puente natural)
    lower: int                    # camino que pasa por debajo (tunel)
    point: np.ndarray
    s_upper: float
    s_lower: float


@dataclass
class PathGraph:
    lines: list[PathLine]
    crossings: list[Crossing] = field(default_factory=list)

    @property
    def main(self) -> PathLine:
        return self.lines[0]

    def loops(self) -> list[PathLine]:
        return self.lines[1:]


def _inside(p: np.ndarray, margin: float) -> bool:
    return bool(MAP_MIN_M + margin <= p[0] <= MAP_MAX_M - margin and MAP_MIN_M + margin <= p[1] <= MAP_MAX_M - margin)


def steer_walk(rng: np.random.Generator, start, heading: float, waypoints: list[np.ndarray], radii: list[float],
               max_len: float, noise_deg: float, wave_m: tuple[float, float]) -> np.ndarray | None:
    """Avanza de 'start' hacia cada waypoint (llega al entrar en su radio) con giro acotado y un
    rumbo que serpentea (dos ondas de longitud distinta; se apaga a menos de 20 m del objetivo).
    Al llegar al ultimo se anade el punto exacto. None si sale del mapa o supera max_len."""
    p = np.asarray(start, dtype=float)
    pts = [p.copy()]
    h = float(heading)
    w1 = float(rng.uniform(*wave_m))
    w2 = w1 * float(rng.uniform(0.35, 0.5))
    ph = rng.uniform(0.0, 2.0 * math.pi, 2)
    limit = math.radians(MAX_TURN_DEG_PER_M) * STEP_M
    s, k = 0.0, 0
    while s < max_len:
        to = waypoints[k] - p
        dist = float(np.hypot(*to))
        if dist < radii[k]:
            k += 1
            if k == len(waypoints):
                pts.append(np.asarray(waypoints[-1], dtype=float))
                return np.array(pts)
            continue
        fade = min(1.0, dist / 20.0)
        wobble = math.radians(noise_deg) * (0.7 * math.sin(2 * math.pi * s / w1 + ph[0])
                                            + 0.3 * math.sin(2 * math.pi * s / w2 + ph[1])) * fade
        want = math.atan2(to[1], to[0]) + wobble
        dh = (want - h + math.pi) % (2.0 * math.pi) - math.pi
        h += max(-limit, min(limit, dh))
        p = p + STEP_M * np.array([math.cos(h), math.sin(h)])
        if not _inside(p, EDGE_MARGIN_M - 10.0):
            return None
        pts.append(p.copy())
        s += STEP_M
    return None


def self_separated(points: np.ndarray, arc: np.ndarray, sep: float) -> bool:
    """Ningun par de puntos lejanos en arco (> SELF_GAP_M) queda a menos de 'sep'."""
    pairs = cKDTree(points).query_pairs(sep, output_type="ndarray")
    return not len(pairs) or bool(np.all(np.abs(arc[pairs[:, 0]] - arc[pairs[:, 1]]) <= SELF_GAP_M))


def trace_main(rng: np.random.Generator, style: PathStyle) -> PathLine:
    """Principal: del borde sur (X minima) al norte (el mar), zigzagueando entre bandas este y
    oeste por 4-5 puntos de paso, con longitud en style.main_length_m."""
    for _ in range(400):
        start = np.array([MAP_MIN_M + 30.0, float(rng.uniform(MAP_MIN_M + 110.0, MAP_MAX_M - 110.0))])
        side = float(rng.choice([-1.0, 1.0]))
        n_way = int(rng.integers(4, 6))
        ways = []
        for k, x in enumerate(np.linspace(MAP_MIN_M + 95.0, MAP_MAX_M - 95.0, n_way)):
            band = float(rng.uniform(90.0, 150.0))
            y = MAP_MIN_M + band if side * (-1.0) ** k < 0 else MAP_MAX_M - band
            ways.append(np.array([x + float(rng.uniform(-15.0, 15.0)), y]))
        end = np.array([MAP_MAX_M - 35.0, float(rng.uniform(MAP_MIN_M + 120.0, MAP_MAX_M - 120.0))])
        raw = steer_walk(rng, start, 0.0, ways + [end], [25.0] * n_way + [3.0],
                         style.main_length_m[1] + 50.0, 55.0, (55.0, 85.0))
        if raw is None:
            continue
        pts, arc = resample(raw, STEP_M)
        if not (style.main_length_m[0] <= arc[-1] <= style.main_length_m[1]):
            continue
        if longest_straight(pts) > 25.0 or not self_separated(pts, arc, style.path_separation_m):
            continue
        return PathLine(0, None, pts, arc)
    raise RuntimeError("no se encontro un camino principal valido en 400 intentos")
```

- [ ] **Paso 4: comprobar que pasa**

Ejecutar: `uv run pytest -q Scripts/tests/test_terrain_path.py -k principal`
Esperado: PASS (3 tests). Si `trace_main` agota los intentos con la semilla 60001, el ajuste permitido es subir `noise_deg` a 65 o ensanchar `wave_m`; nunca relajar los tests.

- [ ] **Paso 5: commit**

```bash
git add Scripts/terrain_path/graph.py Scripts/tests/test_terrain_path.py
git commit -m "feat(tools): camino principal del terreno camino primero (giro acotado, sin rectas)"
```

---

### Tarea 4: lazos, lazos anidados y cruces

**Ficheros:**
- Modificar: `Scripts/terrain_path/graph.py`.
- Test: `Scripts/tests/test_terrain_path.py`.

**Interfaces:**
- Produce:
  - `make_loop(rng, graph, parent_id, style, cross, line_id) -> tuple[PathLine, Crossing | None] | None`;
  - `build_graph(rng, style) -> PathGraph`.
- Un `Crossing` recién creado lleva provisionalmente `upper = lazo` y `lower = padre`. Quién va arriba lo decide la tarea 5.
- Garantías:
  - todo lazo empieza en `parent.point_at(s_out)` y acaba exactamente en `parent.point_at(s_back)`;
  - `s_out < s_back`;
  - un lazo de cruce toca a su padre una sola vez, con un ángulo de 40° o más y a 75 m o más de arco de cada extremo del lazo.

- [ ] **Paso 1: escribir los tests que fallan**

```python
from terrain_path.graph import build_graph  # noqa: E402


@pytest.fixture(scope="module")
def graph():
    return build_graph(np.random.default_rng(60001), C01_STYLE)


def test_hay_lazos_anidados_y_cruces(graph):
    loops = graph.loops()
    assert len(loops) >= C01_STYLE.loops - 1
    assert any(l.parent not in (None, 0) for l in loops), "ningun lazo cuelga de otro lazo"
    assert len(graph.crossings) >= 1


def test_cada_lazo_sale_y_vuelve_a_su_padre(graph):
    for loop in graph.loops():
        parent = graph.lines[loop.parent]
        assert np.hypot(*(loop.points[0] - parent.point_at(loop.s_out))) < 1.5
        assert np.hypot(*(loop.points[-1] - parent.point_at(loop.s_back))) < 1e-6
        assert loop.s_out < loop.s_back


def test_los_lazos_no_se_tocan_salvo_en_sus_uniones_y_cruces(graph):
    from scipy.spatial import cKDTree
    crossing_pts = [c.point for c in graph.crossings]
    for a in graph.lines:
        for b in graph.lines:
            if a.id >= b.id:
                continue
            d, _ = cKDTree(b.points).query(a.points)
            close = a.points[d < 6.0]
            for p in close:
                near_join = any(np.hypot(*(p - q)) < 20.0 for line in (a, b) if line.parent is not None
                                for q in (line.points[0], line.points[-1]))
                near_cross = any(np.hypot(*(p - c)) < 20.0 for c in crossing_pts)
                assert near_join or near_cross, f"caminos {a.id} y {b.id} se tocan en {p}"


def test_los_cruces_son_francos_y_lejos_de_los_extremos(graph):
    for c in graph.crossings:
        up, lo = graph.lines[c.upper], graph.lines[c.lower]
        cos = abs(float(np.dot(up.tangent_at(c.s_upper), lo.tangent_at(c.s_lower))))
        assert cos <= np.cos(np.radians(40.0))
        loop = up if up.parent is not None and up.parent == lo.id else lo
        s = c.s_upper if loop is up else c.s_lower
        assert s >= 75.0 and loop.length - s >= 75.0
```

- [ ] **Paso 2: comprobar que fallan**

Ejecutar: `uv run pytest -q Scripts/tests/test_terrain_path.py -k "lazo or cruce"`
Esperado: ImportError de `build_graph`.

- [ ] **Paso 3: implementar** (añadir a `graph.py`)

```python
JOIN_ARC_M = 16.0                 # tramo junto a una union en el que el lazo aun esta pegado al padre


def _required_gap(arc_i: np.ndarray, length: float, sep: float, anchors: list[float]) -> np.ndarray:
    """Separacion exigida a cada punto del lazo: 'sep' lejos de uniones y cruces; cerca, crece
    con la distancia en arco (el lazo se va apartando del padre en V, no de golpe)."""
    gap = np.minimum(arc_i, length - arc_i)
    for a in anchors:
        gap = np.minimum(gap, np.abs(arc_i - a))
    return np.minimum(sep, np.maximum(3.0, 0.7 * gap))


def _clear_of_others(line: PathLine, graph: PathGraph, style: PathStyle, cross_s: float | None) -> bool:
    anchors = [cross_s] if cross_s is not None else []
    need = _required_gap(line.arc, line.length, style.path_separation_m, anchors)
    for other in graph.lines:
        d, _ = cKDTree(other.points).query(line.points)
        if np.any(d < need):
            return False
    return True


def _find_crossing(line: PathLine, parent: PathLine) -> Crossing | None:
    d, k = cKDTree(parent.points).query(line.points)
    inner = (line.arc > JOIN_ARC_M) & (line.arc < line.length - JOIN_ARC_M)
    hits = np.nonzero((d < 1.5) & inner)[0]
    if not len(hits) or np.ptp(line.arc[hits]) > 6.0:          # ninguno, o mas de un cruce
        return None
    i = int(hits[np.argmin(d[hits])])
    j = int(k[i])
    tl = line.tangent_at(line.arc[i])
    tp = parent.tangent_at(parent.arc[j])
    if abs(float(np.dot(tl, tp))) > math.cos(math.radians(40.0)):
        return None
    if line.arc[i] < 75.0 or line.length - line.arc[i] < 75.0:
        return None
    return Crossing(line.id, parent.id, line.points[i].copy(), float(line.arc[i]), float(parent.arc[j]))


def make_loop(rng: np.random.Generator, graph: PathGraph, parent_id: int, style: PathStyle, cross: bool,
              line_id: int) -> tuple[PathLine, Crossing | None] | None:
    """Un lazo de 'parent_id': sale en s_out hacia un lado, da la vuelta (a veces retrocede antes)
    y vuelve en s_back. Si 'cross', pasa al otro lado del padre a mitad de camino (cruce)."""
    parent = graph.lines[parent_id]
    nested = parent.parent is not None
    lo, hi = style.loop_span_m if not nested else (30.0, min(70.0, parent.length - 30.0))
    if cross:
        lo = max(lo, 90.0)
    if hi <= lo:
        return None
    margin_lo, margin_hi = (40.0, 70.0) if parent_id == 0 else (12.0, 12.0)
    span = float(rng.uniform(lo, hi))
    if parent.length - margin_lo - margin_hi - span <= 0.0:
        return None
    s_out = float(rng.uniform(margin_lo, parent.length - margin_hi - span))
    s_back = s_out + span
    side = float(rng.choice([-1.0, 1.0]))
    reach = float(rng.uniform(*style.loop_reach_m)) * (0.6 if nested else 1.0)
    ways: list[np.ndarray] = []
    radii: list[float] = []
    if not cross and rng.random() < style.backtrack_chance:
        s_b = max(0.0, s_out - float(rng.uniform(10.0, 35.0)))
        ways.append(parent.point_at(s_b) + parent.normal_at(s_b) * side * reach)
        radii.append(12.0)
    if cross:
        s_m1 = s_out + span * float(rng.uniform(0.2, 0.3))
        s_x = s_out + span * 0.5
        s_m2 = s_out + span * float(rng.uniform(0.7, 0.8))
        x_pt, n_x = parent.point_at(s_x), parent.normal_at(s_x)
        ways += [parent.point_at(s_m1) + parent.normal_at(s_m1) * side * reach,
                 x_pt + n_x * side * 14.0, x_pt - n_x * side * 14.0,
                 parent.point_at(s_m2) - parent.normal_at(s_m2) * side * reach]
        radii += [12.0, 3.0, 3.0, 12.0]
        side_back = -side
    else:
        s_m = s_out + span * 0.5
        ways.append(parent.point_at(s_m) + parent.normal_at(s_m) * side * reach)
        radii.append(12.0)
        side_back = side
    b_pt = parent.point_at(s_back)
    ways += [b_pt + parent.normal_at(s_back) * side_back * 12.0 - parent.tangent_at(s_back) * 4.0, b_pt]
    radii += [4.0, 2.5]
    t_a, n_a = parent.tangent_at(s_out), parent.normal_at(s_out)
    lean = t_a * math.cos(math.radians(45.0)) + n_a * side * math.sin(math.radians(45.0))
    raw = steer_walk(rng, parent.point_at(s_out), math.atan2(lean[1], lean[0]), ways, radii,
                     span * 4.0 + 200.0, 40.0, (35.0, 60.0))
    if raw is None:
        return None
    pts, arc = resample(raw, STEP_M)
    line = PathLine(line_id, parent_id, pts, arc, s_out, s_back)
    if longest_straight(pts) > 25.0 or not self_separated(pts, arc, style.path_separation_m):
        return None
    crossing = _find_crossing(line, parent) if cross else None
    if cross and crossing is None:
        return None
    if not _clear_of_others(line, graph, style, crossing.s_upper if crossing else None):
        return None
    return line, crossing


def build_graph(rng: np.random.Generator, style: PathStyle) -> PathGraph:
    """Principal y style.loops lazos: los ultimos style.nested_loops cuelgan de un lazo largo
    (>= 110 m); los primeros style.crossings del principal lo cruzan. Un lazo que no encaja en
    120 intentos se omite (y un cruce que no encaja en 80 se intenta como lazo normal)."""
    graph = PathGraph([trace_main(rng, style)])
    for k in range(style.loops):
        nested = k >= style.loops - style.nested_loops
        cross = (not nested) and k < style.crossings
        for attempt in range(120):
            if nested:
                parents = [line.id for line in graph.lines[1:] if line.length >= 110.0]
                if not parents:
                    break
                parent_id = int(rng.choice(parents))
            else:
                parent_id = 0
            result = make_loop(rng, graph, parent_id, style, cross and attempt < 80, len(graph.lines))
            if result is not None:
                line, crossing = result
                graph.lines.append(line)
                if crossing is not None:
                    graph.crossings.append(crossing)
                break
    return graph
```

- [ ] **Paso 4: comprobar que pasa**

Ejecutar: `uv run pytest -q Scripts/tests/test_terrain_path.py -k "lazo or cruce or principal"`
Esperado: PASS (7 tests).

- [ ] **Paso 5: commit**

```bash
git add Scripts/terrain_path/graph.py Scripts/tests/test_terrain_path.py
git commit -m "feat(tools): lazos, lazos anidados y cruces en el grafo del camino"
```

---

### Tarea 5: cota, anchura, bioma y túneles de cada camino

**Ficheros:**
- Crear: `Scripts/terrain_path/profile.py`.
- Test: `Scripts/tests/test_terrain_path.py`.

**Interfaces:**
- Consume: `PathGraph`, `PathLine`, `Crossing`, `knot_noise` y `PathStyle`.
- Produce:
  - `BIOMES = ("cliffs", "water", "dunes", "beach")`, con los códigos 0 a 3;
  - `LineProfile(z, half_width, biome, tunnel)`: arrays por punto de la línea, con `tunnel` de tipo bool;
  - `PathPlan(graph, profiles: dict[int, LineProfile], crossings: list[Crossing], hill_tunnels: list[tuple[int, float, float]])`;
  - `build_plan(rng, style) -> PathPlan`, que reintenta el grafo si un cruce no cabe en pendiente;
  - `EXCLUDE_EXTRA_M = 12.0` (exclusión del camino de abajo en un cruce: semiancho del de arriba + 12 m).

- [ ] **Paso 1: escribir los tests que fallan**

```python
from terrain_path.layout import WATER_M  # noqa: E402
from terrain_path.profile import build_plan  # noqa: E402


@pytest.fixture(scope="module")
def plan():
    return build_plan(np.random.default_rng(60001), C01_STYLE)


def test_pendientes_suaves(plan):
    for line in plan.graph.lines:
        z = plan.profiles[line.id].z
        grade = np.abs(np.diff(z)) / np.maximum(np.diff(line.arc), 1e-9)
        assert grade.max() <= C01_STYLE.steep_grade + 0.02, f"camino {line.id}: pendiente {grade.max():.2f}"


def test_los_lazos_empalman_a_la_cota_de_su_padre(plan):
    for loop in plan.graph.loops():
        parent, pp = plan.graph.lines[loop.parent], plan.profiles[loop.parent]
        z = plan.profiles[loop.id].z
        assert abs(z[0] - np.interp(loop.s_out, parent.arc, pp.z)) < 0.05
        assert abs(z[-1] - np.interp(loop.s_back, parent.arc, pp.z)) < 0.05


def test_los_cruces_dejan_hueco_de_tunel(plan):
    for c in plan.crossings:
        up, lo = plan.graph.lines[c.upper], plan.graph.lines[c.lower]
        z_up = np.interp(c.s_upper, up.arc, plan.profiles[up.id].z)
        z_lo = np.interp(c.s_lower, lo.arc, plan.profiles[lo.id].z)
        assert z_up - z_lo >= C01_STYLE.cross_clearance_m - 0.1
        assert z_lo >= WATER_M + 0.8
        tunnel = plan.profiles[lo.id].tunnel
        assert tunnel[int(np.searchsorted(lo.arc, c.s_lower))]


def test_biomas_en_orden_y_final_en_la_playa(plan):
    b = plan.profiles[0].biome
    assert list(np.unique(b)) == [0, 1, 2, 3] and np.all(np.diff(b) >= 0)
    assert plan.profiles[0].z[-1] <= WATER_M + 0.6


def test_hay_tuneles_de_cerro_en_el_acantilado(plan):
    assert len(plan.hill_tunnels) >= 1
    for line_id, s0, s1 in plan.hill_tunnels:
        assert line_id == 0 and 25.0 <= s1 - s0 <= 45.0
```

- [ ] **Paso 2: comprobar que fallan**

Ejecutar: `uv run pytest -q Scripts/tests/test_terrain_path.py -k "pendiente or empalman or hueco or biomas or cerro"`
Esperado: ImportError de `build_plan`.

- [ ] **Paso 3: implementar** `Scripts/terrain_path/profile.py`

```python
"""Cota, semiancho, bioma y tramos de tunel de cada camino del grafo.

Cota: nudos cada 15-60 m con valores del rango de su bioma, unidos en linea recta y recortados
a la pendiente maxima (rampas lineales), con las rodillas suavizadas. Los lazos empalman con la
cota de su padre en sus dos uniones; en un cruce, el camino de arriba queda cross_clearance_m
por encima del de abajo (que pasa en tunel)."""

from __future__ import annotations

from dataclasses import dataclass, replace

import numpy as np
from scipy import ndimage
from scipy.spatial import cKDTree

from .curves import knot_noise
from .graph import Crossing, PathGraph, PathLine, build_graph
from .layout import WATER_M
from .style import PathStyle

BIOMES = ("cliffs", "water", "dunes", "beach")
BIOME_LEVEL_M = {0: (1.0, 8.0), 1: (WATER_M + 0.7, WATER_M + 0.7), 2: (0.0, 4.0), 3: (0.5, 1.5)}
EXCLUDE_EXTRA_M = 12.0


class Infeasible(Exception):
    """Un cruce no cabe con la pendiente maxima: hay que rehacer el grafo."""


@dataclass
class LineProfile:
    z: np.ndarray
    half_width: np.ndarray
    biome: np.ndarray
    tunnel: np.ndarray


@dataclass
class PathPlan:
    graph: PathGraph
    profiles: dict[int, LineProfile]
    crossings: list[Crossing]
    hill_tunnels: list[tuple[int, float, float]]


def _smooth01(e0: float, e1: float, x):
    t = np.clip((np.asarray(x, dtype=float) - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def _knot_levels(rng: np.random.Generator, arc: np.ndarray, biome: np.ndarray) -> np.ndarray:
    knots, values = [0.0], []
    while knots[-1] < arc[-1]:
        knots.append(knots[-1] + float(rng.uniform(15.0, 60.0)))
    for s in knots:
        lo, hi = BIOME_LEVEL_M[int(biome[min(int(np.searchsorted(arc, s)), len(arc) - 1)])]
        values.append(float(rng.uniform(lo, hi)))
    return np.interp(arc, knots, values)


def _grades(rng: np.random.Generator, arc: np.ndarray, style: PathStyle) -> np.ndarray:
    grade = np.full(len(arc), style.max_grade)
    s = 0.0
    while s < arc[-1]:
        length = float(rng.uniform(15.0, 60.0))
        if rng.random() < style.steep_chance:
            grade[(arc >= s) & (arc < s + min(length, 20.0))] = style.steep_grade
        s += length
    return grade


def clamp_grade(z: np.ndarray, arc: np.ndarray, grade: np.ndarray, anchors: set[int] = frozenset()) -> np.ndarray:
    """Recorta z para que ninguna pareja de vecinos supere su pendiente; los anclajes no se mueven."""
    z = z.copy()
    ds = np.diff(arc)
    for _ in range(4):
        for i in range(1, len(z)):
            if i not in anchors:
                z[i] = min(max(z[i], z[i - 1] - grade[i] * ds[i - 1]), z[i - 1] + grade[i] * ds[i - 1])
        for i in range(len(z) - 2, -1, -1):
            if i not in anchors:
                z[i] = min(max(z[i], z[i + 1] - grade[i] * ds[i]), z[i + 1] + grade[i] * ds[i])
    return z


def _knees(z: np.ndarray, anchors: set[int]) -> np.ndarray:
    out = ndimage.gaussian_filter1d(z, 2.0, mode="nearest")
    for i in anchors:
        out[i] = z[i]
    return out


def _main_biomes(arc: np.ndarray, shares) -> np.ndarray:
    edges = np.cumsum(shares)[:-1]
    return np.searchsorted(edges, arc / arc[-1], side="right").astype(int)


def _widths(rng: np.random.Generator, arc: np.ndarray, biome: np.ndarray, style: PathStyle) -> np.ndarray:
    lo, mode, hi = style.width_m
    w = knot_noise(rng, arc, (20.0, 60.0), lo, hi, mode=mode)
    river = knot_noise(rng, arc, (20.0, 60.0), *style.river_half_width_m)
    water = ndimage.gaussian_filter1d((biome == 1).astype(float), 5.0, mode="nearest")
    return w * (1.0 - water) + river * water


def main_profile(rng: np.random.Generator, line: PathLine, style: PathStyle) -> LineProfile:
    biome = _main_biomes(line.arc, style.biome_shares)
    target = _knot_levels(rng, line.arc, biome)
    end = line.length
    beach = _smooth01(end - 60.0, end, line.arc)
    target = target * (1.0 - beach) + (WATER_M + 0.3) * beach
    z = _knees(clamp_grade(target, line.arc, _grades(rng, line.arc, style)), set())
    w = _widths(rng, line.arc, biome, style)
    w = np.maximum(w, 7.0 * (1.0 - _smooth01(8.0, 16.0, line.arc)))          # salida: ensanche
    w = w + 38.0 * _smooth01(end - 55.0, end, line.arc)                       # final: abanico a la playa
    return LineProfile(z, w, biome, np.zeros(len(z), dtype=bool))


def loop_profile(rng: np.random.Generator, line: PathLine, graph: PathGraph, profiles: dict[int, LineProfile],
                 style: PathStyle, pin: tuple[float, float] | None) -> LineProfile:
    """Lazo: bioma del principal mas cercano, cota libre empalmada con el padre en s_out y s_back.
    pin = (arco en el lazo, cota) fija un punto (el del cruce)."""
    main = graph.main
    _, k = cKDTree(main.points).query(line.points)
    biome = profiles[0].biome[k]
    parent, pp = graph.lines[line.parent], profiles[line.parent]
    z_a, z_b = float(np.interp(line.s_out, parent.arc, pp.z)), float(np.interp(line.s_back, parent.arc, pp.z))
    z = _knot_levels(rng, line.arc, biome)
    t = line.arc / line.length
    z = z + (z_a - z[0]) * (1.0 - t) + (z_b - z[-1]) * t
    anchors = {0, len(z) - 1}
    z[0], z[-1] = z_a, z_b
    if pin is not None:
        kx = int(np.searchsorted(line.arc, pin[0]))
        flat = np.abs(line.arc - pin[0]) <= 8.0
        before, after = line.arc[kx] - 8.0, line.length - line.arc[kx] - 8.0
        if abs(pin[1] - z_a) > style.max_grade * before or abs(pin[1] - z_b) > style.max_grade * after:
            raise Infeasible(f"cruce del lazo {line.id} no cabe en pendiente")
        z = np.where(flat, pin[1], np.interp(line.arc, [0.0, line.arc[kx] - 8.0, line.arc[kx] + 8.0, line.length],
                                             [z_a, pin[1], pin[1], z_b]) + 0.5 * (z - np.interp(
                                                 line.arc, [0.0, line.length], [z[0], z[-1]])))
        anchors |= set(np.nonzero(flat)[0].tolist())
        z[0], z[-1] = z_a, z_b
    grade = np.full(len(z), style.max_grade)
    z = _knees(clamp_grade(z, line.arc, grade, anchors), anchors)
    return LineProfile(z, _widths(rng, line.arc, biome, style), biome, np.zeros(len(z), dtype=bool))


def _mark(profile: LineProfile, line: PathLine, s0: float, s1: float) -> LineProfile:
    tunnel = profile.tunnel | ((line.arc >= s0) & (line.arc <= s1))
    return replace(profile, tunnel=tunnel)


def _hill_tunnels(rng: np.random.Generator, graph: PathGraph, profile: LineProfile, crossings: list[Crossing],
                  style: PathStyle) -> list[tuple[int, float, float]]:
    main = graph.main
    joins = [s for loop in graph.loops() if loop.parent == 0 for s in (loop.s_out, loop.s_back)]
    joins += [c.s_upper if c.upper == 0 else c.s_lower for c in crossings if 0 in (c.upper, c.lower)]
    cliffs = main.arc[profile.biome == 0]
    out: list[tuple[int, float, float]] = []
    for _ in range(200):
        if len(out) >= style.hill_tunnels or len(cliffs) < 2:
            break
        length = float(rng.uniform(25.0, 45.0))
        s0 = float(rng.uniform(max(60.0, cliffs[0]), max(60.0, cliffs[-1] - length)))
        s1 = s0 + length
        if s1 > cliffs[-1] or any(s0 - 25.0 < j < s1 + 25.0 for j in joins):
            continue
        if any(not (s1 + 30.0 < a or s0 - 30.0 > b) for _, a, b in out):
            continue
        out.append((0, s0, s1))
    return out


def build_plan(rng: np.random.Generator, style: PathStyle) -> PathPlan:
    for _ in range(20):
        graph = build_graph(rng, style)
        try:
            return _profiles(rng, graph, style)
        except Infeasible:
            continue
    raise RuntimeError("20 grafos seguidos con cruces que no caben en pendiente")


def _profiles(rng: np.random.Generator, graph: PathGraph, style: PathStyle) -> PathPlan:
    profiles: dict[int, LineProfile] = {0: main_profile(rng, graph.main, style)}
    crossings: list[Crossing] = []
    by_loop = {c.upper: c for c in graph.crossings}
    for loop in graph.loops():
        c = by_loop.get(loop.id)
        pin = None
        if c is not None:
            parent = graph.lines[c.lower]
            z_p = float(np.interp(c.s_lower, parent.arc, profiles[parent.id].z))
            under = z_p - style.cross_clearance_m >= WATER_M + 0.8 and rng.random() < 0.5
            pin = (c.s_upper, z_p - style.cross_clearance_m if under else z_p + style.cross_clearance_m)
            c = Crossing(parent.id, loop.id, c.point, c.s_lower, c.s_upper) if under else c
        profiles[loop.id] = loop_profile(rng, loop, graph, profiles, style, pin)
        if c is not None:
            crossings.append(c)
    for c in crossings:
        lower, upper_w = graph.lines[c.lower], profiles[c.upper].half_width
        half = float(np.interp(c.s_upper, graph.lines[c.upper].arc, upper_w)) + EXCLUDE_EXTRA_M
        profiles[c.lower] = _mark(profiles[c.lower], lower, c.s_lower - half, c.s_lower + half)
    hills = _hill_tunnels(rng, graph, profiles[0], crossings, style)
    for line_id, s0, s1 in hills:
        profiles[line_id] = _mark(profiles[line_id], graph.lines[line_id], s0, s1)
    return PathPlan(graph, profiles, crossings, hills)
```

- [ ] **Paso 4: comprobar que pasa**

Ejecutar: `uv run pytest -q Scripts/tests/test_terrain_path.py`
Esperado: PASS (todos los anteriores más 5).

- [ ] **Paso 5: commit**

```bash
git add Scripts/terrain_path/profile.py Scripts/tests/test_terrain_path.py
git commit -m "feat(tools): cota, anchura, biomas, cruces y túneles de cerro del camino"
```

---

### Tarea 6: relieve a partir del camino (sección, bordes, vistas y costa) y `PathModel`

**Ficheros:**
- Crear: `Scripts/terrain_path/field.py` y `Scripts/terrain_path/model.py`.
- Test: `Scripts/tests/test_terrain_path.py`.

**Interfaces:**
- Consume: `PathPlan`, `LineProfile` y `Fields` (de `terrain_vol.density`); `smooth` y `soft_max` (de `terrain_vol.density`); `Fbm2D` y `ValueNoise3D` (de `terrain_vol.noise`).
- Produce:
  - `field.section(e, zf, z_soft, w, bw, n_rim, n_top, n_floor, style) -> (height, H, crest_e)`;
  - `field.vista(model, X, Y) -> ndarray` y `field.shore(model, X, Y, height) -> ndarray`;
  - `PathModel(seed, style)` con:
    - `.plan`;
    - `.axis`;
    - `.grid` (un `Fields`);
    - `.region`: 0 camino, 1 borde, 2 vistas, 3 costa;
    - `.route.points`: los puntos del principal;
    - `.start` y `.end`;
    - los métodos `chunk_fields`, `density`, `zones.weights`, `trail_mask`, `plaza_mask` y `samples()`.
- En la tarea 6 `density` es solo relieve 2D. El excavado 3D llega en la tarea 8.

- [ ] **Paso 1: escribir los tests que fallan**

```python
from terrain_path.model import PathModel  # noqa: E402


@pytest.fixture(scope="module")
def model():
    return PathModel(60001, C01_STYLE)


def _cross_section(model, line_id, s, offsets):
    line = model.plan.graph.lines[line_id]
    p, n = line.point_at(s), line.normal_at(s)
    pts = p[None, :] + offsets[:, None] * n[None, :]
    i = pts[:, 0] - model.axis[0]
    j = pts[:, 1] - model.axis[0]
    from scipy import ndimage
    return ndimage.map_coordinates(model.grid.height, [i, j], order=1)


def test_el_suelo_del_camino_es_llano_sin_cuenco(model):
    plan = model.plan
    checked = 0
    for line in plan.graph.lines:
        prof = plan.profiles[line.id]
        for s in np.arange(30.0, line.length - 60.0, 17.0):
            k = int(np.searchsorted(line.arc, s))
            if prof.biome[k] != 0 and prof.biome[k] != 2:
                continue                                   # agua y playa tienen su propio suelo
            if prof.tunnel[max(k - 15, 0):k + 15].any() or model.near_junction(line.point_at(s), 20.0):
                continue
            w = prof.half_width[k]
            h = _cross_section(model, line.id, s, np.linspace(-0.8 * w, 0.8 * w, 9))
            assert np.ptp(h) <= 0.3, f"camino {line.id} s={s:.0f}: cuenco o escalon ({np.ptp(h):.2f} m)"
            checked += 1
    assert checked >= 20


def test_el_borde_cierra_el_paso(model):
    """A 1,5 x semiancho del eje, fuera del camino, la pared ya esta 3 m o mas por encima del suelo."""
    plan = model.plan
    for line in plan.graph.lines:
        prof = plan.profiles[line.id]
        for s in np.arange(30.0, line.length - 60.0, 23.0):
            k = int(np.searchsorted(line.arc, s))
            if prof.biome[k] == 3 or prof.tunnel[max(k - 15, 0):k + 15].any() \
                    or model.near_junction(line.point_at(s), 25.0):
                continue
            w = prof.half_width[k]
            h = _cross_section(model, line.id, s, np.array([0.0, -(w + 6.0), w + 6.0]))
            assert min(h[1], h[2]) - h[0] >= 3.0, f"camino {line.id} s={s:.0f}: borde bajo"


def test_la_union_de_un_lazo_no_tiene_escalon(model):
    """Donde un lazo se une a su padre, el suelo pasa de uno a otro sin salto."""
    for loop in model.plan.graph.loops():
        for s in (1.0, loop.length - 1.0):
            h = _cross_section(model, loop.id, s, np.array([0.0]))[0]
            z = np.interp(s, loop.arc, model.plan.profiles[loop.id].z)
            assert abs(h - z) <= 0.4


def test_el_final_toca_el_mar(model):
    end = model.plan.graph.main.points[-1]
    i, j = int(round(end[0] - model.axis[0])), int(round(end[1] - model.axis[0]))
    patch = model.grid.height[i - 3:i + 25, j - 10:j + 11]
    assert (patch < WATER_M).any()
```

- [ ] **Paso 2: comprobar que fallan**

Ejecutar: `uv run pytest -q Scripts/tests/test_terrain_path.py -k "suelo or borde or union or final"`
Esperado: ImportError de `PathModel`.

- [ ] **Paso 3: implementar** `Scripts/terrain_path/field.py`

```python
"""Relieve a partir del camino (funciones puras sobre rejillas 2D).

Seccion "cuadrado curvado": suelo llano, esquina inferior redondeada de radio rc (min(1.5 m,
0.2 x semiancho)), pared de 'angulo' grados hasta la cresta H y remate redondeado. Nunca un
cuenco: el suelo no se curva hacia las paredes. Detras de la cresta, una banda de bloqueo y la
bajada (o subida) hasta el fondo de vistas."""

from __future__ import annotations

import math

import numpy as np

from terrain_vol.density import smooth, soft_max

from .layout import MAP_MAX_M, MAP_MIN_M, WATER_M


def soft_min(a, b, k: float):
    return -soft_max(-a, -b, k)


def rim_arrays(style) -> tuple[np.ndarray, np.ndarray]:
    rims = np.array([style.rim_cliffs_m, style.rim_water_m, style.rim_dunes_m, style.rim_beach_m])
    return rims, np.radians(np.array(style.wall_angle_deg))


def section(e, zf, z_soft, w, bw, n_rim, n_top, n_floor, style):
    """(relieve, cresta H, distancia e de la cresta). e = distancia al eje - semiancho (< 0 dentro).
    bw: pesos de bioma (..., 4). n_rim en 0..1, n_top y n_floor en -1..1."""
    rims, angles = rim_arrays(style)
    lo, hi = bw @ rims[:, 0], bw @ rims[:, 1]
    rim = lo + (hi - lo) * n_rim
    angle = bw @ angles
    tan = np.tan(angle)
    H = np.maximum(z_soft + rim + 0.8 * n_top, zf + 0.6 * rim)
    rc = np.minimum(1.5, 0.2 * w)
    t = e + rc
    fillet = rc - np.sqrt(np.maximum(rc * rc - np.clip(t, 0.0, rc) ** 2, 0.0))
    rise = np.where(t <= 0.0, 0.0, np.where(t <= rc, fillet, rc + (t - rc) * tan))
    floor = zf + 0.1 * n_floor
    height = soft_min(floor + rise, H, 1.0)
    crest_e = np.maximum(H - zf - rc, 0.0) / np.maximum(tan, 1e-3)
    return height, H, crest_e


def dune_field(model, X, Y, angle: float, wave: float, rise: float = 0.62):
    along = X * math.cos(angle) + Y * math.sin(angle)
    across = -X * math.sin(angle) + Y * math.cos(angle)
    phase = (along + 0.9 * wave * model.n_dune_warp(X, Y) + 0.25 * across * model.n_dune_mix(X, Y)) / wave
    frac = phase - np.floor(phase)
    profile = np.where(frac < rise, frac / rise, (1.0 - frac) / (1.0 - rise))
    return smooth(0.0, 1.0, profile)


def vista(model, X, Y):
    """Fondo de vistas: dunas tupidas, lagos y dunas inundadas; lomas naturales en los bordes
    sur, este y oeste (tapan el final del mapa); al norte, el mar."""
    st = model.style
    mix = smooth(0.35, 0.65, model.n_dune_mix.unit(X, Y))
    w1, w2 = st.vista_dune_wave_m
    dunes = dune_field(model, X, Y, model.dune_angle, w1) * (1 - mix) + dune_field(model, X, Y, model.dune_angle_2, w2) * mix
    a_lo, a_hi = st.vista_dune_amp_m
    amp = a_lo + (a_hi - a_lo) * smooth(0.25, 0.85, model.n_dune_amp.unit(X, Y))
    v = WATER_M - 0.4 + st.vista_pond_m * model.n_pond(X, Y) + amp * dunes + 2.0 * model.n_big.unit(X, Y)
    warp = 10.0 * model.n_edge(X, Y)
    near = np.maximum.reduce([1.0 - smooth(4.0, 30.0, d) for d in
                              (X - MAP_MIN_M + warp, Y - MAP_MIN_M + warp, MAP_MAX_M - Y + warp)])
    return v * (1.0 - near) + np.maximum(v, 7.0 + 4.0 * model.n_rim.unit(X, Y)) * near


def shore(model, X, Y, height):
    """Franja norte: playa que baja al mar; baja tambien los bordes para que el abanico final se
    funda con la orilla."""
    u = MAP_MAX_M - X + 10.0 * model.n_edge(X, Y)
    beach = WATER_M - 1.8 + 5.0 * smooth(4.0, 45.0, u)
    band = 1.0 - smooth(25.0, 55.0, u)
    return height * (1.0 - band) + np.minimum(height, beach) * band, band
```

`Scripts/terrain_path/model.py`:

```python
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
        self.extra_rng = rng                       # rio y castillos (tareas 7 y 9) siguen la secuencia
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
            cols["p"].append(pts)
            cols["n"].append(normals(pts))
            cols["z"].append(np.interp(s, line.arc, prof.z))
            cols["w"].append(np.interp(s, line.arc, prof.half_width))
            k = np.clip(np.searchsorted(line.arc, s), 0, len(line.arc) - 1)
            cols["biome"].append(prof.biome[k])
            cols["line"].append(np.full(len(s), line.id))
            cols["s"].append(s)
            cols["open"].append(~prof.tunnel[k])
        self.S = {k: np.concatenate(v) for k, v in cols.items()}
        keep = self.S["open"]
        self.open_idx = np.nonzero(keep)[0]
        self.open_tree = cKDTree(self.S["p"][keep])

    def samples(self) -> dict[str, np.ndarray]:
        return self.S

    def _plan_extras(self) -> None:
        """Rio y castillos: se anaden en las tareas 7 y 9."""
        self.river = None
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
        onehot = np.eye(4)[self.S["biome"][ik]]
        bw = ((onehot * (np.exp(-0.5 * ((dk - dk[:, :1]) / 15.0) ** 2))[..., None]).sum(axis=1))
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
        height, coast = field.shore(self, X, Y, height)
        region = np.where(e < 0.0, 0, np.where(e < crest_e + band + 14.0, 1, 2))
        region = np.where(coast > 0.5, 3, region)
        if shape == (len(getattr(self, "axis", [])),) * 2:
            self.region = region
        wall_band = (1.0 - smooth(0.0, 3.0, np.abs(e - 0.5 * crest_e))) * bw[..., 0]
        tunnel = self._tunnel_zone(X, Y)
        path = (1.0 - smooth(-1.0, 0.5, e))
        weights = {key: bw[..., b] for b, key in enumerate(ZONE_KEYS)}
        weights["canyon"] = np.zeros(shape)
        zeros = np.zeros(shape)
        return Fields(weights, d, zeros, height, zf, np.clip(wall_band, 0, 1), zeros, tunnel, np.clip(path, 0, 1))

    def _inside_corridor(self, X, Y, height, e, i, zf, w, bw):
        """Suelo del camino segun el tramo: el rio (tarea 7) lo sustituye en el agua."""
        return height

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
```

- [ ] **Paso 4: comprobar que pasa**

Ejecutar: `uv run pytest -q Scripts/tests/test_terrain_path.py`
Esperado: PASS.
- Si `test_el_suelo_del_camino_es_llano_sin_cuenco` falla por el ruido de `n_floor`, se baja el `0.1` de `section` a `0.07`.
- Si falla por la esquina redondeada, se revisa que `rc = min(1.5, 0.2 w)`.
- El umbral de 0,3 m del test no se toca.

- [ ] **Paso 5: commit**

```bash
git add Scripts/terrain_path/field.py Scripts/terrain_path/model.py Scripts/tests/test_terrain_path.py
git commit -m "feat(tools): relieve a partir del camino (sección en cuadrado curvado, bordes, vistas y costa)"
```

---

### Tarea 7: el río como tramo del camino

**Ficheros:**
- Crear: `Scripts/terrain_path/river.py`.
- Modificar: `Scripts/terrain_path/model.py` (`_plan_extras` y `_inside_corridor`).
- Test: `Scripts/tests/test_terrain_path.py`.

**Interfaces:**
- Produce:
  - `Island(line, s, q, half_len, half_wid)`;
  - `River(sides: dict[int, tuple[np.ndarray, np.ndarray]], shelf: dict[int, tuple[np.ndarray, np.ndarray]], islands: list[Island])`, donde `sides[line] = (s_nudos, lado ±1)` y `shelf[line] = (s, ancho)`;
  - `plan_river(rng, model) -> River | None`;
  - `river_floor(model, X, Y, i, zf, w) -> (floor, weight)`.

- [ ] **Paso 1: escribir los tests que fallan**

```python
def test_las_islas_son_alargadas_y_siguen_el_rio(model):
    islands = model.river.islands
    assert len(islands) >= 6
    for isl in islands:
        assert isl.half_len / isl.half_wid >= 1.8


def test_no_hay_ristra_de_islas_en_el_centro(model):
    by_line = {}
    for isl in model.river.islands:
        by_line.setdefault(isl.line, []).append(isl)
    for line_id, isls in by_line.items():
        line = model.plan.graph.lines[line_id]
        prof = model.plan.profiles[line_id]
        run = best = 0
        for isl in sorted(isls, key=lambda a: a.s):
            w = np.interp(isl.s, line.arc, prof.half_width)
            run = run + 1 if abs(isl.q) < 0.25 * w else 0
            best = max(best, run)
        assert best <= 3


def test_el_rio_tiene_agua_honda_y_orilla_seca(model):
    main = model.plan.graph.main
    prof = model.plan.profiles[0]
    s_water = main.arc[prof.biome == 1]
    s = float(np.median(s_water))
    w = float(np.interp(s, main.arc, prof.half_width))
    h = _cross_section(model, 0, s, np.linspace(-0.9 * w, 0.9 * w, 19))
    assert h.min() < WATER_M - 0.5 and h.max() > WATER_M + 0.3
```

- [ ] **Paso 2: comprobar que fallan**

Ejecutar: `uv run pytest -q Scripts/tests/test_terrain_path.py -k "islas or ristra or rio"`
Esperado: FAIL (`model.river` es `None`).

- [ ] **Paso 3: implementar** `Scripts/terrain_path/river.py`

```python
"""Rio como tramo del camino: cauce hondo con una orilla-repisa que va cambiando de lado; en
cada cambio, una cadena de islas alargadas cruza en diagonal (se salta de una a otra). Islas
sueltas alargadas a los lados, nunca en ristra por el centro."""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

from terrain_vol.density import smooth

from .curves import knot_noise
from .layout import WATER_M

BANK_TOP_M = WATER_M + 0.7


@dataclass
class Island:
    line: int
    s: float
    q: float          # desplazamiento lateral (m) desde el eje, con signo
    half_len: float
    half_wid: float


@dataclass
class River:
    sides: dict[int, tuple[np.ndarray, np.ndarray]]
    shelf: dict[int, tuple[np.ndarray, np.ndarray]]
    islands: list[Island]


def _segments(mask: np.ndarray, arc: np.ndarray) -> list[tuple[float, float]]:
    out, start = [], None
    for k, flag in enumerate(mask):
        if flag and start is None:
            start = arc[k]
        if not flag and start is not None:
            out.append((start, arc[k - 1]))
            start = None
    if start is not None:
        out.append((start, arc[-1]))
    return out


def plan_river(rng: np.random.Generator, model) -> River | None:
    sides, shelf, islands = {}, {}, []
    for line in model.plan.graph.lines:
        prof = model.plan.profiles[line.id]
        for s0, s1 in _segments(prof.biome == 1, line.arc):
            if s1 - s0 < 20.0:
                continue
            knots = [s0]
            while knots[-1] < s1:
                knots.append(knots[-1] + float(rng.uniform(30.0, 60.0)))
            side = [float(rng.choice([-1.0, 1.0]))]
            for _ in knots[1:]:
                side.append(side[-1] if rng.random() < 0.25 else -side[-1])
            sides[line.id] = (np.array(knots), np.array(side))
            arc = line.arc[(line.arc >= s0) & (line.arc <= s1)]
            shelf[line.id] = (arc, knot_noise(rng, arc, (15.0, 40.0), 1.6, 3.2))
            for k in range(1, len(knots)):
                if side[k] == side[k - 1] or knots[k] >= s1:
                    continue
                w = float(np.interp(knots[k], line.arc, prof.half_width))
                b = float(np.interp(knots[k], *shelf[line.id]))
                q0, q1 = side[k - 1] * (w - b), side[k] * (w - b)
                n = max(1, int(np.ceil(abs(q1 - q0) / 3.2)) - 1)
                for m in range(1, n + 1):
                    t = m / (n + 1)
                    hw = float(rng.uniform(0.9, 1.2))
                    islands.append(Island(line.id, knots[k] + (t - 0.5) * 2.5 * n, q0 + (q1 - q0) * t,
                                          float(rng.uniform(1.9, 2.4)) * hw, hw))
            count = int(model.style.island_per_100m * (s1 - s0) / 100.0)
            for _ in range(count):
                s = float(rng.uniform(s0 + 5.0, s1 - 5.0))
                w = float(np.interp(s, line.arc, prof.half_width))
                hw = float(rng.uniform(0.8, 2.0))
                q = float(rng.choice([-1.0, 1.0])) * float(rng.uniform(0.3, 0.7)) * w
                islands.append(Island(line.id, s, q, float(rng.uniform(1.8, 3.0)) * hw, hw))
    if not sides:
        return None
    return River(sides, shelf, islands)


def river_floor(model, X, Y, i, zf, w):
    """(suelo del rio, peso 0..1 del rio) en cada punto, segun la muestra mas cercana i."""
    S, river = model.S, model.river
    shape = X.shape
    line, s, p, n = S["line"][i], S["s"][i], S["p"][i], S["n"][i]
    q = (X - p[..., 0]) * n[..., 0] + (Y - p[..., 1]) * n[..., 1]
    bed = WATER_M - 1.2 - 0.5 * model.n_floor.unit(X, Y)
    floor = bed.copy()
    weight = np.zeros(shape)
    for line_id, (knots, side) in river.sides.items():
        on = line == line_id
        if not on.any():
            continue
        k = np.clip(np.searchsorted(knots, s, side="right") - 1, 0, len(side) - 1)
        sd = side[k]
        b = np.interp(s, *river.shelf[line_id])
        bank = smooth(-0.6, 0.6, sd * q - (w - b) + 0.4 * model.n_top(X, Y))
        floor = np.where(on, bed * (1.0 - bank) + BANK_TOP_M * bank, floor)
        seg = (s >= river.shelf[line_id][0][0]) & (s <= river.shelf[line_id][0][-1])
        weight = np.where(on & seg, 1.0, weight)
    for isl in river.islands:
        on = line == isl.line
        r = np.sqrt(((s - isl.s) / isl.half_len) ** 2 + ((q - isl.q) / isl.half_wid) ** 2)
        top = BANK_TOP_M + 0.15 * model.n_floor(X, Y)
        floor = np.where(on & (r < 1.0), np.maximum(floor, bed + (top - bed) * (1.0 - smooth(0.7, 1.0, r))), floor)
    return floor, weight
```

En `model.py`, sustituir `_plan_extras` e `_inside_corridor`:

```python
    def _plan_extras(self) -> None:
        from .river import plan_river
        self.river = plan_river(self.extra_rng, self)
        self.castles = []

    def _inside_corridor(self, X, Y, height, e, i, zf, w, bw):
        if self.river is None:
            return height
        from .river import river_floor
        floor, weight = river_floor(self, X, Y, i, zf, w)
        inside = 1.0 - smooth(-0.5, 0.0, e)
        mix = weight * bw[..., 1] * inside
        return height * (1.0 - mix) + floor * mix
```

- [ ] **Paso 4: comprobar que pasa**

Ejecutar: `uv run pytest -q Scripts/tests/test_terrain_path.py`
Esperado: PASS. El test del suelo llano ya excluye el bioma de agua.

- [ ] **Paso 5: commit**

```bash
git add Scripts/terrain_path/river.py Scripts/terrain_path/model.py Scripts/tests/test_terrain_path.py
git commit -m "feat(tools): río como tramo del camino, con orillas que alternan e islas alargadas"
```

---

### Tarea 8: túneles (cruces y cerros) en 3D

**Ficheros:**
- Modificar: `Scripts/terrain_path/model.py` (`_build_samples`, `_stamps`, `_tunnel_zone` y `density`).
- Test: `Scripts/tests/test_terrain_path.py`.

**Interfaces:**
- Produce:
  - `model.tunnels`: lista de dicts `{"pts", "floor", "half", "t", "covered"}`, con arrays por muestra;
  - `model.tunnel_tree`, un KD sobre la concatenación;
  - `model._tunnel_query(X, Y) -> (u, t, floor, half)`;
  - `density` con el excavado.
- La geometría del hueco es la de `terrain_vol.density.MapModel._tunnel_carve`: elipse de semiancho `half·(1 + 0,22·n)`, alto `5·(1 + 0,18·n)` y suelo plano.

- [ ] **Paso 1: escribir los tests que fallan**

```python
from terrain_vol.mesh import build_chunk, standable_cells, z_levels  # noqa: E402


def _column(model, p):
    """Celdas pisables (cotas) de la columna en p, con la densidad 3D del modelo."""
    i, j = int(round(p[0] - model.axis[0])), int(round(p[1] - model.axis[0]))
    f = model.grid.window(i - 1, i + 2, j - 1, j + 2)
    X, Y = np.meshgrid(model.axis[i - 1:i + 2], model.axis[j - 1:j + 2], indexing="ij")
    D = model.density(X, Y, z_levels(), f)
    return z_levels()[np.nonzero(standable_cells(D)[1, 1])[0]]


def test_los_cruces_tienen_techo(model):
    for c in model.plan.crossings:
        levels = _column(model, c.point)
        lo = np.interp(c.s_lower, model.plan.graph.lines[c.lower].arc, model.plan.profiles[c.lower].z)
        hi = np.interp(c.s_upper, model.plan.graph.lines[c.upper].arc, model.plan.profiles[c.upper].z)
        assert np.any(np.abs(levels - lo) < 0.8), f"sin suelo de tunel en {c.point}: {levels}"
        assert np.any(np.abs(levels - hi) < 0.8), f"sin tablero en {c.point}: {levels}"


def test_los_tuneles_de_cerro_tienen_techo_y_suelo(model):
    main = model.plan.graph.main
    for _, s0, s1 in model.plan.hill_tunnels:
        s = 0.5 * (s0 + s1)
        levels = _column(model, main.point_at(s))
        z = np.interp(s, main.arc, model.plan.profiles[0].z)
        assert np.any(np.abs(levels - z) < 0.8) and levels.max() > z + 5.5
```

- [ ] **Paso 2: comprobar que fallan**

Ejecutar: `uv run pytest -q Scripts/tests/test_terrain_path.py -k techo`
Esperado: FAIL (no hay suelo de túnel, porque todo es macizo).

- [ ] **Paso 3: implementar** en `model.py`

Al final de `_build_samples`:

```python
        self.tunnels = []
        for line in self.plan.graph.lines:
            prof = self.plan.profiles[line.id]
            if not prof.tunnel.any():
                continue
            pts, s = resample(line.points, SAMPLE_STEP_M)
            k = np.clip(np.searchsorted(line.arc, s), 0, len(line.arc) - 1)
            covered = prof.tunnel[k]
            runs = np.split(np.arange(len(s)), np.nonzero(np.diff(covered.astype(int)))[0] + 1)
            for run in runs:
                if not covered[run[0]]:
                    continue
                lo, hi = max(run[0] - 5, 0), min(run[-1] + 5, len(s) - 1)     # 2,5 m de boca a cada lado
                sel = np.arange(lo, hi + 1)
                t = (s[sel] - s[run[0]]) / max(s[run[-1]] - s[run[0]], 1e-9)
                self.tunnels.append({"pts": pts[sel], "floor": np.interp(s[sel], line.arc, prof.z),
                                     "half": np.clip(np.interp(s[sel], line.arc, prof.half_width), 3.0, 4.5),
                                     "t": t, "covered": covered[sel]})
        if self.tunnels:
            self.tunnel_tree = cKDTree(np.vstack([tu["pts"] for tu in self.tunnels]))
            self.tunnel_cat = {k: np.concatenate([tu[k] for tu in self.tunnels]) for k in ("floor", "half", "t", "covered")}
        else:
            self.tunnel_tree = None
```

Métodos nuevos, que sustituyen a `_tunnel_zone` y `_stamps`:

```python
    def _tunnel_query(self, X, Y):
        d, k = self.tunnel_tree.query(np.stack([np.ravel(X), np.ravel(Y)], axis=1))
        c = self.tunnel_cat
        return (d.reshape(X.shape), c["t"][k].reshape(X.shape), c["floor"][k].reshape(X.shape),
                c["half"][k].reshape(X.shape), c["covered"][k].reshape(X.shape))

    def _tunnel_zone(self, X, Y):
        if self.tunnel_tree is None:
            return np.zeros(X.shape)
        u, _, _, half, _ = self._tunnel_query(X, Y)
        return 1.0 - smooth(half + 1.5, half + 5.0, u)

    def _stamps(self, X, Y, height, e, i, zf, w):
        """Cerro sobre cada tramo cubierto: roca >= 8 m sobre el suelo del tunel (el camino de
        arriba de un cruce ya queda 8,5 m por encima)."""
        if self.tunnel_tree is None:
            return height
        u, t, floor, half, covered = self._tunnel_query(X, Y)
        hill = (1.0 - smooth(half + 2.0, half + 10.0, u)) * covered * smooth(-0.1, 0.05, t) * smooth(-1.1, -0.95, -t)
        return np.maximum(height, (floor + 8.0 + 1.5 * self.n_top.unit(X, Y)) * hill + height * (1.0 - hill))

    def _carve(self, X, Y, Z3):
        u, t, floor, half, _ = self._tunnel_query(X, Y)
        half = half * (1.0 + 0.22 * self.n_tunnel(X, Y))
        height = 5.0 * (1.0 + 0.18 * self.n_tunnel(Y, X))
        v = Z3 - floor[..., None]
        center, radius_v = 0.42 * height[..., None], 0.62 * height[..., None]
        ellipse = 1.0 - np.sqrt((u[..., None] / half[..., None]) ** 2 + ((v - center) / radius_v) ** 2)
        inside = np.minimum(ellipse * half[..., None], v + 0.3)
        active = (t > -0.2) & (t < 1.2)
        return np.where(active[..., None], inside, -1.0)
```

En `density`, antes de `return D`:

```python
        if self.tunnel_tree is not None and np.any(f.tunnel > 0.0):
            carve = self._carve(X, Y, Z3)
            carve = carve + 0.5 * self.n_wall3d(X3 * 1.7, Y3 * 1.7, Z3 * 1.7) * (carve > -0.5)
            D = np.minimum(D, -carve)
```

Nota: el `smooth(-0.1, 0.05, t) * smooth(-1.1, -0.95, -t)` limita el cerro al tramo cubierto (t entre 0 y 1), así que no tapa las bocas.

- [ ] **Paso 4: comprobar que pasa**

Ejecutar: `uv run pytest -q Scripts/tests/test_terrain_path.py`
Esperado: PASS.

- [ ] **Paso 5: commit**

```bash
git add Scripts/terrain_path/model.py Scripts/tests/test_terrain_path.py
git commit -m "feat(tools): túneles de cruce y de cerro excavados en 3D en el camino"
```

---

### Tarea 9: castillos de arena

**Ficheros:**
- Crear: `Scripts/terrain_path/castles.py`.
- Modificar: `Scripts/terrain_path/model.py` (`_plan_extras` y `_stamps`).
- Test: `Scripts/tests/test_terrain_path.py`.

**Interfaces:**
- Produce:
  - `Castle(center, facing, base)`;
  - `plan_castles(rng, model) -> list[Castle]`;
  - `castle_stamp(X, Y, height, castle) -> ndarray`;
  - `CASTLE_RADIUS_M = 3.0`.

- [ ] **Paso 1: escribir los tests que fallan**

```python
def test_hay_castillos_en_ensanches(model):
    assert 2 <= len(model.castles) <= C01_STYLE.castles + 1


def test_los_castillos_dejan_paso(model):
    """Entre el castillo y la pared opuesta quedan 3 m o mas de suelo del camino."""
    for c in model.castles:
        h = []
        for dist in np.arange(4.5, 12.0, 0.5):
            p = c.center + c.facing * dist
            i, j = int(round(p[0] - model.axis[0])), int(round(p[1] - model.axis[0]))
            h.append(model.grid.height[i, j] - c.base)
        free = np.array(h) < 0.3
        assert free[:6].all(), f"castillo en {c.center} tapona el camino"
```

- [ ] **Paso 2: comprobar que fallan**

Ejecutar: `uv run pytest -q Scripts/tests/test_terrain_path.py -k castillo`
Esperado: FAIL (`model.castles` está vacío).

- [ ] **Paso 3: implementar** `Scripts/terrain_path/castles.py`

```python
"""Castillos de arena como estampa del relieve (prueba: si a 1 m de resolucion no quedan bien,
pasan a ser assets de los disenadores). Muralla redonda con puerta hacia el camino y cuatro
torres, en un ensanche, pegado a un lado para dejar paso."""

from __future__ import annotations

import math
from dataclasses import dataclass

import numpy as np

from terrain_vol.density import smooth

CASTLE_RADIUS_M = 3.0
MIN_WIDTH_M = 6.5


@dataclass
class Castle:
    center: np.ndarray
    facing: np.ndarray        # unitario, del castillo hacia el centro del camino
    base: float


def plan_castles(rng: np.random.Generator, model) -> list[Castle]:
    S = model.S
    plan = model.plan
    ok = S["open"] & (S["w"] >= MIN_WIDTH_M) & (S["biome"] != 1)
    main = plan.graph.main
    main_s = np.where(S["line"] == 0, S["s"], 1e9)
    ok &= (main_s > 40.0) & (main_s < main.length - 80.0) | (S["line"] != 0)
    blocked = [c.point for c in plan.crossings] + [line.points[k] for line in plan.graph.loops() for k in (0, -1)]
    candidates = np.nonzero(ok)[0]
    rng.shuffle(candidates)
    out: list[Castle] = []
    for idx in candidates:
        if len(out) >= model.style.castles:
            break
        p = S["p"][idx]
        if any(np.hypot(*(p - q)) < 30.0 for q in blocked) or any(np.hypot(*(p - c.center)) < 60.0 for c in out):
            continue
        if model.tunnel_tree is not None and model.tunnel_tree.query(p)[0] < 25.0:
            continue
        side = float(rng.choice([-1.0, 1.0]))
        n = S["n"][idx]
        center = p + n * side * (S["w"][idx] - CASTLE_RADIUS_M - 0.8)
        out.append(Castle(center, -n * side, float(S["z"][idx])))
    return out


def castle_stamp(X, Y, height, castle: Castle):
    dx, dy = X - castle.center[0], Y - castle.center[1]
    dist = np.hypot(dx, dy)
    if dist.min() > CASTLE_RADIUS_M + 3.0:
        return height
    yaw = math.atan2(castle.facing[1], castle.facing[0])
    ang = np.abs((np.arctan2(dy, dx) - yaw + math.pi) % (2.0 * math.pi) - math.pi)
    gate = smooth(0.8, 1.2, ang / (1.4 / CASTLE_RADIUS_M))
    wall = (1.0 - smooth(0.45, 0.9, np.abs(dist - CASTLE_RADIUS_M))) * gate
    out = np.where(wall > 0.0, np.maximum(height, castle.base + 1.3 * wall), height)
    for k in range(4):
        a = yaw + math.pi / 4.0 + k * math.pi / 2.0
        tx = castle.center[0] + CASTLE_RADIUS_M * math.cos(a)
        ty = castle.center[1] + CASTLE_RADIUS_M * math.sin(a)
        tower = 1.0 - smooth(0.9, 1.4, np.hypot(X - tx, Y - ty))
        out = np.where(tower > 0.0, np.maximum(out, castle.base + 2.6 * tower), out)
    return out
```

En `model.py`:
- En `_plan_extras`, sustituir la línea `self.castles = []` por:

```python
        from .castles import plan_castles
        self.castles = plan_castles(self.extra_rng, self)
```

- Al final de `_stamps`, justo antes del `return`, aplicar los castillos:

```python
        from .castles import castle_stamp
        for castle in self.castles:
            height = castle_stamp(X, Y, height, castle)
```

- Reorganizar `_stamps` para que el primer `return height` (el de `self.tunnel_tree is None`) no se salte los castillos. Queda así:

```python
    def _stamps(self, X, Y, height, e, i, zf, w):
        if self.tunnel_tree is not None:
            u, t, floor, half, covered = self._tunnel_query(X, Y)
            hill = (1.0 - smooth(half + 2.0, half + 10.0, u)) * covered * smooth(-0.1, 0.05, t) * smooth(-1.1, -0.95, -t)
            height = np.maximum(height, (floor + 8.0 + 1.5 * self.n_top.unit(X, Y)) * hill + height * (1.0 - hill))
        from .castles import castle_stamp
        for castle in self.castles:
            height = castle_stamp(X, Y, height, castle)
        return height
```

- `plan_castles` usa `model.tunnel_tree`, así que `_build_samples` (que lo crea) tiene que ir antes que `_plan_extras`. Es el orden que ya tiene `__init__`.

- [ ] **Paso 4: comprobar que pasa**

Ejecutar: `uv run pytest -q Scripts/tests/test_terrain_path.py`
Esperado: PASS.

- [ ] **Paso 5: commit**

```bash
git add Scripts/terrain_path/castles.py Scripts/terrain_path/model.py Scripts/tests/test_terrain_path.py
git commit -m "feat(tools): castillos de arena como estampa en los ensanches del camino"
```

---

### Tarea 10: CLI, mapa C01 completo y tests de recorrido

**Ficheros:**
- Crear: `Scripts/gen_terrain_path.py`.
- Test: `Scripts/tests/test_terrain_path.py`.
- Salida: `Scripts/terrain_volumes/Variants/C01_camino/` y `Variants/index.json`.

**Interfaces:**
- Consume: `PathModel`, `build_all`, `global_standable`, `zone_map`, `walk`, `ground_level`, `world_index` (todos con `grid=4`), `write_map(..., grid=4)` y `global_top(..., grid=4)`.
- Produce: `python Scripts/gen_terrain_path.py [--seed N]`, que escribe el mapa y deja su entrada la primera de `index.json`.

- [ ] **Paso 1: escribir los tests que fallan**

```python
from gen_terrain_volume import build_all, ground_level, walk, world_index  # noqa: E402


@pytest.fixture(scope="module")
def chunks(model):
    return build_all(model, grid=4)


@pytest.fixture(scope="module")
def reached(model, chunks):
    standable = global_standable(chunks, grid=4)
    start = world_index(model.start)
    return standable, walk(standable, (*start, ground_level(standable, *start)))


def test_se_llega_a_pie_del_inicio_al_final(model, reached):
    standable, seen = reached
    i, j = world_index(model.end)
    assert seen[i, j].any(), "no se llega al final andando o saltando"


def test_todos_los_lazos_se_recorren(model, reached):
    _, seen = reached
    for loop in model.plan.graph.loops():
        i, j = world_index(loop.point_at(loop.length / 2.0))
        assert seen[i - 2:i + 3, j - 2:j + 3].any(), f"lazo {loop.id} inalcanzable"


def test_las_vistas_no_se_pisan(model, reached):
    _, seen = reached
    region = model.region[1:-1, 1:-1]
    assert int((seen.any(axis=2) & (region == 2)).sum()) == 0


def test_los_trozos_vecinos_coinciden(chunks):
    left, right = chunks[(1, 1)], chunks[(2, 1)]
    a = left.vertices[np.isclose(left.vertices[:, 1], 5000.0, atol=1e-2)]
    b = right.vertices[np.isclose(right.vertices[:, 1], -5000.0, atol=1e-2)]
    assert len(a) > 0 and len(a) == len(b)
    a = a[np.lexsort((a[:, 2], a[:, 0]))][:, [0, 2]]
    b = b[np.lexsort((b[:, 2], b[:, 0]))][:, [0, 2]]
    assert np.allclose(a, b, atol=0.1)


def test_sin_picos_de_una_celda(model):
    from scipy import ndimage
    h = model.grid.height
    spike = np.abs(h - ndimage.median_filter(h, size=3))
    assert int((spike > 2.0).sum()) < 30 and int((spike > 4.0).sum()) == 0
```

Nota sobre los índices: `global_standable` va de -50 a 350 m (401 muestras), igual que `model.axis[1:-1]`. De ahí el recorte `region[1:-1, 1:-1]`, que alinea `region` con `seen`.

- [ ] **Paso 2: comprobar que fallan o pasan**

Ejecutar: `uv run pytest -q Scripts/tests/test_terrain_path.py -k "llega or lazos_se or vistas or vecinos or picos"`
Esperado: los tests se ejecutan (tardan unos 60 s: 16 trozos). Cualquier fallo es un defecto real de las tareas 6-9 y se arregla en la función que lo causa, con un commit propio:
- **fallan «vistas»** → sube `block_band_m` o `rim_*` en esa franja;
- **falla el recorrido en el río** → islas con más solape en `plan_river`;
- **fallan los picos** → baja el ruido `0.8 * n_top` de `section`.

- [ ] **Paso 3: implementar** `Scripts/gen_terrain_path.py`

```python
"""Genera un mapa "camino primero" (Docs/Diseno_Terreno_CaminoPrimero.md) en
Scripts/terrain_volumes/Variants/<nombre>/ (trozos TNTM2, manifest, vistas) y lo pone el primero
en Variants/index.json (el desplegable de ATN_MapVariantLoader en LVL_MapVariants).

    uv run --with numpy --with scipy --with pillow --with scikit-image python Scripts/gen_terrain_path.py
"""

from __future__ import annotations

import argparse
import json
import time
from pathlib import Path

from gen_terrain_volume import build_all, global_standable, ground_level, walk, world_index, zone_map
from terrain_path.layout import GRID
from terrain_path.model import PathModel
from terrain_path.style import C01_STYLE
from terrain_vol.export import global_top, write_map

VARIANTS = Path(__file__).resolve().parent / "terrain_volumes" / "Variants"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--seed", type=int, default=60001)
    args = parser.parse_args()
    t0 = time.time()
    style = C01_STYLE
    model = PathModel(args.seed, style)
    chunks = build_all(model, grid=GRID)
    standable = global_standable(chunks, grid=GRID)
    s_ij, e_ij = world_index(model.start), world_index(model.end)
    start = (*s_ij, ground_level(standable, *s_ij))
    ok = bool(start[2] >= 0 and walk(standable, start)[e_ij].any())
    top = global_top(chunks, grid=GRID)
    out = VARIANTS / style.name
    write_map(out, style.name, args.seed, chunks, (*model.start, float(top[s_ij])), (*model.end, float(top[e_ij])),
              zone_map(model, chunks, grid=GRID), model.route.points, style=style,
              extra_manifest={"description": style.description, "recorrible": ok}, grid=GRID)
    index_path = VARIANTS / "index.json"
    index = json.loads(index_path.read_text(encoding="utf-8")) if index_path.exists() else []
    index = [e for e in index if e["name"] != style.name]
    size_mb = sum(f.stat().st_size for f in out.rglob("*") if f.is_file()) / (1024 * 1024)
    index.insert(0, {"name": style.name, "seed": args.seed, "description": style.description,
                     "recorrible": ok, "size_mb": round(size_mb, 2)})
    index_path.write_text(json.dumps(index, indent=1, ensure_ascii=False), encoding="utf-8")
    g = model.plan.graph
    print(f"{style.name}: {'OK' if ok else 'NO RECORRIBLE'} {time.time() - t0:.1f}s principal {g.main.length:.0f} m, "
          f"{len(g.loops())} lazos, {len(model.plan.crossings)} cruces, {len(model.plan.hill_tunnels)} tuneles de cerro, "
          f"{len(model.river.islands) if model.river else 0} islas, {len(model.castles)} castillos, {size_mb:.1f} MB")


if __name__ == "__main__":
    main()
```

- [ ] **Paso 4: generar y pasar toda la suite**

Ejecutar:
1. `uv run --with numpy --with scipy --with pillow --with scikit-image python Scripts/gen_terrain_path.py`
   Esperado: `C01_camino: OK ... 6-7 lazos, 1-2 cruces, 2 tuneles de cerro, ... castillos, ~5 MB`.
2. `uv run pytest -q`
   Esperado: toda la suite en verde, las 53 anteriores más las nuevas.
3. Mirar `Scripts/terrain_volumes/Variants/C01_camino/preview_debug.png` con Read. Se comprueban a ojo:
   - el principal serpentea sin rectas;
   - los lazos vuelven;
   - el río tiene islas a los lados;
   - el abanico final llega al mar.

- [ ] **Paso 5: commit**

```bash
git add Scripts/gen_terrain_path.py Scripts/tests/test_terrain_path.py Scripts/terrain_volumes/Variants/C01_camino Scripts/terrain_volumes/Variants/index.json
git commit -m "feat(tools): mapa C01 del terreno camino primero y su generador"
```

---

### Tarea 11: agua cartoon y arena mojada

**Ficheros:**
- Modificar: `Scripts/build_grid_demo_assets.py`:
  - guarda `if __name__ == "__main__":`;
  - franja mojada en `build_terrain_material`.
- Crear: `Scripts/build_water_toon.py`.
- Verificación: script headless en el scratchpad y capturas con el MCP de Unreal.

**Interfaces:**
- Consume:
  - `build_water.custom_node`, `scalar_parameter`, `vector_parameter`, `texture_object`, `import_texture`, `mel`, `asset_lib`, `asset_tools` y `WATER_ROOT`;
  - `build_grid_demo_assets.build_terrain_material` y `build_grain_texture`.
- Produce:
  - `/Game/Environment/Water/M_TortunaboWaterToon`;
  - `M_GridTerrain` con los parámetros `WaterZ` (-400) y `WetPeriod` (4 s);
  - los actores `Water` y `WaterHorizon` de `LVL_MapVariants` con el material nuevo.

- [ ] **Paso 1: arena mojada en `M_GridTerrain`**

En `build_grid_demo_assets.py`, cambiar la última línea `main()` por:

```python
if __name__ == "__main__":
    main()
```

En `build_terrain_material`, sustituir el bloque que conecta `base_color` por:

```python
    # Arena mojada: la ola sube y baja por la orilla (periodo WetPeriod) y oscurece la arena
    # hasta donde llega; mas arriba, seca. Mismo periodo que la espuma de M_TortunaboWaterToon.
    wet = mel.create_material_expression(material, unreal.MaterialExpressionCustom, -600, -380)
    wet.set_editor_property("code", WET_HLSL)
    wet.set_editor_property("description", "ArenaMojada")
    wet.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    wet.set_editor_property("inputs", [custom_input(name) for name in ("Z", "T", "WaterZ", "Period")])
    world_z = mel.create_material_expression(material, unreal.MaterialExpressionComponentMask, -900, -380)
    world_z.set_editor_property("b", True)
    mel.connect_material_expressions(world_position, "", world_z, "")
    time = mel.create_material_expression(material, unreal.MaterialExpressionTime, -900, -300)
    water_z = scalar_parameter(material, "WaterZ", -400.0, -900, -220)
    period = scalar_parameter(material, "WetPeriod", 4.0, -900, -140)
    for expression, pin in ((world_z, "Z"), (time, "T"), (water_z, "WaterZ"), (period, "Period")):
        mel.connect_material_expressions(expression, "", wet, pin)
    vertex_color = mel.create_material_expression(material, unreal.MaterialExpressionVertexColor, -600, -180)
    grained = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -400, -100)
    mel.connect_material_expressions(vertex_color, "", grained, "A")
    mel.connect_material_expressions(triplanar, "", grained, "B")
    base_color = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -250, -160)
    mel.connect_material_expressions(grained, "", base_color, "A")
    mel.connect_material_expressions(wet, "", base_color, "B")
    mel.connect_material_property(base_color, "", unreal.MaterialProperty.MP_BASE_COLOR)
```

Constante nueva en el mismo fichero, junto a `TRIPLANAR_HLSL`:

```python
# Arena mojada (uu): la ola llega de 10 a 40 uu sobre el agua y vuelve; la arena que moja se
# oscurece y se seca en 25 uu de altura. Multiplicador del color.
WET_HLSL = """\
float wave = 0.5 + 0.5 * sin(T * 6.2831853 / max(Period, 0.1));
float reach = WaterZ + 10.0 + 30.0 * wave;
float dry = saturate((Z - reach) / 25.0);
return lerp(0.55, 1.0, dry);
"""
```

- [ ] **Paso 2: material de agua cartoon** en `Scripts/build_water_toon.py`

```python
"""Agua cartoon de Tortunabo: translucida sin luz (colores planos), tres bandas de color por
profundidad, espuma de borde neto que sube y baja por la orilla (mismo periodo que la arena
mojada de M_GridTerrain), lineas de brillo que se desplazan y ondas suaves de vertice.

Se ejecuta DENTRO del editor de Unreal (headless):
    UnrealEditor-Win64-DebugGame-Cmd.exe <uproject> -run=pythonscript
        -script=<repo>/Scripts/build_water_toon.py -EnablePlugins=PythonScriptPlugin
        -unattended -nosplash -nullrhi

Reconstruye tambien M_GridTerrain (arena mojada) y pone el agua nueva en LVL_MapVariants.
"""

import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_grid_demo_assets as grid  # noqa: E402
import build_water as water  # noqa: E402

MATERIAL_NAME = "M_TortunaboWaterToon"
LEVEL_PATH = "/Game/Maps/Run/LVL_MapVariants"

TOON_HLSL = """\
float depth = max(SceneD - PixD, 0.0);
float3 c0 = float3(0.46, 0.93, 0.86);
float3 c1 = float3(0.10, 0.70, 0.80);
float3 c2 = float3(0.04, 0.33, 0.64);
float3 col = depth < 60.0 ? c0 : (depth < 220.0 ? c1 : c2);
float a = depth < 60.0 ? 0.55 : (depth < 220.0 ? 0.82 : 0.95);
float wave = 0.5 + 0.5 * sin(T * 6.2831853 / max(Period, 0.1));
float n = Texture2DSample(Foam, FoamSampler, P.xy / 600.0 + T * float2(0.010, 0.004)).r;
float foam = step(D, (25.0 + 55.0 * wave) * (0.7 + 0.6 * n));
float glint = step(0.93, sin(dot(P.xy, float2(0.004, 0.0027)) + T * 0.9) * (0.6 + 0.4 * n));
col = lerp(col, float3(1.0, 1.0, 1.0), max(foam, 0.6 * glint));
return float4(col, max(a, foam));
"""


def build_material(foam_texture):
    path = f"{water.WATER_ROOT}/{MATERIAL_NAME}"
    if water.asset_lib.does_asset_exist(path):
        water.asset_lib.delete_asset(path)
    material = water.asset_tools.create_asset(MATERIAL_NAME, water.WATER_ROOT, unreal.Material,
                                              unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mel = water.mel
    world = mel.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -1400, 0)
    time = mel.create_material_expression(material, unreal.MaterialExpressionTime, -1400, 150)
    distance = mel.create_material_expression(material, unreal.MaterialExpressionDistanceToNearestSurface, -1400, 300)
    scene = mel.create_material_expression(material, unreal.MaterialExpressionSceneDepth, -1400, 450)
    pixel = mel.create_material_expression(material, unreal.MaterialExpressionPixelDepth, -1400, 600)
    foam = water.texture_object(material, "WaterFoam", foam_texture, -1400, 750)
    period = water.scalar_parameter(material, "WetPeriod", 4.0, -1100, 850)
    toon = water.custom_node(material, TOON_HLSL, ("SceneD", "PixD", "D", "P", "T", "Foam", "Period"),
                             unreal.CustomMaterialOutputType.CMOT_FLOAT4, -800, 300, "AguaCartoon")
    for source, pin in ((scene, "SceneD"), (pixel, "PixD"), (distance, "D"), (world, "P"), (time, "T"),
                        (foam, "Foam"), (period, "Period")):
        mel.connect_material_expressions(source, "", toon, pin)
    rgb = mel.create_material_expression(material, unreal.MaterialExpressionComponentMask, -500, 250)
    for c in ("r", "g", "b"):
        rgb.set_editor_property(c, True)
    alpha = mel.create_material_expression(material, unreal.MaterialExpressionComponentMask, -500, 400)
    alpha.set_editor_property("a", True)
    mel.connect_material_expressions(toon, "", rgb, "")
    mel.connect_material_expressions(toon, "", alpha, "")
    mel.connect_material_property(rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY)
    amplitude = water.scalar_parameter(material, "WaveAmplitude", 0.35, -1100, -200)
    waves = water.custom_node(material, water.WAVES_HLSL, ("P", "T", "Amplitude"),
                              unreal.CustomMaterialOutputType.CMOT_FLOAT3, -800, -150, "Oleaje")
    for source, pin in ((world, "P"), (time, "T"), (amplitude, "Amplitude")):
        mel.connect_material_expressions(source, "", waves, pin)
    mel.connect_material_property(waves, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    mel.recompile_material(material)
    water.asset_lib.save_loaded_asset(material)
    return material


def main():
    grid.build_terrain_material(grid.build_grain_texture())
    foam_texture = water.import_texture("T_WaterFoam", unreal.TextureCompressionSettings.TC_GRAYSCALE)
    material = build_material(foam_texture)
    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    level.load_level(LEVEL_PATH)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    count = 0
    for actor in actors:
        if actor.get_actor_label() in ("Water", "WaterHorizon"):
            actor.static_mesh_component.set_material(0, material)
            count += 1
    level.save_current_level()
    unreal.log_warning(f"[WaterToon] {MATERIAL_NAME} en {count} actores de {LEVEL_PATH}")


if __name__ == "__main__":
    main()
```

- [ ] **Paso 3: ejecutar en headless** (editor cerrado; preguntar a Rodrigo antes de cerrarlo)

Ejecutar:
`"C:/Program Files/Epic Games/UE_5.6/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe" "C:/Users/Rodrigo/PERSONAL/ProyectosPersonales/Tortunabo/Tortunabo.uproject" -run=pythonscript -script="C:/Users/Rodrigo/PERSONAL/ProyectosPersonales/Tortunabo/Scripts/build_water_toon.py" -EnablePlugins=PythonScriptPlugin -unattended -nosplash -nullrhi > "$TEMP/toon.log" 2>&1; grep "WaterToon\|Error" "$TEMP/toon.log"`

Esperado:
- `[WaterToon] M_TortunaboWaterToon en 2 actores de /Game/Maps/Run/LVL_MapVariants`;
- `Python script executed successfully`;
- ningún `Error:` del script.

- [ ] **Paso 4: verificar en el editor**

1. Abrir el editor en `LVL_MapVariants`.
2. Con MCP `execute_python`, poner en el cargador `variant = 'C01_camino'` y llamar a `recargar()`. Debe informar de 16 `ProceduralMeshComponent`.
3. Hacer capturas con `vision.capture_from`:
   - la salida;
   - un lazo;
   - un cruce (tablero y túnel);
   - un túnel de cerro;
   - el río con sus islas;
   - un castillo;
   - el abanico final con el mar;
   - la orilla con la espuma y la arena mojada.
4. Guardar las capturas en `Saved/Screenshots/WindowsEditor/C01_*.png` con `AutomationLibrary.take_high_res_screenshot`, una llamada por captura, y avisar a Rodrigo.

- [ ] **Paso 5: commit**

```bash
git add Scripts/build_grid_demo_assets.py Scripts/build_water_toon.py Content/Environment/Water/M_TortunaboWaterToon.uasset Content/Blueprints/Gameplay/GridMap/M_GridTerrain.uasset Content/Maps/Run/LVL_MapVariants.umap
git commit -m "feat(content): agua cartoon con espuma de orilla y arena mojada en el terreno"
```

---

### Tarea 12: cierre de la primera entrega

- [ ] **Paso 1:** `uv run pytest -q` → todo en verde. Tests UE: `Automation RunTests Tortunabo` → 58/58.
- [ ] **Paso 2:** actualizar la tarjeta del kanban `card-1790364451730-3wzrsi` con el resultado y los commits.
- [ ] **Paso 3:** enseñar a Rodrigo las capturas y la vista cenital, y esperar su validación. **Sin push** hasta entonces (spec §8). Tras su visto bueno llegan las 30 variantes, el push y la limpieza de memoria con Astra, cada una con su propio plan.

> **Obsoleto** (2026-10-02): plan de implementación ya ejecutado; el diseño vigente está en `Docs/Diseno_Terreno_CaminoPrimero.md`.

# Terreno «camino primero» v2 — plan de implementación

> **Para agentes:** SUB-SKILL: superpowers:executing-plans (ejecución nativa). Pasos con casillas `- [ ]`.

**Objetivo:** C01 con cruces de 1 a 4 (túnel bajo cerro o puente fino), tablero 3D fundido con el terreno, barranco mortal con río, paredes de cima natural y textura distinta en suelo y pared.

**Arquitectura:** Todo vive en `Scripts/terrain_path/` (generador offline en Python). El tablero y el barranco son módulos nuevos (`bridge.py`, `canyon.py`) que `model.py` consulta en `_fields` (2D) y `density` (3D). Las cajas de muerte viajan en el manifest y las pone `ATN_MapVariantLoader` (C++) en `BeginPlay`.

**Stack:** Python 3 + numpy/scipy/scikit-image (vía `uv run --with ...`), pytest, Unreal 5.6 C++.

**Spec:** `Docs/2026-09-26-Terreno-CaminoPrimero-v2-Design.md`

## Restricciones globales

- Cruces por mapa: 1–4. Puente fino: tablero de 3–4 m de ancho, losa de 1,5–2 m, hueco libre ≥ 5 m. Túnel: ≥ 8 m de roca sobre el suelo.
- Barranco: 20–35 m de ancho, 8–14 m bajo el camino que lo cruza, río en el fondo a `WATER_M`.
- Pie de pared 3–3,5 m casi vertical (`field.FOOT_M = 3.5`), sin cambios.
- Semiancho: `width_m = (4.0, 6.5, 10.5)`.
- Semilla de C01: 60001. Los añadidos usan el generador `extra` (semilla + 17) para no mover el grafo.
- Comando de tests: `cd Scripts && uv run --with numpy --with scipy --with scikit-image --with pytest python -m pytest tests/test_terrain_path.py -q`
- Commits: uno por tarea, `feat(tools): ...`, sin Co-Authored-By.

## Foco de revisión

1. El tablero fino desaparece tras el suavizado Taubin de la malla → comprobar sobre la malla exportada (no solo sobre la densidad) que hay vértices del tablero encima del cruce.
2. El barranco corta un lazo o el principal fuera del puente → el camino queda partido; la colocación debe rechazar trazados que corten otro camino.
3. Las cajas de muerte tapan el puente o una orilla andable → la caja no sube por encima de `WATER_M + 2 m`.
4. La cima natural baja del pie en algún tramo y abre salida a las vistas → `test_las_vistas_no_se_pisan` sigue verde.
5. Un cruce de tipo puente sobre un camino de abajo muy ancho supera 4 m de tablero → el ancho del tablero es fijo (3–4 m) e independiente de la luz.

---

### Tarea 1: Estilo v2

**Ficheros:** `Scripts/terrain_path/style.py`, `Scripts/tests/test_terrain_path.py`

**Produce:** `PathStyle.crossings: tuple[int, int] = (1, 4)`, `bridge_share: float = 0.5`, `canyon: str = "deadly"` (`"none" | "deadly" | "walkable"`), `canyon_width_m = (20.0, 35.0)`, `canyon_depth_m = (8.0, 14.0)`, `crest_extra_m = (0.0, 7.0)`, `crest_roughness: float = 1.0`, `block_band_m = (3.0, 18.0)`, `width_m = (4.0, 6.5, 10.5)`, `wall_color_mix: float = 1.0`.

- [ ] Test `test_estilo_c01` pasa a exigir `C01_STYLE.crossings == (1, 4)`, `C01_STYLE.width_m[1] == 6.5`, `C01_STYLE.canyon == "deadly"`. Ejecutar: falla.
- [ ] Cambiar `style.py`. `graph.build_graph` lee `style.crossings` como rango: `n_cross = int(rng_extra.integers(lo, hi + 1))` con el generador aparte; `variants.py` pasa sus `crossings=3` a `(3, 3)` y `crossings=0` a `(0, 0)`.
- [ ] `block_band_m` pasa a rango: en `model._fields`, `band = lo + (hi - lo) * self.n_band.unit(X, Y)` con `self.n_band = Fbm2D(extra, 80.0, 2)`.
- [ ] Tests verdes. Commit.

### Tarea 2: Cruces de 1 a 4 y su tipo

**Ficheros:** `Scripts/terrain_path/graph.py` (`Crossing`), `profile.py` (`_profiles`), tests.

**Produce:** `Crossing.kind: str` (`"bridge" | "tunnel"`), asignado en `_profiles` con `rng.random() < style.bridge_share` (generador `extra` pasado a `build_plan`).

- [ ] Test `test_cruces_de_uno_a_cuatro_con_tipo`: `1 <= len(plan.crossings) <= 4` y todos con `kind in ("bridge", "tunnel")`.
- [ ] `build_graph`: los primeros `n_cross` lazos del principal intentan cruzar (hasta 80 intentos cada uno, como ahora); si tras todos los lazos hay 0 cruces, reintentar el grafo (`Infeasible`).
- [ ] `_profiles`: tipo `tunnel` marca el camino de abajo como cubierto `w_up + 8 m` a cada lado (reusa el bloque hoy muerto tras el `continue`, que se borra); tipo `bridge` marca solo `w_up + 1,5 m`.
- [ ] Tests verdes. Commit.

### Tarea 3: Tablero 3D fundido (`bridge.py`)

**Ficheros:** crear `Scripts/terrain_path/bridge.py`; modificar `model.py` (`_build_samples`, `_stamps`, `density`, `_plan_arches`); tests.

**Produce:**

```python
@dataclass
class Deck:
    pts: np.ndarray        # (N, 2) eje del tablero, cada 0,5 m
    top: np.ndarray        # (N,) cota del tablero (suelo del camino de arriba)
    half: float            # semiancho del tablero, 1,5-2,0 m
    thick: float           # grosor en el centro, 1,5-2,0 m
    span: float            # luz (m) entre estribos

def plan_decks(model, rng) -> list[Deck]: ...
def deck_density(decks, X, Y, Z3) -> np.ndarray: ...   # > 0 dentro de la losa
```

- Losa: `inside = min(half - |u|, top - Z, Z - (top - thick - arch(t)))`, con `arch(t) = 1.2 * (1 - (2t - 1)^2)` más grosor hacia los estribos (el arco inferior sube en el centro de la luz: más fino en medio, más grueso en los apoyos) y ruido 3D de 0,3 m en la cara inferior.
- Fusión: `D = soft_max(D_tras_excavar, deck, k=1.5)` (el `soft_max` de `terrain_vol.density`); en los extremos del tablero, 4 m de rampa de fusión con la pared.
- El camino de arriba de un cruce `bridge` se estrecha: `half_width` baja a `deck.half` en `span/2 + 4 m` alrededor del cruce (antes de muestrear en `_build_samples`).
- Los arcos de `_plan_arches` pasan a ser `Deck` sin camino encima (su `top` = suelo del camino de abajo + 6,3 m).

- [ ] Test `test_los_puentes_tienen_losa_gruesa_y_hueco`: en cada cruce `bridge`, con `_column`, el sólido por encima del suelo de abajo empieza a ≥ 5 m y la losa mide ≥ 1,4 m; hay suelo pisable a la cota del de arriba.
- [ ] Test `test_el_tablero_sobrevive_a_la_malla`: con el fixture `chunks`, hay ≥ 20 vértices en un radio de 2 m del punto de cruce con `z` entre `top - 0,5` y `top + 0,5`.
- [ ] Implementar. Tests verdes (incluidos `test_los_cruces_tienen_techo` y el recorrido). Commit.

### Tarea 4: Paredes con cima natural

**Ficheros:** `Scripts/terrain_path/model.py` (`_fields`), `field.py` (`section`), tests.

- Sustituir el lomo `ridge = (...) * sin(pi*k)^2` por ruido multiescala: `crest = extra_lo + (extra_hi - extra_lo) * n_crest_big.unit + crest_roughness * (1.5 * n_crest_mid + 0.6 * n_crest_fine)`, con `Fbm2D(extra, 60, 2)`, `Fbm2D(extra, 18, 3)` y `Fbm2D(extra, 6, 2)`; se aplica solo con `e > crest_e` y se desvanece hacia las vistas con el mismo `t` de la bajada.
- La bajada a las vistas (`run`) varía también con `n_band`: `run = max(10, (1.2 + 1.6 * n_band.unit) * (H - v))`.

- [ ] Test `test_la_cresta_no_es_meseta`: a lo largo del principal, cada 10 m, altura máxima del perfil lateral entre `w` y `w + 25 m`; desviación típica ≥ 1,5 m, y en la banda de cresta menos del 20 % de celdas tienen un `ptp` 5×5 < 0,3 m.
- [ ] Test `test_la_pared_acaba_a_distancias_distintas`: distancia desde el borde del camino al punto donde el perfil baja 3 m de su máximo; desviación típica ≥ 4 m.
- [ ] Implementar. Tests verdes, incluidos `test_el_borde_cierra_el_paso`, `test_las_vistas_no_se_pisan` y `test_sin_picos_de_una_celda`. Commit.

### Tarea 5: Barranco mortal (`canyon.py`)

**Ficheros:** crear `Scripts/terrain_path/canyon.py`; modificar `model.py`, `profile.py` (cota del principal en el cruce), `gen_terrain_path.py` (manifest), tests.

**Produce:**

```python
@dataclass
class Canyon:
    pts: np.ndarray          # (N, 2) eje de oeste a este, cada 1 m
    half: np.ndarray         # (N,) semiancho 10-17,5 m
    s_main: float            # arco del principal donde lo cruza
    mode: str                # "deadly" | "walkable"

def plan_canyon(model, rng) -> Canyon | None: ...
def canyon_height(canyon, X, Y, height) -> np.ndarray: ...   # baja el relieve al fondo
def kill_boxes_uu(canyon) -> list[dict]: ...                 # {"center": [x, y, z], "extent": [ex, ey, ez], "yaw": deg}
```

- Trazado: `steer_walk` de `Y = MAP_MIN_M` a `Y = MAP_MAX_M` con waypoints aleatorios en X; se acepta si corta el principal exactamente una vez con ángulo ≥ 50° y a ≥ 40 m de uniones, cruces y túneles, y no toca ningún lazo (distancia ≥ semiancho + ancho + 8 m). Hasta 200 intentos; si no, `None`.
- Cota: en el cruce, el principal se fija a ≥ `WATER_M + 1.5 + 8 m` (pin llano ±`span/2 + 6 m`, como `PIN_FLAT_M`).
- Relieve: dentro del barranco, `height = min(height, fondo)` con fondo `WATER_M - 1,5` y orillas de arena de 1,5–3 m a `WATER_M + 0,5`; paredes con el mismo pie y cima natural de la tarea 4.
- El cruce del principal es un `Deck` (tarea 3) de luz `2 * half + 6`.
- Cajas: una por tramo de 20 m del eje, `z` de `WATER_M - 2` a `WATER_M + 2`, anchura del cauce.

- [ ] Test `test_el_barranco_cruza_el_principal_una_vez`, `test_el_barranco_no_toca_lazos`, `test_el_barranco_tiene_puente_y_fondo_con_agua`.
- [ ] Test `test_las_cajas_cubren_el_fondo_y_no_el_puente`: toda muestra del fondo está dentro de alguna caja; el tablero queda ≥ 4 m por encima de la cara superior de las cajas.
- [ ] `check()` de `gen_terrain_path.py`: el recorrido excluye las celdas dentro de las cajas (caer = morir) y aun así llega al final.
- [ ] `write_map(..., extra_manifest={"kill_boxes_uu": ...})`. Commit.

### Tarea 6: Cajas de muerte en `ATN_MapVariantLoader`

**Ficheros:** `Source/Tortunabo/Public/World/TN_MapVariantLoader.h`, `Source/Tortunabo/Private/World/TN_MapVariantLoader.cpp`.

- En `BeginPlay` (solo con autoridad), leer `kill_boxes_uu` del manifest y hacer `SpawnActor<ATN_DeathZoneVolume>` en `center` con `FRotator(0, yaw, 0)`, `TriggerBox->SetBoxExtent(extent)`. Guardar en `TArray<TWeakObjectPtr<AActor>> SpawnedKillZones`; destruirlas en `EndPlay`.
- [ ] Compilar `TortunaboEditor Win64 DebugGame` con el editor cerrado. Verde.
- [ ] Commit.

### Tarea 7: Texturas de suelo y pared

**Ficheros:** `Scripts/terrain_vol/mesh.py` (`vertex_colors`), `Scripts/terrain_path/model.py` (atributos), `Scripts/build_grid_demo_assets.py` (`build_terrain_material`).

- `vertex_colors`: `cliff = smooth(cos(30°), cos(40°), 1 - n_z)` → mezcla con la paleta de pared multiplicada por `getattr(model, "wall_color_mix", 0.6)` (C01: 1.0); vetas horizontales en pared: `vein_z = 0.5 + 0.5 * sin(z * 2π / 0.9 + 0.8 * sin(x / 13))`, oscureciendo un 12 %.
- Material: en `build_terrain_material`, `GrainTileSize` pasa a ser `lerp(FloorTile, WallTile, steep)` con `steep = 1 - saturate((N.z - 0.75) / 0.15)`; `WallTile = 160`, `FloorTile = 400`. Regenerar `M_GridTerrainWet` con `build_water_toon.py` en el editor.
- [ ] Test `test_suelo_y_pared_tienen_color_distinto`: con normales `(0,0,1)` y `(1,0,0)` en el mismo punto, la distancia RGB ≥ 0,12.
- [ ] Commit.

### Tarea 8: Generar C01 y validar

- [ ] `cd Scripts && uv run --with numpy --with scipy --with pillow --with scikit-image python gen_terrain_path.py` → `C01_camino: OK`, `n` cruces, barranco, vistas pisadas 0.
- [ ] Batería completa de tests verde.
- [ ] Editor: abrir `LVL_MapVariants`, `Recargar` en el cargador, capturas del puente, un cruce, el barranco y las paredes. Rodrigo valida.
- [ ] Commit de la variante y del nivel.

(El modo `walkable` del barranco y el catálogo de 30 van en un plan aparte, tras validar C01.)

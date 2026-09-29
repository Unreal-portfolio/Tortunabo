# C — Mapas geográficos (España → Europa → Mundo) para carreras de buggies

Solo lectura sobre `main` (41a0ee8a5). «Medido» = ejecutado con el código del repo, sin escribir.

## 1. Pipeline actual de España

- **Datos**: teselas *terrarium* de AWS Terrain Tiles (mezcla SRTM, GMTED, ETOPO1…) a zoom 7 (≈ 940 m/px a 40° N) y frontera Natural Earth 1:50m. `terrain_geo/fetch_spain.py` descarga con `urllib`, cachea en `Saved/terrain_geo_cache/` (no versionado) y deja `terrain_geo/data/ES_dem.png` (16 bits, cota + 5000 m, 1,5 MB) y `ES_mask.png` (cobertura), versionados.
- **Proyección**: Web Mercator a mano (`layout.py`), centro fijo, `MERC_M_PER_M = 2800` → 1 m ≈ 2,1 km; distorsión N‑S del 12 % entre 36° y 44°.
- **Modelo** (`model.py`): relieve suavizado σ 1,5 m, `limit_slope` (90 iteraciones), ×K = 40/3480, ruido fino fBm, fusión tierra/mar por cobertura. Exageración efectiva **24,7×** (medido; el doc dice ~20×).
- **Cadena común**: densidad 3D (89 niveles Z de 0,5 m) → marching cubes + Taubin → TNTM2 (zlib, cuantizado) → `ATN_MapVariantLoader` (un `UProceduralMeshComponent` por trozo, colisión compleja cocinada **síncrona**, lee de `Scripts/`: solo editor/PIE; para build hay que pasar por `import_terrain_mesh.py` → StaticMesh).
- **Coste**: modelo 8 s (medido), mapa completo ~25 s; E01 = 36 trozos, 1,08 M triángulos, 6,3 MB en 0,36 km² (≈ 3 M tri/km²).
- **Licencias**: Natural Earth, dominio público. Terrain Tiles: la mayoría de fuentes son dominio público, pero algunas exigen atribución (EU‑DEM/Copernicus, Kartverket, CDEM Canadá, Australia CC‑BY…). Hoy no hay fichero de créditos en el juego: **obligatorio antes de Steam**.
- **Conducibilidad actual (medido)**: pendiente en tierra p50 18°, p90 45°; solo el 54 % de España está por debajo de 20° y el 27 % de los pasos de 1 m supera 0,4 m. **E01 no es conducible por un buggy**; y cruzarlo entero a 80 km/h lleva 27 s.

## 2. Generalización a «cualquier región»

Sustituir las constantes de `layout.py`/`model.py` por un `GeoRegion` (dataclass congelada, serializada en el manifest):

| Parámetro | Uso |
|---|---|
| `bbox` o `center` + `extent_km` | ventana geográfica |
| `projection` (cadena PROJ) | vía `pyproj` (MIT) en lugar del Mercator a mano |
| `km_per_game_m` | escala horizontal |
| `exaggeration` | vertical relativa (no «cima en m»): pendiente_juego = pendiente_real × E |
| `sea_level_m` | desplazar el nivel del mar (temáticos: glaciación −120 m, deshielo +70 m) |
| `mask` | país (NE admin‑0), costa (NE land) o ninguna |
| `smooth_sigma`, `max_slope_deg` | suavizado global y talud |
| `route` | puntos de paso, anchura, pendiente máx. de calzada (§4) |
| `dem_source` | terrarium (zoom automático), GEBCO, ETOPO, Copernicus |

Zoom automático terrarium: resolución = 156 543·cos(lat)/2^z m/px; elegir z tal que el píxel real ≤ `km_per_game_m`·1000·resolución_raster.

**Fuentes compatibles con uso comercial** (todas con atribución en créditos):

| Fuente | Resolución | Cobertura | Licencia | Uso |
|---|---|---|---|---|
| AWS Terrain Tiles | hasta ~30 m | global, con batimetría | mixta, atribución por fuente | región y país (ya integrado) |
| Copernicus DEM GLO‑30/90 | 30/90 m | global | gratuita, comercial con atribución | Europa, islas, ciudades |
| GEBCO 2025 | 15″ (~450 m) | global, tierra + fondo | dominio público con cita | mundo, mapas «sin agua» |
| ETOPO 2022 (NOAA) | 15/30/60″ | global | dominio público (EE. UU.) | mundo |
| SRTM | 30 m | 56° S–60° N | dominio público | no cubre Escandinavia ni Islandia |
| Natural Earth | 1:10m/50m | global | dominio público | costa, lagos, ríos, ciudades |
| OpenStreetMap | — | — | ODbL (share‑alike en BD derivadas) | evitar o usar solo como *produced work* |
| NASA LOLA/MOLA | ~60–460 m | Luna, Marte | dominio público | temáticos |

**Proyección**:
- Región/país: transversa de Mercator o azimutal equidistante centrada en el bbox (distorsión < 1 % a 1 000 km).
- Europa: LAEA Europa (EPSG:3035), el estándar de la UE.
- Mundo: Mercator descartado (infinito en los polos). Recomiendo **equirectangular recortada a ±72°** (rectángulo, meridianos rectos, fácil de leer) o **Equal Earth** si se quiere tamaño real de continentes (bordes curvos: agua). Antártida y Ártico fuera o como franja; el mundo no se cierra E‑O (no hay envoltura sin teletransporte).
- **Riesgo**: las fronteras de Natural Earth incluyen territorios en disputa (Crimea, Cachemira, Sáhara, Taiwán). Para Steam global, los mapas continentales deben usar **costa**, no fronteras políticas.

## 3. Límites en UE 5.6 y escalas propuestas

**Tamaño de pista**: 3–6 min a una media realista de 60–70 km/h (curvas, subidas) son **3,2–7 km** de recorrido. Un circuito de 1,5–2,5 km a 2–3 vueltas cabe en un mapa de 1–2 km; un punto a punto de 5 km pide un mapa de 3–5 km de lado.

**La malla volumétrica no escala**: a 3 M tri/km², 16 km² serían ~48 M triángulos, ~280 MB y colisión cocinada en carga. Los mapas geográficos son campos de altura puros, así que el destino es **UE Landscape**: heightmap de 16 bits, colisión *heightfield*, LOD y streaming nativos, el caso probado de Chaos Vehicles. TNTM2 queda para mapas con voladizos (C01, P01) o piezas locales (túneles, puentes).

- **World Partition**: innecesario hasta ~4 km; recomendado desde 8 km (en *listen server* el anfitrión carga las celdas de los 8).
- **LWC y precisión**: posiciones en doble desde UE 5.0 (Chaos incluido). Los riesgos reales son materiales con posición de mundo y Niagara lejos del origen (> 10–20 km) y la cuantización de red (`FVector_NetQuantize`, ±2²⁰ uu ≈ ±10,5 km por eje; comprobar en 5.6 el camino de desbordamiento). **Centrar los mapas en el origen** (hoy van de −50 a 550 m) y no pasar de ±8 km.
- **Memoria**: Landscape de 4 033² a 1 m ≈ 32 MB de alturas; 8 129² ≈ 130 MB. Sin problema.

| Mapa | Lado de juego | 1 m de juego ≈ | E sugerida | Zoom/fuente |
|---|---|---|---|---|
| España (Península + Baleares) | 4 km | 320 m | 3–5 | terrarium z9 |
| Europa (Lisboa–Urales, Islandia–Creta) | 8 km | 500 m | 4–6 | terrarium z8 / GLO‑90 |
| Mundo (±72°) | 16 × 6,4 km | 2,5 km | 10–15 | ETOPO 60″ / GEBCO |
| Isla o cordillera (Alpes, Islandia) | 3–4 km | 100–250 m | 1,5–3 | z10 / GLO‑30 |

Las carreras usan **tramos** de estos mapas (Pirineo, Alpes, París–Dakar), no la extensión completa.

## 4. Conducibilidad

1. **Calibrar la exageración por la pendiente, no por la cima.** Medido en España a ~1 km/px: pendiente real p95 = 9°. Para p95 < 20° en todo el mapa, E ≤ 2,3; el E01 usa 24,7. Solución: E de 3–6 para que el relieve se reconozca y una **calzada tallada** por los valles y puertos.
2. **Ruta automática**: coste mínimo con `skimage.graph.MCP_Geometric` (scikit-image ya es dependencia). Coste = distancia × (1 + a·pendiente²), infinito si la pendiente > θmáx (20°), en el mar o fuera del bbox. Puntos de paso: ciudades de NE *populated places* o lat/lon a mano (Roncesvalles → Santiago ya existe). Después, Catmull-Rom y remuestreo (`terrain_vol/route.py`).
3. **Tallado del corredor**: perfil longitudinal suavizado a lo largo del arco (gaussiana de 50–100 m y rampa ≤ 12–15 %), anchura de 12–16 m para 8 buggies, arcenes de 10–20 m con fundido; peralte transversal < 8°; radio mínimo ≥ 25 m. Relajar `limit_slope` fuera del corredor.
4. **Validación** (test, no captura): pendiente máxima y p99 del corredor, paso vertical máximo entre muestras de 1 m ≤ 0,4 m, radio mínimo, anchura libre y `flood` con `CLIMB_M` = tan 20° en vez de 1 m.
5. **Checkpoints**: cada 150–250 m de arco y en los picos de curvatura, orientados por la tangente; se exportan como `checkpoints_uu` en el manifest (igual que `kill_boxes_uu`). En carrera, el mar reaparece al jugador en el último checkpoint en vez de matarlo.
6. **Ríos y lagos**: el MDE no los resuelve (≥ 1 km/px). Tallarlos con NE `rivers_lake_centerlines` y `lakes` (anchura mínima de juego 6–10 m); vados de < 0,3 m conducibles con frenado y puentes generados donde la ruta cruce un cauce profundo. **Hoy hay un único plano de agua a −4 m**: los lagos de montaña necesitan su propia cota (Water plugin o lámina por lago).

## 5. Catálogo propuesto

| Mapa | Escala | Por qué es divertido |
|---|---|---|
| Galápagos | 1 m ≈ 100 m | islas de tortugas, volcanes, lava negra y arena (encaja con Tortunabo) |
| Islandia | 1 m ≈ 150 m | volcanes, ríos glaciares que vadear, arena negra |
| Canarias | 1 m ≈ 120 m | Teide; islas separadas por mar (saltos o ferris) |
| Mallorca | 1 m ≈ 40 m | Tramuntana, Sa Calobra (curvas de herradura) |
| Alpes | 1 m ≈ 200 m | puertos reales (Stelvio, Gotardo) como pista |
| Pirineo/Camino | 1 m ≈ 150 m | continuidad con E01 |
| París–Dakar (Mundo o Europa+África) | 1 m ≈ 1–2,5 km | Sahara de dunas, icono de los buggies |
| Gran Cañón / Monument Valley | 1 m ≈ 20–50 m | bajadas al cañón, mesas |
| San Francisco, Lisboa, Río | 1 m ≈ 3–10 m | colinas icónicas; sin edificios (OSM es ODbL) se queda flojo |
| Luna, Marte (Olympus Mons, Valles Marineris) | 1 m ≈ 0,5–5 km | NASA, dominio público; gravedad baja como modificador |
| Mundo sin agua (GEBCO) | 1 m ≈ 2,5 km | dorsal atlántica, fosa de las Marianas |
| Europa en la glaciación (−120 m) | 1 m ≈ 500 m | Doggerland: Gran Bretaña unida al continente |
| Mapa desde foto (F01) | libre | contenido de la comunidad |

## 6. Tareas

| # | Tarea | h |
|---|---|---|
| 1 | Prototipo de buggy Chaos en pista de prueba: medir el ángulo y el escalón máximos reales para calibrar θmáx | 6–10 |
| 2 | `GeoRegion` + `pyproj` + fetch genérico (zoom automático, caché por zoom), tests | 8–12 |
| 3 | Adaptadores GEBCO/ETOPO/Copernicus (`rasterio`) y capas NE; fichero de atribuciones | 7–9 |
| 4 | Exportación de heightmap 16 bits (tamaños válidos de Landscape, escala Z) e importador headless a Landscape | 12–20 |
| 5 | Ruta de coste mínimo, tallado de corredor y validación de conducibilidad | 16–24 |
| 6 | Checkpoints en el manifest y actores de checkpoint/respawn en C++ | 10–16 |
| 7 | Ríos, lagos con cota propia, vados y puentes | 12–20 |
| 8 | Proyección mundial (equirectangular ±72° / Equal Earth) y origen centrado | 4–6 |
| 9 | Prueba de rendimiento en PIE con 8 buggies (World Partition, red) | 6–10 |
| 10 | Mapas del catálogo: generación y playtest | 3–4 cada uno |

Total del núcleo (1–9): **81–127 h**.

**Riesgos**
- **Alto**: importar Landscape por Python headless en 5.6 está poco documentado; puede hacer falta un commandlet en C++.
- **Alto**: replicar Chaos Vehicles con 8 jugadores en *listen server*; la predicción de física en red de 5.6 es experimental. No depende del mapa, pero bloquea el modo.
- **Medio**: reconocibilidad frente a conducibilidad. Con E baja, Europa es plana; el corredor tallado lo compensa.
- **Medio**: licencias. Hace falta la lista de atribuciones de Terrain Tiles y Copernicus; nada de OSM sin revisión ODbL; fronteras disputadas.
- **Medio**: disponibilidad de AWS Terrain Tiles; se mitiga versionando los rasters derivados, como ya se hace.
- **Bajo**: el loader no se empaqueta; en Steam, assets cocinados.

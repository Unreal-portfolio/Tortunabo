# Terreno por stamps — diseño

Fecha: 2026-09-21
Estado: diseño aprobado en brainstorming, pendiente de plan de implementación
Rama de partida: `feat/procgen-terrain` @ 78d957c
Sustituye a: `2026-09-21-Terreno-Modular-Design.md` (ver sección 9)

## 1. Problema

El terreno del grid procedural se genera evaluando ruido Perlin en runtime
(`TN_GridTerrainDecisions.h`). El diseñador no puede intervenir en la forma y el relieve
no se lee como natural. El diseño anterior resolvía la autoría con módulos de celda
completa cosidos por un perfil de borde canónico; al revisarlo aparecieron tres requisitos
que ese diseño no cubre:

1. **Blend automático.** Dos piezas contiguas pueden empezar a cotas distintas y la unión
   no debe verse. Con celdas cosidas la junta existe y hay que casarla.
2. **Compatibilidad con los puzzles.** Los puzzles son prefabs hechos a mano que hoy viven
   en `ATN_ChunkManager`. Tienen que poder asentarse sobre terreno generado.
3. **Autoría por diseñadores y por script.** Los diseñadores esculpen y retocan; además
   debe poder generarse una tanda de piezas a partir de imágenes y bocetos.

Objetivo final: generación de mapas compatible con los puzzles, limpia, distinta en cada
partida y divertida.

## 2. No-objetivos

- No se toca el algoritmo de camino (`TNGridLogic::GeneratePath`): camino único sin
  ramificaciones sobre un grid 6x6.
- No se decide qué puzzle cae en qué punto del camino. La integración con
  `ATN_ChunkManager` y `RunGameMode` lleva spec propio, después de la fase 3.
- No se añade streaming, LOD ni generación en GPU. Las 36 celdas se construyen en la carga.
- No se sustituye `UProceduralMeshComponent`.
- No hay máscara pintada a mano en v1: la máscara del stamp es paramétrica.
- No se toca el material triplanar (commit 78d957c).

## 3. Decisiones

| Decisión | Elegido | Descartado |
|---|---|---|
| Suelo caminable | Corredor 100 % procedural, tallado en runtime | El módulo dibuja el pasillo; máscara de excepciones |
| Unidad de autoría | Stamp con máscara de influencia, posición y rotación libres | Celda de 4000 uu con franja de blending; dos capas |
| Composición | Tres capas en orden fijo (relieve aditivo, corredor, asientos) | Media ponderada normalizada `ΣW·S / ΣW` |
| Puzzles | Puzzle = stamp de asiento que aplana el terreno | Celdas reservadas planas; el puzzle esculpe el terreno |
| Siembra | Reglas por stamp + muestreo de Poisson | Composiciones autoradas; dos capas |
| Replicación | El servidor siembra y replica la lista cuantizada | Solo semilla; semilla + checksum |
| Dónde viaja la lista | En el `FTNGridTileInit` de cada tile | En el generador |
| Autoría | Volumen de autoría con bake por trazas + import PNG | Proxy de Landscape; malla de Blender rasterizada |
| Código de editor | Módulo nuevo `TortunaboEditor` | `#if WITH_EDITOR` dentro del módulo runtime |

La media ponderada normalizada se descarta por dos defectos: en el borde de un stamp
aislado `W/W = 1`, lo que produce un escalón donde la máscara ya casi vale cero; y un
stamp de relieve que solape un puzzle rompe la planitud del asiento.

## 4. La primitiva

La altura en un punto `P` del plano se obtiene aplicando tres capas en orden fijo:

```
1. Relieve   H = Base(P) + Σ_k W_k(P) · Delta_k(P)
2. Corredor  H = Lerp(H, CorridorFloor(P), Wc(distancia a la Centerline))
3. Asientos  H = Lerp(H, Cota_j, Wp_j(P))          para cada asiento j
```

- **Base(P)**: meseta a `WallHeight` más una ondulación suave de baja frecuencia. La pared
  que impide ver las celdas vecinas sale de aquí.
- **Relieve**: cada stamp guarda un delta sobre la base, no una altura absoluta, y su
  máscara `W_k` cae a 0 en el borde. Todos los stamps empiezan y acaban a cota de base:
  no hay juntas que casar. La suma es conmutativa en el plano matemático; el orden de
  evaluación se fija por igualdad bit a bit (sección 6.3).
- **Corredor**: sin cambios respecto a hoy (`SampleCorridorFloor`, `InteriorLaneHalfWidth`).
  Al aplicarse después del relieve, ningún stamp puede bloquear el camino.
- **Asientos**: `Cota_j` es la altura del suelo del corredor en el ancla del puzzle. Donde
  `Wp_j = 1` la altura es exactamente `Cota_j`. Se aplican los últimos: el puzzle siempre gana.

### 4.1 Invariante de pared

Dentro de una distancia `CorridorHalfWidth + BankWidth` de la Centerline, la contribución
del relieve se acota a `>= 0`. Un delta negativo pegado al corredor abriría la vista a las
celdas vecinas o permitiría salir trepando.

### 4.2 Qué sale del código actual

- `SampleHighGround` y el `Fbm` como generador de forma macro se eliminan.
- `SampleCorridorFloor`, `BuildCenterline`, `AssignCellStyles`, `SampleColor` y
  `BuildGridTriangles` se conservan.
- `EvaluateTerrain` pasa a delegar en `TNStampField::SampleHeight`.

## 5. Piezas

Se sigue el patrón `*Decisions.h` del repo: lógica pura en cabeceras, testeable sin mundo.

| Unidad | Responsabilidad | Depende de |
|---|---|---|
| `UTNTerrainStamp` (UDataAsset) | Delta, máscara paramétrica, tamaño, reglas de siembra | — |
| `UTNTerrainStampLibrary` (UDataAsset) | Array de stamps; el índice es el Id de red | `UTNTerrainStamp` |
| `FTNPlacedStamp` | Stamp colocado, cuantizado, 7 bytes | — |
| `FTNPlacedSeat` | Asiento de puzzle: posición, yaw, footprint, halo | — |
| `TN_StampSeedingDecisions.h` | Reglas + Poisson → lista de stamps. Solo servidor | Librería, camino |
| `TN_StampFieldDecisions.h` | `SampleHeight(P, contexto)` con las tres capas | Librería, listas |
| `ATN_GridMapGenerator` | Siembra en servidor y reparte la lista a cada tile | Todo lo anterior |
| `ATN_GridTerrainTile` | Construye su malla desde su `FTNGridTileInit` | Campo |
| `ATN_StampAuthoringVolume` (módulo `TortunaboEditor`) | Bake, Load Stamp, preview | Campo, librería |
| `Scripts/import_stamp.py` | PNG 16-bit + json → `UTNTerrainStamp` | Librería |
| `Scripts/sketch_to_stamp.py` (uv + numpy) | Boceto → limpieza y erosión → PNG 16-bit | — |

### 5.1 `UTNTerrainStamp`

- Delta `uint16`, 129×129 muestras, con `HeightScale` y `HeightOffset` (admite negativos).
- `SizeUU`: lado del stamp en uu, entre 1000 y 6000.
- Máscara paramétrica: `Shape` (círculo o caja redondeada) y `FalloffWidth` (0-1).
- Reglas de siembra: `DistCaminoMin`, `DistCaminoMax`, `SeparacionMin`, `Densidad`
  (por celda) y `EstilosPermitidos` (máscara de bits sobre `ETNTerrainStyle`).
- Peso: ~33 KB por stamp; 80 stamps ≈ 2,6 MB.

### 5.2 Puerta única de creación

El bake del editor y el import PNG terminan en la misma función,
`BuildStampFromHeights(alturas, tamaño, máscara)`. Esa función fuerza delta 0 en el borde
de la máscara: es lo que garantiza el blend sin juntas. Si la entrada trae un borde alto,
lo atenúa y emite un aviso con la magnitud recortada.

### 5.3 Librería

`UTNTerrainStampLibrary` contiene el array de stamps. El Id que viaja por red es el índice
en ese array. Todas las máquinas ejecutan el mismo build, así que el índice es estable. El
bake añade el stamp nuevo al final; rehornear un stamp existente conserva su índice.

## 6. Siembra, replicación y late-join

### 6.1 Siembra (solo servidor)

Función pura de semilla + camino + librería + lista de asientos:

1. Los asientos entran como lista ya dada. En la demo se usan asientos de test.
2. Para el relieve: el número de candidatos sale de `Densidad × celdas`; el stamp se saca
   de una bolsa sin reemplazo con el `FRandomStream` de la semilla; se comprueban las
   reglas y la separación mínima contra lo ya colocado; un rechazo se reintenta hasta 8
   veces (`MaxPlacementTries`, ajustable) y después se descarta.
3. La salida se ordena por índice global de colocación. Ese orden es parte del contrato.

### 6.2 Formato replicado

`FTNPlacedStamp`: `StampId` `uint16`, `X` e `Y` `uint16` sobre la extensión del mapa
(resolución ≈ 0,37 uu para 24000 uu), `Yaw` `uint8` (pasos de ≈ 1,4°). Total: 7 bytes.

La lista viaja en el `FTNGridTileInit` de cada tile, que ya se replica con
`COND_InitialOnly`. Cada tile recibe solo los stamps y asientos cuyo radio toca su celda
ampliada en un paso de malla (margen que necesitan las normales por diferencias
centrales). Un stamp a caballo entre dos celdas viaja en las dos con los mismos enteros.
Estimación: 3-6 stamps por tile, ~40 bytes por tile, menos de 1 KB en total.

Llevar la lista en el tile y no en el generador evita una carrera entre dos actores
replicados: el tile tiene todo lo que necesita en su propio bunch inicial.

### 6.3 Igualdad entre máquinas

- El servidor también construye desde los valores cuantizados, nunca desde los float
  originales de la siembra.
- Cada tile suma sus stamps en orden de índice global. La suma en coma flotante depende
  del orden; con el mismo orden a ambos lados de una junta, el vértice compartido da el
  mismo resultado bit a bit. Se conserva la garantía que ya da `GridPoint`.

### 6.4 Late-join

`COND_InitialOnly` entrega la lista en el bunch inicial del tile, igual que hoy la semilla
y el camino. No requiere código nuevo.

## 7. Autoría

### 7.1 Volumen de autoría

`ATN_StampAuthoringVolume` es una caja colocada en un nivel de autoría. El diseñador mete
dentro cualquier geometría: mallas esculpidas con Modeling Tools, rocas en kitbash o un
Landscape. Tres acciones, todas `UFUNCTION(CallInEditor)` (botones en el panel de
detalles, sin UI Slate):

- **Bake to Stamp**: rejilla de 129×129 trazas hacia abajo con colisión compleja;
  `delta = Z del impacto − Z de la base del volumen`. Crea el asset o sobrescribe el
  asignado, y lo añade a la librería si es nuevo.
- **Load Stamp**: vuelca un stamp existente como malla dentro del volumen
  (GeometryScripting) para retocarlo y rehornearlo sobre el mismo asset.
- **Preview**: malla con `SampleHeight(Base + este stamp)`. Enseña la caída de la máscara,
  que es lo único que el diseñador no ve mientras esculpe.

Vive en un módulo nuevo `TortunaboEditor` para que el código de editor no entre en el
build de distribución.

### 7.2 Import PNG y pipeline offline

- `Scripts/import_stamp.py` (Python de editor): PNG 16-bit en grises + `.json` hermano con
  tamaño, máscara y reglas de siembra → `UTNTerrainStamp`, a través de
  `BuildStampFromHeights`.
- `Scripts/sketch_to_stamp.py` (uv + numpy, fuera del editor): boceto o imagen → limpieza,
  reescalado, erosión hidráulica a 385×385 submuestreada a 129 → PNG 16-bit. Hereda la
  erosión del diseño anterior; desaparece el forzado de bordes al perfil canónico.

Flujo previsto: se genera una tanda de stamps desde bocetos y los diseñadores la retocan
con Load Stamp.

## 8. Fallos, coste y tests

### 8.1 Fallos

| Situación | Comportamiento |
|---|---|
| `StampId` fuera de la librería | Se ignora ese stamp; log `Error` con Id y tile |
| Librería vacía o sin asignar | Base + corredor; el nivel sigue siendo jugable; log `Warning` |
| La siembra no coloca todos los candidatos | Menos stamps; log `Verbose` con el recuento |
| Entrada al bake con borde alto | Se atenúa a 0 y se avisa de la magnitud recortada |
| Traza del bake sin impacto | Delta 0 en esa muestra |

Ningún fallo bloquea la partida: el corredor es procedural y no depende de los stamps.

### 8.2 Coste

36 celdas × 97² ≈ 339k vértices; con los stamps repartidos por tile cada vértice consulta
~3. Estimación: ~0,2 s una vez en la carga, normales incluidas. Es una estimación sin
medir; la fase 1 incluye un test de tiempo que la confirma o la corrige.

### 8.3 Tests (puros, sin mundo)

- Misma semilla → misma lista de stamps.
- Se respeta `SeparacionMin` entre todos los pares colocados.
- Se respetan `DistCaminoMin` y `DistCaminoMax`.
- Ida y vuelta de la cuantización con error ≤ 0,37 uu.
- Vértice de borde idéntico bit a bit en dos tiles con un stamp a caballo.
- Un stamp aislado vale exactamente `Base` en el borde de su máscara.
- Asiento exactamente plano donde `Wp = 1`, también con un stamp de relieve solapado.
- Invariante de pared: ningún punto del banco queda por debajo de `Base`.
- `BuildStampFromHeights` deja delta 0 en el borde con una entrada de borde alto.
- Tiempo de construcción de las 36 celdas.

## 9. Relación con el diseño anterior

De `2026-09-21-Terreno-Modular-Design.md` se conservan el material triplanar (ya
implementado), la erosión offline con uv + numpy, la bolsa sin reemplazo con el
`RandomStream` de la semilla y el principio de replicar el resultado y no la receta.

Quedan sin efecto: la celda como módulo (`UTNTerrainModule`), el perfil de borde canónico
por familia, las orientaciones ×4/×2 con espejo, el test de winding y la validación de
transitabilidad del módulo.

## 10. Fases

Cada fase deja algo visible en `LVL_ProcGenDemo`.

1. **Campo.** `UTNTerrainStamp`, `UTNTerrainStampLibrary`, `TN_StampFieldDecisions.h`,
   tests, import PNG mínimo, 3 stamps de prueba con lista puesta a mano, test de tiempo.
2. **Siembra y red.** `TN_StampSeedingDecisions.h`, lista en `FTNGridTileInit`, tests,
   comprobación en PIE 2P.
3. **Asientos.** Capa 3, `FTNPlacedSeat`, un asiento de test en la demo.
4. **Autoría en editor.** Módulo `TortunaboEditor`, volumen, Bake, Load Stamp, preview.
5. **Pipeline offline.** `sketch_to_stamp.py` y primera tanda de librería.

# E — Carrera: pulido del reparto y modelo híbrido autor + procedural

Base: `main` 41a0ee8a5, solo lectura. `L.h` = `Public/World/Beach/TN_BeachLayout.h`.

Corrección al encargo: el relieve **no** depende de la semilla. `TerrainSeed` es `constexpr` (`L.h:68`) y solo cambia el reparto. Por eso un tramo de autor puede anclarse a una X absoluta sobre el relieve real.

## 1. Cómo reparte hoy `GenerateRound`

- **Pasadas** (`L.h:4283-4318`, un solo `FBuilder` con un único `Rng`, `L.h:2458`): primero las estructuras (castillos, fortalezas, quads, gaviotas, trincheras), luego filas, enemigos con pasada propia, lo militar, trampas y ayudas destacadas, lanzadores, cofres, el relleno por bandas de 50 m, `EnsureCatapults`, los tapones y el interés. Salen unos 3100 elementos, el 80 % decorado local en HISM.
- **Reglas** (`TryAdd`, `L.h:2582`):
  - Comprobaciones en orden: `InBounds`, `Fits` (núcleos más `ItemPad` de 1,5 m; 45 cm entre piezas de basura), `TerrainAllows` y `SeatIsGentle`.
  - Lo que bloquea tiene que dejar paso en ±60 m (`FPassGrid`).
  - Los lanzadores reservan su arco (`L.h:2670`).
- **Densidad y dificultad**:
  - Cupos por banda de `(base + mar·t) × multiplicador` (`L.h:4003-4005`).
  - Decorado hasta `BandCoverage` (`L.h:2292`).
  - Enemigos apiñados en Difícil (`L.h:2491`).
- **Anti-repetición**:
  - En la ronda: solo `MaxPerRound` y los pesos.
  - Entre rondas: ninguna. La semilla sale del reloj (`Private/Game/TN_BeachRaceGameMode.cpp:493-505`).
- **Equidad**: los huevos quedan a ±15 m con 15 m libres delante. El primer tramo no se hace simétrico.
- **Pruebas** (`Private/Tests/TN_BeachLayoutTest.cpp:586-850`): paso, solapes, terreno, arcos, cupos, ocupación, rectas y determinismo.

### Defectos

| # | Defecto | Evidencia | Efecto |
|---|---|---|---|
| D1 | **El sprint destruye el castillo principal.** El castillo va al 42-58 % (≈332-453 m, Y ±39 m, radio 40 m) y la línea del sprint está hacia los 400 m. `ClearElementsAround` borra lo que toca su círculo (~32 m). | `L.h:2846`, `L.h:1171`, `TN_BeachRaceGenerator.cpp:527-533`, `TN_BeachRaceGameMode.h:304` | Los centros quedan a menos de 73 m, así que cae casi siempre. Quedan alas hacia la nada, un asiento liso de 40 m y las conchas de la cima flotando a ~11,5 m (solo se limpian los rebuscables, `TN_BeachLoot.cpp:301`; `TN_BeachLootShells.cpp:653`). Cálculo geométrico, **sin verificar en PIE**. |
| D2 | `Remove` no deshace `Reserved` ni `Interest`. | `L.h:2611-2619` frente a `L.h:2519` y `L.h:2650` | Arcos fantasma (huecos vacíos) y botín de cosas que ya no están. |
| D3 | El trampolín puesto ante una plataforma o un castillo enorme sobrevive si `RestorePassage(BandStart)` quita su objetivo. | `L.h:3968-3972`, `L.h:4016` | Lanzadores huérfanos. |
| D4 | `EnsureCatapults` cambia un trampolín (arco de 22 m) por una catapulta (30 m) con `TryAdd`, sin validar el arco y sin actualizar `Interest.To`. El test excluye el rol `Launcher`. | `L.h:3579-3588`, `TN_BeachLayoutTest.cpp:715` | Catapultas que lanzan contra el decorado. |
| D5 | Filas fijas al 11/35/62/78/88,5 % ±2 %, lado del hueco cíclico (`(Pattern+r)%3`) y tema que puede repetirse seguido. | `L.h:3131`, `L.h:3142-3143` | Ritmo aprendible: tres barreras a todo lo ancho entre el 35 y el 62 % (con el castillo) y 180 m sin ninguna entre el 11 y el 35 %. |
| D6 | La rampa de densidad está invertida: el objetivo del 66-76 % nunca se alcanza. | `L.h:2292`, `Docs/Modo_Carrera.md:347` | El primer tercio queda al 52 % y la media al 48 %: la playa se vacía hacia el mar. |
| D7 | La ocupación se mide por `Pos.X`: lo alargado cuenta entero en una sola banda. | `L.h:3836`, `L.h:4251` | Vacíos y saturación mal medidos. |
| D8 | Unas 2700 piezas de basura a 45 cm, y las trampas llevan el mismo `ItemPad` que todo. | `L.h:161`, `L.h:4009` | Poca legibilidad del peligro. |
| D9 | Un único RNG en cadena. | `L.h:2458` | Cualquier ajuste o tramo insertado cambia toda la ronda. |
| D10 | Latente: las ranuras de las trampas destacadas ignoran `MaxT`. | `L.h:3537` | Hoy no se nota. |

## 2. Pulido priorizado

| P | Mejora | h |
|---|---|---|
| P1 | `GenerateRound(..., bSprint)` en `RoundNet`: reserva el nido y aparta el castillo de [SprintX − 80, SprintX + 30] m. Se retira `ClearElementsAround`. | 5-7 |
| P2 | `OwnerItem` en `Reserved` e `Interest`: el lanzador cae con su objetivo y `Finish` filtra los huérfanos. | 6-8 |
| P3 | `EnsureCatapults` con `TryAddWithArc`, `To` actualizado y test ampliado. | 2-3 |
| P4 | Curva de intensidad por banda: filas a 60-110 m sin posiciones fijas, sin repetir tema ni lado, y un respiro cada ~150 m. | 10-14 |
| P5 | Presupuesto de decorado por banda que se pueda cumplir y crezca hacia el mar. | 4-6 |
| P6 | Halo de 2,5-3 m sin basura alrededor de trampas y lanzadores; 6 m limpios delante de las filas. | 6-8 |
| P7 | Ayudas de la primera banda en el centro o en espejo, con test de equidad. | 3-4 |
| P8 | Métricas en el test: disco libre mayor, repeticiones, huérfanos y ocupación por cápsula. | 6-8 |
| P9 | Subflujos de RNG por pasada. Rompe una vez las semillas antiguas. | 4-6 |

## 3. Diseño híbrido

### Formato

`UTN_BeachStretchAsset : UPrimaryDataAsset`, un fichero por tramo (`.uasset` es binario y Alvaro2rh trabaja en `main`).

- `Id`, `Version` y `ContentHash`.
- Largo en X. Anclaje `Pinned` (X absoluta, posible porque el relieve es fijo) o `Floating` (rango en T). `bAllowMirrorY`.
- `Items`: `Element`, `LocalPos`, `Yaw`, `SizeScale`, `Extent`, `SpecSeed`, `Flags` y máscara de dificultad. Es el vocabulario `ETNBeachElement` actual, así que no necesita ruta nueva de red ni de render.
- `Reserve` (cápsulas en las que el relleno no entra) y `FillPolicy` (`None`, `DecorOnly`, `Full`).
- Asiento por pieza (`MakeStamp`, `L.h:1812`) o `bFlattenPad` (un sello único).
- `Interest` de autor para el botín.

`UTN_BeachRaceCourseAsset`:

- `Slots`: tramo o hueco, X o rango, `bRequired`, `Chance` y espejo.
- Máscara de pasadas procedurales y densidad por hueco.
- Enfriamiento entre rondas.
- **Carrera fija**: todos los slots de autor y la máscara casi vacía.
- **Huecos procedurales**: slots sin tramo, con tema y densidad propios.

### Inserción

1. El asset pasa a un POD en el hilo de juego, porque el reparto corre sin UObjects (`TN_BeachRaceGenerator_Round.cpp:210/247`).
2. `PlaceAuthored()` va **antes** de `PlaceMainDungeon`, con su propio subflujo de RNG. Resuelve la X de los tramos `Floating` probando candidatos contra `TerrainAllows` y apunta `ReservedIntervals`.
3. `AddAuthored` marca la rejilla, la ocupación y las cubetas, con `Role = Authored`. `RestorePassage` lo salta: solo se quita lo procedural.
4. Las pasadas con ancla fija (castillo principal, filas, colosales) consultan `IsXReserved` y se desplazan o se omiten. Las colosales solo caben en 15-120 y 280-440 m (`Modo_Carrera.md:245`).
5. El relieve no se toca. Si un tramo no cabe: el opcional se omite con aviso y el `bRequired` usa su X de respaldo.
6. **Con el curso vacío, el reparto sale idéntico bit a bit**. Lo comprueba un test dorado.

### Replicación

`FTNBeachRoundNet` (`TN_BeachRaceGenerator.h:30-61`) gana `CourseId`, `CourseHash` y un array de colocaciones (índice, X cuantizada y espejo; pocos bytes). El servidor elige los tramos con el historial de la partida y los clientes reciben la colocación ya resuelta. Si el hash no coincide, se registra y se avisa en `UTN_BeachRoundSyncComponent`.

- El decorado de autor va a `ATN_BeachDecorField` y los elementos a `SpawnElement`, sin cambios.
- **Fase 2**: mallas propias vía `BatchFor`, que ya recibe `UStaticMesh*` (`TN_BeachDecorField.h:253`), y BP de juego propios creados en el servidor con la regla de dormancy.

### Herramienta de editor

- Nivel `LVL_BeachStretch_Workbench` con `PreviewRound`.
- `ATN_BeachStretchProxy`, solo de editor: `Element`, `SizeScale`, `Extent` y `Seed` editables, con la malla del mismo `TNBeachDecorKit`. Hoy `Spec` no se puede editar (`TN_BeachElement.h:99`).
- `ATN_BeachStretchVolume` (caja, ancla y espejo) con botones `CallInEditor`: *Export*, *Import* y *Validate*. Validar comprueba límites, terreno, asiento, arcos, el paso del tramo aislado y dibuja las huellas.
- Un test de automatización sobre todos los tramos, ejecutable sin abrir el editor.

## 4. Riesgos y tareas

- **Desincronización** (datos o parche distintos): hash, POD y colocación replicada.
- **Pasadas fijas sin sitio**: los tests exigen de 1 a 3 castillos; hay que parametrizarlos por curso.
- **Paso libre**: lo de autor que bloquea no se puede quitar. Si el tramo solo, sin nada alrededor, ya cierra el paso, la exportación lo rechaza.
- **Vista previa distinta del juego** si el proxy no usa el mismo kit.
- **P9 frente al test dorado**: primero el híbrido con salida idéntica, y P9 después en un commit aparte.
- **Rendimiento**: hay margen (unos 20 ms de 150).

| # | Tarea | h |
|---|---|---|
| T0 | Test dorado (24 semillas × 3 dificultades) | 2 |
| T1 | Correcciones P3, P2 y P1 | 13-18 |
| T2 | Assets de tramo y de curso, y POD | 6-8 |
| T3 | Generador: `PlaceAuthored`, intervalos, rol `Authored`, pasadas fijas y máscara | 12-16 |
| T4 | Red, selección con enfriamiento y hash | 6-8 |
| T5 | Editor: proxy, volumen, exportar, importar y validar | 16-22 |
| T6 | Tests de tramos, carrera fija y sprint | 6-8 |
| T7 | Pulido P4-P8 | 29-40 |
| T8 | Fase 2: mallas y BP propios | 14-20 |
| T9 | P9 y nuevo dorado | 4-6 |

Total: 108-148 h. Mínimo útil (T0-T6): 61-82 h.

# Traspaso: probar y pulir el mapa procedural (sesión local)

Contexto para una sesión de Claude Code que corre **en el PC del equipo, con el editor
abierto**, y continúa el trabajo de una sesión en la nube que no podía compilar ni
ejecutar Unreal. Rama `claude/elegant-fermi-6n1hxy`, PR
[Unreal-portfolio/Tortunabo#8](https://github.com/Unreal-portfolio/Tortunabo/pull/8).
Lee también [`Mapa_Procedural.md`](Mapa_Procedural.md) (arquitectura y parámetros).

**Tu trabajo:** probarlo todo en el editor, valorar lo que no funcione o no se vea bien y
cambiarlo. Compila, juega (PIE), mira el Output Log y arregla. Haz commits pequeños y
coherentes en la misma rama y súbelos.

---

## 1. Estado al traspasar

- Compila en el PC del usuario (UE 5.6). La capa UE se escribió sin poder compilarla;
  la lógica pura (`TNProcMap`, solo cabeceras) sí está probada.
- El script `Scripts/build_procmap_assets.py` ya se ejecutó: existen `/Game/ProcMap`
  (materiales, `DA_ProcMapSettings`, 8 `DA_Biome_*`, `BP_ProcMapGameMode`),
  `/Game/Maps/Run/LVL_ProcMap` y dos `TN_ProcModeSelector` en `LVL_HQ`.
- **Bug visto por el usuario:** el suelo se veía invertido y desde arriba no se veía
  nada. Causa: los triángulos se emitían con la cara frontal al revés (en UE la cara
  frontal de (A,B,C) es la de normal (C−A)×(B−A)). Corregido en
  `TN_ProcMapGenerator_Build.cpp` (terreno y `FTNProcMeshBuffers::AddTri`). **Lo primero:
  recompilar y comprobar** que terreno, puentes, cuevas, isletas, pasarelas, lava y
  tobogán se ven por arriba y la luz es correcta.

## 2. Lo que el usuario pidió (decisiones cerradas)

- Rejilla de módulos **irregulares** de **400 m**, siempre conexos. Tamaño por modo ×
  dificultad; Coop 3×3 / 6×6 / 8×8 (por defecto 6×6). Convive con `ATN_ChunkManager`.
- Coop: 10–20 min ideal, pero por defecto el camino cubre ~25–30 de 36 módulos (sale
  ~35–40 min). El usuario lo quiere así para probar; todo configurable.
- Módulos vacíos: Elevated / BranchesAndScenery / Explorable / Mixed (configurable).
- Biomas por **regiones de varios módulos** (tipo Minecraft), al azar, bien
  distinguibles y con transición natural: selva, playa, desierto, volcánico, agua
  (isletas), acantilados, manglar, zona humana. Último módulo: playa con mar abierto.
  Terreno ondulado sin tendencia general. Río opcional.
- Camino de ancho variable 4–35 m con pasos estrechos. Ramas configurables que no se
  alejan más de 1–3 módulos y vuelven al camino (el equipo suele ir junto).
- Cruces **colosales** tipo Mario Kart (puentes y cuevas) sobre/bajo un módulo ya
  recorrido, a los que se llega por conexiones especiales automáticas de un sentido
  (géiser que sube, cascada-tobogán que baja). Caerse de ellos mata. Puentes dentro de
  un módulo: normales.
- Métricas de salto: 2 m corriendo, 4 m con dive, ~1,5 m de altura. Huecos del camino
  principal 1,3–3,9 m. Cambios grandes de altura solo entre módulos.
- Agua: nado básico (entre andar y correr) y agua peligrosa (criaturas tipo medusa por
  bioma, tiburón/morena, corrientes, remolinos). La idea es quedarse en el camino.
- El generador coloca todo el gameplay. La tormenta sigue el camino.
- Generación en runtime por semilla replicada; PCG sí (gancho); greybox + slots.
- Modos: **Coop** (mapa largo, 1 ronda = partida por defecto), **Carrera** (todos
  contra todos, primero en meta gana la ronda, gana quien llegue a 3), **2vs2** (exige
  4, parejas que rotan cada ronda, gana la ronda la pareja que llega ENTERA, victorias
  por jugador, a 3; camino principal que a veces se separa en carriles con puzles de
  lanzamiento y botones de sabotaje).
- Reaparición en **pilas de huevos** (como las del centro del lobby) en los cruces de
  módulo; densidad según dificultad.
- Coger y lanzar (issue #6 fase 2): solo tortugas en caparazón o aturdidas, cualquiera;
  si la llevada se mueve 2 s seguidos se libera; mientras forcejea tiembla la cámara
  del portador y lanza mucho menos; la lanzada no sale del caparazón en el aire hasta
  un rebote vertical al caer (se estira durante el rebote y cae de pie). Caída de más
  de 5 m (no géiser/tobogán) → caparazón; más de 30–40 m → muere.
- Selección de modo con interactuables del lobby; nivel nuevo `LVL_ProcMap`; *Clásico*
  sigue yendo a `LVL_Run`.
- Objetivo: PC medio a 60 fps.

## 3. Plan de pruebas (en este orden)

1. **Visual del mapa en el editor**: en `LVL_ProcMap`, selecciona `ProcMapGenerator` →
   *ProcMap|Editor* (semilla, modo, dificultad; `bDebugDraw` para ver camino y
   módulos) → *Generate In Editor*. Revisa: biomas por regiones y transiciones, camino
   legible y continuo, bordes altos, costa y mar al final, cruces colosales, isletas.
   Pulsa *Clear* antes de jugar.
2. **Play 1 jugador** (Coop Normal por defecto; para iterar rápido pon *Difficulty
   Without Lobby = Easy* en `BP_ProcMapGameMode`, que da 3×3). En el log:
   `[ProcMap] Mapa listo · semilla …` (tiempos por fase, minutos estimados) y
   `[ProcMapGameMode] ═══ Ronda 1 en marcha`. Comprueba:
   - aparición en la salida, colisión del terreno, que no se cae al vacío;
   - saltos de los huecos (1,3–3,9 m), géiser y tobogán (sin caparazón ni muerte);
   - caerse de un puente colosal mata; caída de >5 m mete en el caparazón;
   - pilas de huevos: al pisarlas se activan; al morir se reaparece en la última;
   - tormenta (Coop): avanza por el camino tras la gracia; mata a los rezagados;
   - agua: nadar, flotar con medio cuerpo fuera, salto desde el agua, corrientes,
     remolinos, depredador, criaturas-medusa; salir a la orilla;
   - meta → resultados → vuelta a `LVL_HQ`.
3. **Rendimiento**: `stat fps`, `stat unit`, tiempo de generación del log. Si el 6×6
   tarda o cae de 60 fps, ajusta (`VertexSpacing`, `TileQuads`, densidad de
   vegetación en los `DA_Biome_*`, `CullDistance`).
4. **Coger y lanzar** con 2 jugadores (PIE, Listen Server): meterse en el caparazón,
   coger, forcejear 2 s, lanzar, rebote al caer.
5. **Carrera** (`Mode Without Lobby = Race`): rondas, mapa nuevo por ronda, victoria a
   3, tabla final.
6. **2vs2** con 4 jugadores: parejas que rotan, muro de lanzamiento, compuertas de
   sabotaje, gana la pareja que llega entera.
7. **Lobby**: `LVL_Menu` → `LVL_HQ`, los dos pedestales cambian modo y dificultad y el
   viaje va a `LVL_ProcMap` (o `LVL_Run` en Clásico).
8. **Tests de automatización**: *Session Frontend → Automation → Tortunabo.ProcMap*.

Semilla fija para reproducir un mapa: `Fixed Seed` en `BP_ProcMapGameMode`. La semilla
de cada mapa sale en el log.

## 4. Dónde está cada cosa

| Tema | Archivo |
|---|---|
| Decisiones de layout (módulos, ruta, camino, alturas, biomas, features, terreno) | `Public/World/ProcMap/TN_ProcMap{Math,Layout,Modules,Route,Path,Features,Terrain,Generate}.h` |
| Perfiles por modo × dificultad, DataAssets, greybox por defecto | `TN_ProcMapTypes.h/.cpp` |
| Construcción en el mundo | `TN_ProcMapGenerator.cpp` (red, consultas), `_Build.cpp` (terreno, agua, estructuras), `_Spawn.cpp` (vegetación, actores, peligros, PCG) |
| Géiser, tobogán, zonas de muerte, meta | `TN_ProcTraversalActors` |
| Agua y fauna | `TN_ProcWaterActors` |
| Huevos, puzles 2vs2, tormenta | `TN_ProcEggNest`, `TN_ProcPuzzleActors`, `TN_PathStorm` |
| Rondas y modos | `Game/TN_ProcMapGameMode`, `Game/TN_ProcMapGameState` |
| Coger/lanzar, nado, caídas | `Player/TN_CarryComponent`, `Player/TortugaCharacter.cpp` (`Landed`, `OnMovementModeChanged`, `TickFallRules`, `TickShellVisual`, `Jump`) |
| Lobby | `Lobby/TN_ProcModeSelector`, `Lobby/TN_HQGameMode.cpp` (`BeginMatchTravel`) |

Logs útiles: `[ProcMap]`, `[ProcMapGameMode]`, `[PathStorm]`, `[Fall]`.

## 5. Riesgos conocidos para vigilar

- Colisión del terreno con cocinado asíncrono: la ronda espera `MinPreRoundSeconds`
  (2 s) antes de soltar a los jugadores. Si alguien cae al vacío al empezar, súbelo.
- Tamaño/tiempo de generación del 6×6 y 8×8 (terreno de ~1–2 M vértices en tiles).
- Nado: el volumen de agua mide por el centro de la cápsula; comprobar que no bota.
- Red: solo se replica la semilla; los clientes congelan su pawn hasta tener el mapa y
  avisan con `ServerReportProcMapReady`. Probar con 2–4 clientes.
- El HUD no muestra todavía `RoundResultText` ni las victorias por ronda.

## 6. Herramientas

- MCP de Unreal para manejar el editor desde Claude Code: plugin `UnrealMCPython` y
  `Scripts\setup-unreal-mcp.ps1 -WithClaude` (ver README, sección MCP).
- Tests de la lógica pura: también compilan fuera del motor; ver
  `Private/Tests/TN_ProcMapDecisionsTest.cpp`.
- Convenciones: mensajes de commit y documentación en español; el código sigue el
  estilo de alrededor (prefijo `TN`, helpers únicos por archivo por el unity build).

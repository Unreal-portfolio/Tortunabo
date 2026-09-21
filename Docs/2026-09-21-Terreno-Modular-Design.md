# Terreno modular — diseño

Fecha: 2026-09-21
Estado: diseño aprobado en brainstorming, pendiente de plan de implementación
Rama de partida: `feat/procgen-terrain` @ e079522

## 1. Problema

El terreno del grid procedural se genera hoy evaluando ruido Perlin en runtime
(`TN_GridTerrainDecisions.h`). Eso produce tres defectos que bloquean el uso del sistema
por parte de diseñadores:

1. **La textura se estira en las pendientes.** Las UV son una proyección planar XY pura
   (`TN_GridTerrainDecisions.h:529`, `UVs = P / CellSize`). En una pared de 800 uu de alto
   con un talud de 380 uu de ancho el texel se estira un factor `1/cos(atan(800/380))` ≈ 2,3.
2. **El relieve no se lee como natural.** `Fbm` (`:235`) suma tres octavas de
   `FMath::PerlinNoise2D`. El resultado es un campo isótropo de grumos: sin cauces, sin
   coherencia de ladera, sin taludes en ángulo de reposo.
3. **El diseñador no puede intervenir.** La única palanca son ~25 escalares de
   `FTNGridTerrainSettings`. No hay forma de decir "aquí quiero esta forma".

## 2. No-objetivos

- No se toca el algoritmo de camino (`TNGridLogic::GeneratePath`). Sigue siendo un camino
  único sin ramificaciones sobre un grid 6x6, de 10 a 20 celdas.
- No se implementa curva de dificultad ni integración con `ATN_ChunkManager` / `RunGameMode`.
- No se añade streaming ni LOD de terreno. La celda entera se construye de una vez, como hoy.
- No se sustituye `UProceduralMeshComponent` por Nanite ni por StaticMesh horneado.

## 3. Decisiones

| Decisión | Elegido | Descartado |
|---|---|---|
| Unidad de módulo | Celda completa de 4000 uu con 4 bordes tipados | Sub-tiles componibles; módulos multi-celda |
| Origen del módulo | Boceto cenital en escala de grises | Foto real de paisaje (profundidad monocular); heightmap externo como único camino |
| Tamaño de librería | 60-80 módulos de forma | 600 módulos |
| Forma del relieve | Erosión hidráulica offline | Ruido con domain warp + terrazas; escultura manual como único camino |
| Costura | Perfil de borde canónico por familia | Interpolación en franja |
| Rol del ruido | Detalle fino y decorado sembrado | Ruido como generador de forma macro |

### 3.1 Por qué 60-80 y no 600

La variedad percibida crece con la raíz del tamaño de la librería, no linealmente.
Cálculo sobre un camino medio de 15 celdas (~9 rectas, ~6 giros — reparto estimado, no
medido) con extracción **con** reemplazo:

| Librería | P(repetición en 1 partida) | 1ª repetición percibida | Recorrer la librería entera |
|---|---|---|---|
| 300+300, sin orientaciones | 15,7 % | partida 2,4 | 209 partidas |
| 300+300, con espejo e inversión (x4) | 5,3 % | partida 4,8 | 836 partidas |
| 75+75, con espejo e inversión (x4) | 15,7 % | partida 2,4 | 52 partidas |

75 módulos con orientaciones igualan a 300 sin ellas. Cuadruplicar la librería solo duplica
el tiempo hasta la primera repetición. El coste de 600 módulos a 15-30 min cada uno es de
150 a 300 h de modelado, incompatible con el calendario de publicación en Steam.

La variedad percibida es multiplicativa por capas: forma x orientación x decorado sembrado
x paleta de bioma. El jugador reconoce siluetas, no identidades de módulo.

### 3.2 La bolsa sin reemplazo elimina la repetición intra-partida

La tabla anterior asume extracción con reemplazo. Con una bolsa barajada sin reemplazo
dentro de la partida, la probabilidad de repetición dentro de una misma partida es 0
mientras la librería supere la longitud máxima del camino (80 > 20). Solo queda la
repetición entre partidas.

## 4. Arquitectura

### 4.1 Formato del módulo

`UTNTerrainModule : UDataAsset`

- `TArray<float> Heights` — `VertsPerSide * VertsPerSide` = 97 x 97 = 9409 valores, en uu
  relativas al suelo del pasillo. A 4000 uu de celda el paso de rejilla es 41,67 uu.
- `ETNModuleShape Shape` — `Straight` | `Turn`, alineado con `TNGridLogic::ECellShape`.
- `ETNEdgeFamily Edges[4]` — familia de perfil por borde (N/E/S/O): `None` (pared maciza),
  `Narrow`, `Medium`, `Wide`.
- `FName Biome`, `TArray<FName> Tags` — filtrado y selección.
- `TSoftObjectPtr<UTexture2D> Preview` — miniatura para el catálogo del diseñador.

No se guarda StaticMesh: el heightmap alimenta `TNGridTerrain::BuildTileMesh`
(`TN_GridTerrainDecisions.h:483`) sin cambios en la generación de colisión.

### 4.2 Costura: perfil de borde canónico

Cada familia de borde define un perfil de alturas fijo de 97 valores. Dos módulos encajan
si y solo si la familia del borde compartido coincide. La junta casa exacta por
construcción: no hay blending, no hay franja aplanada cada 4000 uu.

Invariante verificable: `Modulo[c].Edges[Este] == Modulo[c+1].Edges[Oeste]`, y los 97
valores de altura del borde coinciden con el perfil canónico de esa familia con tolerancia 0.

### 4.3 Selección en runtime

Para cada celda del camino:

1. Filtrar la librería por `Shape` + familia compatible con el vecino ya colocado + bioma.
2. Extraer de una bolsa barajada con el `FRandomStream` de la semilla, sin reemplazo.
3. Elegir orientación entre las válidas para esa forma.

Orientaciones: una recta admite espejo transversal e inversión longitudinal (4 variantes);
un giro admite inversión longitudinal (2 — su espejo coincide con otra rotación, no aporta).

Trampa: el espejo invierte el winding de los triángulos. Hay que invertir el orden de
índices o recalcular normales, o las caras quedan orientadas hacia dentro.

### 4.4 Replicación

`FTNGridTileInit` gana, por celda del camino, un índice de módulo (`int16`) y flags de
orientación (`uint8`): 20 celdas x 3 bytes = 60 bytes adicionales, una sola vez
(`COND_InitialOnly`).

Beneficio colateral: hoy cada máquina recalcula el ruido y la igualdad servidor/cliente
depende de que `FMath::PerlinNoise2D` sobre `double` dé resultados idénticos en todas las
plataformas. Con módulos, la forma viaja por la red y esa divergencia latente desaparece.

### 4.5 Material: proyección triplanar

Tres muestreos de textura en los planos XY, XZ e YZ, mezclados por los componentes de la
normal del mundo elevados a 4 y normalizados. Deja el tamaño de texel constante en
cualquier pendiente y hace innecesarias las UV de la malla (`TN_GridTerrainDecisions.h:529`
se elimina).

El resultado se **multiplica** sobre el color de vértice, que sigue siendo el que produce
`TNGridTerrain::SampleColor` (estratos, arena mojada, moteado de basura). Coste: 3
muestreos por píxel en lugar de 1.

### 4.6 Qué pasa con el ruido

Sale de la ruta de forma macro: `SampleHighGround`, `SampleCorridorFloor` y el uso de `Fbm`
para relieve dejan de decidir la altura. Permanece en:

- el detalle fino de superficie (`FloorRippleAmplitude`),
- el tinte y el moteado de `SampleColor`,
- la colocación sembrada de basura (`TN_GridJunkDecisions.h`), que es la capa que impide
  reconocer un módulo repetido.

## 5. Pipeline offline

Ejecutado fuera del editor con `uv run`, en Python con numpy. Un horneado de 80 módulos es
un proceso por lotes; iterarlo sin abrir Unreal es lo que hace viable ajustar la erosión.

```
boceto.png  ->  [uv run] normalizar -> forzar bordes -> erosionar -> validar  ->  modulo.npy
modulo.npy  ->  [editor, Python]  ->  UTNTerrainModule
```

### 5.1 Lectura del boceto

Imagen cuadrada en escala de grises: el gris es altura normalizada. Se reescala a la
resolución de trabajo y se mapea a `[0, WallHeight + DuneAmplitude]`.

Los bordes se **sobrescriben** con el perfil canónico de la familia declarada para ese
módulo. El dibujo del diseñador manda en el interior; la costura manda en el borde.

### 5.2 Erosión

Se trabaja a 4x de resolución (385 x 385) y se submuestrea a 97 al guardar. A 41,67 uu por
vértice un cauce no cabe en la rejilla final: erosionar directamente a 97 produce aliasing
en lugar de surco.

- **Hidráulica por partículas**: gotas que siguen el gradiente descendente acumulando
  velocidad y sedimento, erosionando cuando la capacidad supera la carga y depositando
  cuando sobra. Parámetros: inercia, capacidad, tasas de erosión y deposición, evaporación,
  radio de deposición.
- **Térmica**: relajación iterativa de las pendientes que superan el ángulo de reposo
  (~35 grados), que produce taludes de derrubios al pie de las paredes.

### 5.3 Validación de jugabilidad

Antes de guardar, el módulo debe cumplir:

- La pendiente del suelo dentro del carril central (`InteriorLaneHalfWidth`) no supera el
  ángulo caminable. El proyecto no sobrescribe `WalkableFloorAngle`, así que rige el valor
  por defecto de Unreal, 44,765 grados.
- Existe un camino transitable de borde de entrada a borde de salida.
- Ninguna cota del interior del pasillo alcanza `WallHeight / 2`, para que subirse a un
  afloramiento no sirva de escalón hacia la meseta.

Un módulo que falle la validación se rechaza con el motivo concreto; no se corrige en
silencio.

## 6. Testing

Siguiendo el patrón de `TN_GridTerrainDecisionsTest.cpp` y `TN_GridPathDecisionsTest.cpp`
(lógica pura, sin mundo):

- Costura: para todo par de módulos con familias compatibles, los 97 valores del borde
  compartido coinciden exactamente.
- Espejo e inversión: el winding se mantiene consistente y las normales apuntan hacia
  arriba en todo el suelo del pasillo.
- Bolsa: en 1000 caminos generados con semillas distintas, ninguna partida repite módulo.
- Determinismo: la misma semilla produce la misma secuencia de (índice, orientación).
- Validación: un módulo con una pendiente de 60 grados en el carril central es rechazado.
- Python: la erosión es determinista a semilla fija; los bordes tras erosionar siguen
  siendo idénticos al perfil canónico.

## 7. Orden de implementación (fail-fast)

1. **Triplanar.** Independiente del resto y verificable a ojo en `LVL_ProcGenDemo` sobre el
   terreno actual. Si el estiramiento no desaparece aquí, no desaparece con módulos.
2. **Perfiles de borde canónicos + `UTNTerrainModule`** con 3 módulos escritos a mano
   (una recta, un giro, una pared maciza). Valida la costura antes de invertir en pipeline.
3. **Selección en runtime + orientaciones + replicación**, con esos 3 módulos.
4. **Pipeline Python**: lectura de boceto, forzado de bordes, volcado. Sin erosión todavía.
5. **Erosión** hidráulica y térmica, y validación de jugabilidad.
6. **Catálogo**: 60-80 módulos.

Los pasos 1 a 3 dejan el sistema funcionando end-to-end con 3 módulos. Si algo estructural
falla, falla ahí y no después de dibujar 80 bocetos.

## 8. Riesgos

| Riesgo | Mitigación |
|---|---|
| Todas las juntas se ven iguales (coste del perfil canónico) | Varias familias de borde; decorado sembrado que cruza la junta |
| 3 muestreos de textura por píxel en un mapa de 240 m de lado | Medir en el playtest; el terreno no es el mayor coste, las 5751 instancias de basura sí |
| La erosión rompe la transitabilidad del pasillo | La validación de 5.3 rechaza el módulo antes de guardarlo |
| 60-80 bocetos siguen siendo mucho trabajo manual | El pipeline acepta también heightmaps de Gaea o World Machine, que ya traen erosión |
| El submuestreo 385 -> 97 se come los cauces erosionados | Submuestreo por área, no por punto; verificar a ojo en el paso 5 |

## 9. Criterios de éxito

- En `LVL_ProcGenDemo`, el tamaño de texel en una pared de 800 uu no difiere en más de un
  10 % del tamaño en el suelo del pasillo.
- Ninguna junta entre celdas es visible como discontinuidad geométrica.
- 1000 caminos generados sin una sola repetición de módulo dentro de una misma partida.
- Un boceto en PNG produce un módulo cargable en el editor sin pasos manuales.
- La suite de tests existente sigue verde.

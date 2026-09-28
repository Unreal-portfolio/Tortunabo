# Sonido de la tortuga: pasos, jadeo, panzazo y tos

Todo sintetizado en tiempo real, sin archivos de audio, como el resto del juego (ambiente, música, parque del lobby, tos
de la tormenta). Cartoon pero creíble: patas blandas (almohadilla y dedos), nada de botas.

| Qué | Dónde | Cuándo |
|---|---|---|
| Pasos andando, más rápidos y fuertes corriendo | `UTN_TurtleFoleyComponent` | Cada pisada de la animación, con el timbre de la superficie |
| Impulso al saltar y golpe al aterrizar | `UTN_TurtleFoleyComponent` | Al despegar (subiendo a más de 1,5 m/s) y al caer (más de 0,12 s en el aire o a más de 2,5 m/s) |
| Jadeo | `UTN_TurtleFoleyComponent` | Estamina por debajo del 45 %; a tope al agotarse; se calma al recuperarse |
| «Plaf» del panzazo | `UTN_TurtleFoleyComponent` | Al caer de tripa, más fuerte cuanto más rápido caía y corría |
| Arrastre sobre la tripa | `UTN_TurtleFoleyComponent` | Mientras se desliza tras el panzazo, con la fuerza de la velocidad y el timbre de la superficie |
| «Tonc» del caparazón | `UTN_TurtleFoleyComponent` | Al chocar contra algo arrastrándose (la velocidad cambia de golpe) |
| Tos | `UTN_StormCoughComponent` (ya existía) | Dentro de la tormenta del Coop; en la tormenta el jadeo calla |

## Arquitectura

- `Player/TN_TurtleFoleyComponent.h/.cpp`: `USynthComponent` que `ATortugaCharacter::BeginPlay` crea con
  `FindOrAddTo(this)` en cada máquina con audio (nada en servidor dedicado; transitorio, adjunto a la cápsula).
- `Private/Player/TN_TurtleFoleyDSP.h`: el motor (`TNTurtleFoley::FEngine`), C++ puro (solo `CoreMinimal` y `<atomic>`),
  calibrado en un arnés fuera del motor. Corre en el hilo de render de audio: sin UObjects, asignaciones ni bloqueos.
- Estado local por máquina, sin RPC: cada fotograma lee lo replicado de la tortuga (velocidad, en el suelo o en el aire,
  nadando, sprint, estamina, agotada, derribada, muerta, caparazón, panzazo, llevar o ser llevada) y los huesos de su malla.
- La superficie la resuelve `Player/TN_TurtleSurface` (`TNTurtleSurface::Probe` / `Resolve`), compartida con el
  rozamiento del arrastre del panzazo (`UTN_TurtleMovementComponent`) y su polvo (`UTN_TurtleDustComponent`).
- Fuente 3D mono en la raíz de la tortuga: volumen pleno hasta 3 m, caída natural hasta 27 m y agudos que se apagan con
  la distancia. La tortuga local suena ×1,3.
- Hilos: el juego deja las pisadas en un anillo de un escritor (`FSharedParams::PushStep`) y los objetivos (volúmenes,
  jadeo, callar el jadeo) en atómicos. Cada generador lee el anillo con su propio cursor y solo si su arranque es el
  vigente (`RunId`, lo fija `Init`): el de un arranque anterior que aún suene no roba ni repite pasos. `Busy` vuelve
  del audio al juego para no parar el sintetizador a media respiración.
- Coste: el sintetizador arranca con la primera pisada o el primer jadeo y se para tras 2,5 s sin nada (y con el
  generador callado) o lejos del oyente; lejos, el componente pasa a tick de 4 Hz. Una traza de línea por pisada como
  mucho (con caché de 0,15 s / 30 cm) y solo para tortugas que se oyen.

## Pasos

**Sincronía con la zancada.** Se leen los huesos `LeftFoot` y `RightFoot` (Mixamo; también `foot_l`/`foot_r` o los
dedos si cambia la malla) en el espacio de la malla, en `TG_PostPhysics` (la pose ya está evaluada). El «suelo de los
pies» es el pie más bajo, con subida lenta (8 cm/s) para seguir el bamboleo de la cadera de la carrera. Un pie que sube
más de 2 cm queda armado y suena al volver por debajo de 0,9 cm o, si la zancada alargada (`Amplify` del clip de andar)
lo deja un poco en el aire, cuando deja de bajar cerca del suelo (talón). Así suena igual con el clip `Walking` que con
la carrera procedural de `UTN_TurtleAnimInstance` (que pisa en el punto más bajo). Mínimo 0,12 s entre pisadas del mismo
pie.

**Reloj de reserva.** Si la malla no tiene esos huesos, o lleva 0,8 s moviéndose sin que los huesos den pasos (malla sin
animar), un reloj al ritmo de la animación: la cadencia de la carrera (dos pasos por ciclo de `0,8 + v/300` ciclos/s,
entre 2 y 3,4) o, andando, el largo de paso aprendido de los propios huesos (64 cm al empezar).

**Fuerza.** Andando, de 0,35 a 0,66 según la velocidad; corriendo, de 0,88 a 1; agotada, un 6 % más pesados; llevando
a otra tortuga en alto, más graves y pesados. Aterrizaje de 0,45 a 1,35 según la velocidad de caída (las dos patas casi
a la vez). Cada pisada varía al azar tono (±8 %), nivel (de -1,5 a +0,7 dB), brillo y separación almohadilla-dedos; la pata derecha suena
un pelín más aguda y cada tortuga tiene su tono de pata (sale de su semilla).

**Sin pasos** en el aire, nadando, en el caparazón, en panzazo, llevada, derribada o muerta, ni por debajo de 60 cm/s.

**Superficie** (pesos que se mezclan en el sintetizador: arena, tierra, roca, madera, agua):

- Mapa procedural, terreno: mezcla de biomas del sitio (`FLayout::BiomeWeightsAt`).

  | Bioma | Suena a |
  |---|---|
  | Playa, desierto | Arena |
  | Agua con isletas | Arena húmeda (80 % arena, 20 % tierra) |
  | Selva | Tierra y hojarasca (15 % arena) |
  | Manglar | Fango (70 % tierra, 30 % agua) |
  | Volcán | Roca con ceniza y grava (45 % arena) |
  | Acantilados | Roca (20 % arena) |
  | Zona humana | Tierra pisada y empedrado (50/50) |

  En pendientes (normal por debajo de 0,9, del todo a 0,7), hasta un 80 % de roca, como se pinta el talud.
- Agua poco profunda: pisando terreno que queda bajo el nivel del agua del mapa (`TNProcMap::SeaLevel`, el mismo para
  mar, lagunas y río), chapoteo desde 2 cm de profundidad y del todo desde 13 cm. Solo sobre el terreno (un puente no
  chapotea). Las pozas de los toboganes tienen su propio nivel y no se detectan.
- Estructuras del mapa (a más de 35 cm del terreno): la sección 1 de `StructureMesh` son los tablones (madera); el resto
  (piedra, hierro pintado), roca. Hace falta la traza compleja con índice de cara.
- Decorado con malla estática del mapa y todo lo de fuera del mapa: por palabras en el nombre del componente, la clase y
  el nombre del actor, la malla y el material (agua, arena, madera, tierra, roca, por ese orden; el castillo de arena del
  lobby suena a arena y el puente bamboleante a madera). Sin palabras conocidas, roca (el «pat» neutro). Encima de otra
  tortuga, su caparazón suena hueco (madera).

**Síntesis de una pisada.** Fuerza de apoyo en dos contactos (almohadilla y dedos, 18-45 ms entre ellos; más juntos
corriendo) que excita el golpe sordo de la pata (seno grave que cae de ~150 a ~95 Hz) y un «pat» apagado, más la capa de
cada superficie: arena (granos densos de ~1 ms por un paso banda de 2,4-3,4 kHz sobre un «shhh», que se hunde),
tierra (golpe muy apagado a ~300 Hz y crujidos sueltos de hojas), roca (chasquido suave, un «toc» corto a 1,1-1,7 kHz y
arenilla), madera (tablón libre golpeado con algo blando: modos 1 : 2,76 : 5,4 sobre ~200 Hz) y agua (chapoteo que
baja de 3 kHz a 1 kHz, masa de agua grave y hasta tres burbujas que suben de tono).

## Panzazo

El movimiento del arrastre está en `Docs/Animacion_Tortuga.md` («Panzazo: arrastre sobre la tripa»). El sonido solo lee
`ATortugaCharacter::IsBellyPoseActive` / `IsBellyOnGround` y la velocidad replicada, así que suena igual en todas las
máquinas. Durante el panzazo no hay pasos; en cuanto se levanta, vuelven.

- **«Plaf»** (`StepKind::Belly`), al caer de tripa: fuerza de 0,5 a 1,4 (0,55 más la caída por encima de 1,5 m/s entre 9
  más la velocidad entre 25 m/s). Toda la tripa a la vez: los dos contactos casi juntos (4-12 ms), las caídas 1,8 veces
  más largas, el golpe sordo mucho más grave (×0,62) y un chasquido de carne blanda (ruido por un paso alto de
  0,8-1,2 kHz que dura lo que el golpe). Encima, la capa de la superficie: en el agua, un chapuzón con burbujas.
- **Arrastre** (`FDragVoice`, continuo): fuerza = (velocidad sobre la tripa − 25 cm/s) hasta 650 cm/s (`MinDragSpeed`,
  `DragFullSpeed`) con curva suave, y viveza = velocidad / 845 cm/s. Entra en 30 ms y se apaga en 100 ms (el cuerpo aún
  roza al pararse). Más deprisa, filtros más agudos y granos más seguidos. Fondo grave del cuerpo que roza (dos pasos
  bajos a 160 Hz) en todas las superficies, más la de cada una por su peso:

  | Superficie | Suena a |
  |---|---|
  | Arena | «Shhh» granulado (paso banda de 1,9 a 3,6 kHz con granos de 1 ms, de 700 a 2600 por segundo) y siseo de fondo |
  | Tierra | Roce sordo (0,5-1 kHz) y crujidos de hojas sueltas (10-45 por segundo, 3,8 kHz) |
  | Roca | Raspado a trompicones, que se pega y se suelta (1,4-2,6 kHz, 9-40 tirones por segundo) y arenilla aguda |
  | Madera | Roce (0,65-1,4 kHz) y zumbido del tablón (modos a 190 y 520 Hz excitados por el ruido) |
  | Agua | Siseo de la estela (0,9-2,4 kHz), masa de agua que empuja (380 Hz) y burbujas sueltas que suben de tono |

  Unos baches (el nivel sube y baja un 22 % cada 40-110 ms) para que no suene a ruido plano. La superficie se mira con
  la misma traza y caché que los pasos (0,15 s o 30 cm).
- **«Tonc»** (`StepKind::Bump`), al chocar arrastrándose (la velocidad horizontal cambia más de 2,6 m/s de un fotograma
  a otro yendo a más de 1,8 m/s; como mucho uno cada 0,25 s): el golpe sordo del cuerpo (×0,75) y los modos del tablón
  afinados al caparazón (270-340 Hz), sin la textura del suelo.

Los «plaf» y «tonc» suenan aunque el Blueprint tenga `FootstepSound` (no son pasos).

## Guardar y sacar del caparazón

Al cambiar de ranura del inventario (o al sacar lo guardado porque se ha gastado lo de la mano), la aleta va a la
espalda y el objeto entra o sale del caparazón (`Docs/Animacion_Tortuga.md`, «Objetos en las aletas»). En ese momento
`UTN_InventoryComponent` pide `UTN_TurtleFoleyComponent::PlayStash`, en cada máquina y sin red: un «toc» hueco y corto
(`StepKind::Stash`) con los mismos modos del caparazón que el «tonc», sin suelo, más agudos (330-380 Hz al guardar,
440-520 Hz al sacar), más flojos (fuerza 0,6 y 0,5) y algo más cortos. Suena aunque el Blueprint tenga `FootstepSound`.

## Jadeo

- Objetivo 0..1: nada por encima de `PantBelowStamina` (45 % de la estamina máxima con el peso que lleva), sube con curva
  suave hasta el 5 % y a tope al agotarse (`IsExhausted`). Sube en medio segundo y se calma con `PantCalmSeconds` (2,5 s):
  la estamina se recarga deprisa, el jadeo no.
- Respiraciones por la boca con el modelo de la tos (aire y voz por tres formantes): espiración «hah» con la boca
  abierta y la lengua fuera, inspiración «hhh» más aguda y floja. De ~1 ciclo por segundo con el jadeo flojo a ~2,4 al
  agotarse, cada vez más fuerte y, desde la mitad, con algo de voz («huh»). Al calmarse tras un jadeo fuerte, suspira.
- La voz sale de la semilla del jugador con la misma cuenta que la tos (`PlayerId`): jadeo y tos son la misma tortuga.
- En la tormenta manda la tos: mientras `UTN_StormCoughComponent` tose (intensidad > 0 o aún sonando su último
  carraspeo) el jadeo calla al instante (`PantHush`) y vuelve después si sigue cansada. Muerta o derribada, tampoco.

## Tos de la tormenta (comprobación)

La tos ya estaba hecha y enganchada; revisado el camino entero sin encontrar fallos:

1. `ATN_ProcMapGameMode::StartStormIfNeeded` crea `ATN_PathStorm` solo en Coop y con `StormSpeed > 0` en el perfil (en
   el nivel de solo terreno no hay tormenta ni tos).
2. `ATN_PathStorm::Tick` → `TickCough` cada 0,1 s en cada máquina con audio: a cada tortuga viva con
   `ATN_CoopPlayerState` le da su `UTN_StormCoughComponent` (`FindOrAddTo`) y le pasa si está dentro según el frente
   replicado y qué fracción lleva del tiempo que mata; a las muertas las calla.
3. El componente arranca su sintetizador al toser (siempre en la tortuga local; las demás si el oyente está a tiro) y se
   para al callar o lejos.

Con `TN.Voice.Debug 1` la línea de cada tortuga dice `sin tos` (aún no tiene el componente), `tos 0.00` (lo tiene, fuera
de la tormenta) o `tos 0.45 sonando`. Nota: con `TNStorm <sitio>` la tormenta es inofensiva solo en el servidor
(`DebugPlaceFront` no replica `SecondsInsideToDie`): en un cliente la tos llega a lo más fuerte a los 5 s dentro, en el
servidor a los 12 s.

## Ajustes y consola

`UPROPERTY` del componente (se pueden tocar en caliente en el panel de detalles durante PIE): `Loudness`,
`StepLoudness`, `BreathLoudness`, `LocalPlayerBoost` (1,3), `PantBelowStamina` (0,45), `PantCalmSeconds` (2,5),
`MinStepSpeed` (60 cm/s), `DragLoudness` (1: arrastre, «plaf» y «tonc»), `MinDragSpeed` (25 cm/s), `DragFullSpeed`
(650 cm/s), `InnerRadius` (300 cm) y `FalloffDistance` (2400 cm).

| Consola | Qué hace |
|---|---|
| `TN.Voice.Volume <x>` | Volumen de pasos y jadeo de todas las tortugas (1 normal, 0 apagado) |
| `TN.Voice.Surface <-1..4>` | Fuerza la superficie: -1 la de verdad, 0 arena, 1 tierra, 2 roca, 3 madera, 4 agua |
| `TN.Voice.Steps <0\|1\|2>` | Pasos de prueba en el sitio en la tortuga local: 0 apagados, 1 andando, 2 corriendo |
| `TN.Voice.Pant <0\|1\|2>` | Jadeo de prueba en la tortuga local: 0 manda la estamina, 1 suave, 2 agotada |
| `TN.Voice.Drag <0\|1\|2>` | Arrastre de prueba en el sitio en la tortuga local: 0 manda el panzazo, 1 lento, 2 rápido (con `TN.Voice.Surface` para comparar superficies) |
| `TN.Voice.Debug 1` | Una línea por tortuga que se oye: velocidad, pasos por huesos o por reloj, superficie, estamina, jadeo y tos |
| `TN.Storm.Cough <0\|1\|2>` | (ya existía) Tos de prueba en la tortuga local sin tormenta |

Compatibilidad: si el Blueprint asigna el `FootstepSound` de siempre (hoy vacío en `BP_TortugaCharacter`), los pasos
sintetizados callan para no doblarse; el salto, el aterrizaje sintetizados y el jadeo siguen.

## Niveles (arnés fuera del motor, Master 1)

| Sonido | Pico |
|---|---|
| Paso andando (fuerza 0,65) | -16 a -19 dBFS según superficie (madera y agua, 1-2 dB más) |
| Paso corriendo (0,95) | -11 a -14 dBFS |
| Aterrizaje fuerte (1,2) | -6 a -10 dBFS |
| Impulso del salto | -19 a -23 dBFS |
| Jadeo flojo (0,3) / fuerte (0,7) / agotada (1) | -22 / -16 / -13 dBFS |

El limitador de salida (-2 dBFS) solo actúa con el refuerzo de la tortuga local en las caídas más fuertes. Todo por
debajo de la tos (golpes de tos fuerte de -7 a -1,5 dBFS).

El panzazo aún no ha pasado por el arnés: el arrastre está estimado a unos -28 dBFS eficaces a fondo (por el ancho de
banda de cada filtro, `DragTrim`), el «plaf» parte del aterrizaje fuerte (golpe más largo y grave más el chasquido,
`Trim::Slap` 0,3) y el «tonc» del tablón (`Trim::Shell` 0,009). Se afinan de oído con `DragLoudness` y `TN.Voice.Drag`.

## Probar en PIE

1. `TN.Voice.Debug 1` y andar: la línea local dice `pasos: huesos` (si dice `reloj`, la malla no da pasos: ver arriba) y
   la superficie de debajo. Correr (sprint): pasos más seguidos, fuertes y vivos.
2. Mapa procedural: pasar por playa (arena), selva (tierra), acantilados (roca), un puente de tablones (madera) y la
   orilla metiéndose en el agua poco profunda (chapoteo). `TN.Voice.Surface 0..4` con `TN.Voice.Steps 1` para comparar
   las cinco superficies en el sitio.
3. Saltar desde alto: impulso al despegar y golpe al caer, más fuerte cuanto más alto.
4. Esprintar hasta agotarse: jadeo cada vez más seguido y con voz al agotarse; al parar se calma en unos segundos y
   suspira. `TN.Voice.Pant 1` / `2` para oírlo sin correr.
5. Coop con tormenta: dejar que el frente alcance a la tortuga (o `TNStorm desierto`): tose y, si venía jadeando, el
   jadeo calla; al salir, un último carraspeo y vuelve el jadeo si sigue cansada.
6. Con dos jugadores: los pasos y el jadeo del otro se oyen en 3D y se apagan con la distancia; los propios, algo más altos.
7. Panzazo en arena, tierra, roca, un puente de tablones y la orilla: «plaf» al caer y el arrastre de cada superficie,
   que se apaga al pararse; contra una pared, «tonc». En el sitio: `TN.Voice.Drag 2` con `TN.Voice.Surface 0..4`
   (`TN.Voice.Drag 0` para volver).

# Sonido de la tortuga: pasos, jadeo y tos

Todo sintetizado en tiempo real, sin archivos de audio, como el resto del juego (ambiente, música, parque del lobby, tos
de la tormenta). Cartoon pero creíble: patas blandas (almohadilla y dedos), nada de botas.

| Qué | Dónde | Cuándo |
|---|---|---|
| Pasos andando, más rápidos y fuertes corriendo | `UTN_TurtleFoleyComponent` | Cada pisada de la animación, con el timbre de la superficie |
| Impulso al saltar y golpe al aterrizar | `UTN_TurtleFoleyComponent` | Al despegar (subiendo a más de 1,5 m/s) y al caer (más de 0,12 s en el aire o a más de 2,5 m/s) |
| Jadeo | `UTN_TurtleFoleyComponent` | Estamina por debajo del 45 %; a tope al agotarse; se calma al recuperarse |
| Tos | `UTN_StormCoughComponent` (ya existía) | Dentro de la tormenta del Coop; en la tormenta el jadeo calla |

## Arquitectura

- `Player/TN_TurtleFoleyComponent.h/.cpp`: `USynthComponent` que `ATortugaCharacter::BeginPlay` crea con
  `FindOrAddTo(this)` en cada máquina con audio (nada en servidor dedicado; transitorio, adjunto a la cápsula).
- `Private/Player/TN_TurtleFoleyDSP.h`: el motor (`TNTurtleFoley::FEngine`), C++ puro (solo `CoreMinimal` y `<atomic>`),
  calibrado en un arnés fuera del motor. Corre en el hilo de render de audio: sin UObjects, asignaciones ni bloqueos.
- Estado local por máquina, sin RPC: cada fotograma lee lo replicado de la tortuga (velocidad, en el suelo o en el aire,
  nadando, sprint, estamina, agotada, derribada, muerta, caparazón, panzazo, llevar o ser llevada) y los huesos de su malla.
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
`MinStepSpeed` (60 cm/s), `InnerRadius` (300 cm) y `FalloffDistance` (2400 cm).

| Consola | Qué hace |
|---|---|
| `TN.Voice.Volume <x>` | Volumen de pasos y jadeo de todas las tortugas (1 normal, 0 apagado) |
| `TN.Voice.Surface <-1..4>` | Fuerza la superficie: -1 la de verdad, 0 arena, 1 tierra, 2 roca, 3 madera, 4 agua |
| `TN.Voice.Steps <0\|1\|2>` | Pasos de prueba en el sitio en la tortuga local: 0 apagados, 1 andando, 2 corriendo |
| `TN.Voice.Pant <0\|1\|2>` | Jadeo de prueba en la tortuga local: 0 manda la estamina, 1 suave, 2 agotada |
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

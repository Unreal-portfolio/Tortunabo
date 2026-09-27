# Animación de la tortuga

`UTN_TurtleAnimInstance` (`Player/TN_TurtleAnimInstance`) anima a la tortuga del jugador sin AnimBP, en C++, sobre el
esqueleto Mixamo de `TotugaDemo_Rig`. `ATortugaCharacter::BeginPlay` la fuerza como clase de animación. Hereda de
`UTN_ProcAnimInstance`, así que los ajustes por hueso de los sistemas viejos se siguen aplicando al final.

## Cómo se monta la pose

La evaluación (`FTNTurtleAnimProxy::Evaluate`, que puede correr fuera del hilo de juego) hace, en orden:

1. **Locomoción con los clips.** `Old_Man_Idle`, `Walking` y `Drunk_Run_Forward`, mezclados por velocidad como
   `ABS_Walk`: andar entra hasta 1,5 m/s y correr entre 4,8 y 7,3 m/s. Las fases avanzan al ritmo de los pasos (andar
   sin patinar a 3,8 m/s y correr a 7,2 m/s) y la cadera queda en su sitio (sin avance propio de los clips).
2. **Fiesta** (emote 9): el clip `Yelling` con rebote.
3. **Poses de estado** sobre la postura en T, mezcladas con la de arriba por su peso (que entra y sale suave):
   salto (brazos arriba que aletean, piernas recogidas; más arriba al caer), panzazo (brazos por delante, piernas
   estiradas; el personaje ya tumba la malla), nado (brazada y patada), llevar a otra tortuga en alto, ser llevada
   (patalea), tumbada (floja y con la cabeza caída) y los emotes 0-8 (saludar, aplauso, helicóptero, palmada potente,
   aplaudir, baile irlandés, flotar, señalar y modo loco).
4. **Capas encima:** inclinación hacia delante al correr y hacia dentro en las curvas, cansancio (se encorva y
   jadea), el golpe de brazos al lanzar y el caparazón: cabeza, brazos y patas encogen hacia el cuerpo y el cuerpo
   baja al suelo. Con caparazón físico (`bShellBody`, ver abajo) el cuerpo **no** baja: la malla ya va tumbada sobre la
   tripa y ese eje apunta hacia delante.
5. **Levantarse del ragdoll** (`BeginGetUp`): al acabar un derribo, la pose del ragdoll se mezcla hacia la pose de pie
   en 0,75 s (`GetUpW`, curva suave), con un empujón de brazos y rodillas dobladas a mitad de camino (`GetUpFlex`).

Las poses se escriben como giros alrededor de los ejes de la malla (mira a +Y, arriba +Z, su izquierda +X) en la
articulación de cada hueso; los hijos le siguen. Brazo izquierdo: abajo +Y, arriba -Y, adelante +Z (el derecho, al
revés en Y y en Z). Piernas: adelante +X, rodilla -X. Espalda hacia delante -X; cabeza arriba +X.

## Caparazón con física propia

Metida en el caparazón y suelta, la tortuga es una caja física replicada (`ATN_ShellBody`, 55 × 46 × 42 cm, 38 kg,
fricción 0,25 y rebote 0,35) que rueda, resbala y rebota. `UTN_ShellComponent` la crea (`StartBody`) y la quita
(`StopBody`), y en cada máquina `ApplyBodyLocalState` apaga el movimiento del personaje y deja la cápsula solo con
solapamientos. En `TG_PostPhysics`, `ATortugaCharacter::PlaceOnShellBody` pone la cápsula de pie sobre la caja y la
malla tumbada sobre la tripa (rotación de la malla `FQuat(FMatrix((0,-1,0), (0,0,-1), (1,0,0)))`: la cabeza a +X de la
caja, la tripa abajo); `ResetMeshTransform` la devuelve a su sitio al salir.

- Entrar a mano: la caja nace de pie en el tronco y se vuelca hacia delante sobre la tripa.
- Lanzada o escapando del que la lleva: nace tumbada con volteretas y la tortuga sale sola cuando la caja se para.
- Caída de más de 5 m: se hace bola con física y sale al pararse. Al agua: sale y nada.
- Mientras la llevan no hay caja (va enganchada al que la lleva).

## Derribo, pajaritos y ojos

- Todas las fuentes de derribo pasan por el ragdoll de `ApplyKnockdownVisual` y se quedan al menos
  `MinKnockdownSeconds` (2,2 s) en el suelo. Luego se levanta con la mezcla del paso 5.
- `UTN_DizzyBirdsComponent`: tres pájaros y tres estrellitas dando vueltas sobre el hueso `Head` mientras está
  noqueada, con su sonido sintetizado (`UTN_DizzySynthComponent`: trinos y cuerdas mareadas). Se encienden en todas
  las máquinas con el derribo.
- Ojos (`ATortugaCharacter::TickEyes` → `UTN_CosmeticLook::SetEyeState`): parpadea cada 2,5-5,5 s en 0,16 s (a veces
  dos veces seguidas) y pone los ojos en espiral (`EyeDizzy`) mientras está noqueada o muerta. Son dos parámetros de
  `M_TurtleBody`; los tipos de ojo están en `Docs/Tienda_Probador.md`.

## Cara: lengua, cansancio, sudor y boca

`UTN_TurtleFaceComponent` (`Player/TN_TurtleFaceComponent`, subobjeto `TurtleFace` del personaje) anima la cara. Es
cosmético y local en cada máquina: solo lee estado que ya se replica (estamina, sprint, derribo, caparazón, emote,
chat rápido y voz), así que no manda nada por la red. En el servidor dedicado no hace nada. Solo actúa con la malla de
demo (`UTN_CosmeticLook::IsDemoTurtle`: dos ranuras); con otra malla se apaga en su primer fotograma.

### Ánimo (como las caras del HUD)

Con los mismos umbrales que `TN_RunHUDWidget` (energía = `CurrentStamina / MaxStamina`, con margen para no parpadear):

| Ánimo | Cuándo | Cara |
|---|---|---|
| Feliz | energía ≥ 0,5 | Sonrisa abierta pequeña. Quieta, de vez en cuando asoma la punta de la lengua un segundo. |
| Cansada | energía < 0,5 | Párpados a media asta y mirada baja (`EyeTired` 0,5), boca pequeña entreabierta, colorete suave y una gota de sudor. |
| Jadeando | agotada o energía < 0,22 | Párpados más caídos (`EyeTired` 1), boca muy abierta al ritmo del jadeo (2,4 por segundo), la lengua colgando por delante de la barbilla, colorete fuerte, dos gotas de sudor y, a ratos, ojos apretados «>_<». |
| Tumbada | noqueada o muerta | Ojos en espiral (`TickEyes`), boca torcida y la lengua cayendo floja por un lado. |
| Caparazón | metida dentro | Sin lengua ni sudor. |

Además: al agotarse del todo, «>_<» casi un segundo; al esprintar o en el panzazo, la lengua al viento; y emotes con
cara propia: WAZAAA (boca abierta y la lengua fuera meneándose), HAPPIE (sonrisa enorme), modo loco (lengua al aire
cambiando de lado) y fiesta (boca a gritos y ojos apretados).

### La lengua

La lengua rígida de la malla (ranura `lambert2`, sin hueso) se esconde con `HideTongue` de `M_TurtleHelmetSlot`. La
sustituye una malla procedural (`UProceduralMeshComponent`, 12 anillos de 16 vértices y la punta, rosa `#FF6F8E` con
el surco `#D94A6A`, más oscura por debajo y en la raíz) que se reconstruye cada fotograma desde una cadena de 8 puntos:

- **Ancla.** Un `USceneComponent` enganchado al hueso `Head` con la inversa de su postura de referencia: sus hijos se
  colocan en coordenadas de la malla (mira a +Y, arriba +Z, su izquierda +X) y siguen a la cabeza animada, también en
  el ragdoll. La raíz está dentro del hueco de la boca de la malla (`|x|` < 1,7; z 41,3-43,9), en (0; 12,8; 42,1), o
  0,7 hacia una comisura.
- **Simulación** (en el mundo, pasos de ~1/120 s, hasta 4 por fotograma): gravedad, rozamiento con el aire (a la
  carrera empuja la lengua hacia atrás), aleteo (una onda de la raíz a la punta, más rápida y fuerte con el viento) y
  el bombeo del jadeo; muelles de forma que llevan cada tramo hacia su dirección de reposo (del primer tramo, la
  salida de la boca, al último; más blandos hacia la punta) y largo fijo (cada punto sigue al de delante).
- **Choque con la cara.** Una tabla de alturas de la cara vista de frente (medida sobre la malla: z 32-47, `|x|` 0-10,
  el hueco de la boca tapado) saca los puntos que se meten por la normal de la superficie: en las mejillas, hacia el
  lado. Así, al viento, la lengua resbala por la mejilla. La raíz y el primer punto no chocan.
- **Formas de reposo.** Al viento: sale por la comisura, se abre hacia el lado y se dobla hacia atrás. Colgando: por
  delante de la barbilla. Tumbada: floja hacia un lado. Punta: corta y firme, meneándose.
- **Lado al viento.** Al empezar a esprintar, uno al azar; en una curva cerrada (más de 80°/s), el de fuera (girar a
  la derecha la lanza a su izquierda). Al cambiar de lado cruza por delante de la boca.
- Fuera de cámara (`WasRecentlyRendered`) no se simula ni se reconstruye; al volver a verse, o tras un teletransporte,
  la cadena se recoloca en reposo.

Ajustes (propiedades del componente): `SprintTongueLength` (8 unidades de la malla, 20 cm), `PantTongueLength` (6,5),
`TongueHalfWidth`, `TongueShapeStiffness`, `TongueGravity`, `TongueAirDrag`, `TongueFlap` y `bIdleBlep`.

### Boca, ojos y colorete

Son parámetros de `M_TurtleBody` que el componente escribe en la instancia del cuerpo (`UTN_CosmeticLook::GetBodyMaterial`;
si `ApplyLook` crea otra, los vuelve a escribir): `EyeTired`, `EyeSqueeze`, `MouthOpen`, `MouthSmile` y `FaceBlush`
(ver `Docs/Tienda_Probador.md`). La boca se pinta alrededor del hueco de la malla; se abre y se cierra con suavidad.

### Hablar

- **Chat rápido.** El componente escucha `ATN_CoopGameState::OnQuickChatReceived` (el multicast que también saca el
  bocadillo del HUD, en todas las máquinas). Si el mensaje es de su jugador, mira lo largo que es la frase en el
  catálogo del mando local (`ResolveQuickChatDisplayData`) y habla 0,4 s + 0,07 s por letra (entre 1 y 4,2 s).
- **Voz de proximidad.** Mientras `UProximityVoiceComponent::IsHeardSpeaking()` (lo mismo que enseña el bocadillo con
  barras del HUD).
- La boca va por sílabas: de 0,09 a 0,17 s cada una, abriendo de un tercio a del todo, con alguna pausa entre palabras;
  encima de la cara que tenga (cansada o jadeando también habla).

### Sudor

Dos gotas procedurales (media esfera con un cono hasta la punta, celestes con brillo) junto al casco, a cada lado de
la cabeza: aparecen con un saltito, resbalan y se encogen. Una cansada (cada 1,7 s); dos jadeando, a contratiempo
(cada 1,15 s).

### Pruebas por consola (solo en la máquina que las escribe)

- `tn.Face.Mood 0|1|2|3`: fuerza feliz, cansada, jadeando o tumbada (-1 = la real).
- `tn.Face.Tongue 0|1|2|3`: lengua dentro, al viento (como al esprintar), colgando o asomando la punta (-1 = la real).
- `tn.Face.Talk 1`: todas las tortugas mueven la boca como si hablaran.

## Estado que lee

Del personaje: velocidad, `IsFalling`/`IsSwimming` del movimiento, `IsDiving`, `IsInShell` (y si el caparazón tiene
caja física), `IsKnockedDown`, la pose guardada al levantarse, el emote activo y su tiempo (`GetActiveEmoteIndex`,
`GetEmoteTime`); del `UTN_CarryComponent`, si lleva o la llevan (al soltar se hace el lanzamiento); del
`UTN_StaminaComponent`, si está agotada. La cara (`UTN_TurtleFaceComponent`) lee además la estamina, el sprint, la
velocidad, el giro, el chat rápido y la voz.

## Para añadir un clip

Cargarlo en `NativeInitializeAnimation`, pasarlo al proxy en `NativeUpdateAnimation` y mezclarlo en `Evaluate` con
`SampleClip` y `BlendInto` (como `Yelling` en la fiesta).

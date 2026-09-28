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

## Estado que lee

Del personaje: velocidad, `IsFalling`/`IsSwimming` del movimiento, `IsDiving`, `IsInShell` (y si el caparazón tiene
caja física), `IsKnockedDown`, la pose guardada al levantarse, el emote activo y su tiempo (`GetActiveEmoteIndex`,
`GetEmoteTime`); del `UTN_CarryComponent`, si lleva o la llevan (al soltar se hace el lanzamiento); del
`UTN_StaminaComponent`, si está agotada.

## Para añadir un clip

Cargarlo en `NativeInitializeAnimation`, pasarlo al proxy en `NativeUpdateAnimation` y mezclarlo en `Evaluate` con
`SampleClip` y `BlendInto` (como `Yelling` en la fiesta).

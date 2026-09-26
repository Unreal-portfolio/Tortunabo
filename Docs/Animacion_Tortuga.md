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
   baja al suelo.

Las poses se escriben como giros alrededor de los ejes de la malla (mira a +Y, arriba +Z, su izquierda +X) en la
articulación de cada hueso; los hijos le siguen. Brazo izquierdo: abajo +Y, arriba -Y, adelante +Z (el derecho, al
revés en Y y en Z). Piernas: adelante +X, rodilla -X. Espalda hacia delante -X; cabeza arriba +X.

## Estado que lee

Del personaje: velocidad, `IsFalling`/`IsSwimming` del movimiento, `IsDiving`, `IsInShell`, `IsKnockedDown`, el emote
activo y su tiempo (`GetActiveEmoteIndex`, `GetEmoteTime`); del `UTN_CarryComponent`, si lleva o la llevan (al soltar
se hace el lanzamiento); del `UTN_StaminaComponent`, si está agotada.

## Para añadir un clip

Cargarlo en `NativeInitializeAnimation`, pasarlo al proxy en `NativeUpdateAnimation` y mezclarlo en `Evaluate` con
`SampleClip` y `BlendInto` (como `Yelling` en la fiesta).

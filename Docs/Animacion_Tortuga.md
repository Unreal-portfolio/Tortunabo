# Animación de la tortuga

`UTN_TurtleAnimInstance` (`Player/TN_TurtleAnimInstance`) anima a la tortuga del jugador sin AnimBP, en C++, sobre el
esqueleto Mixamo de `TotugaDemo_Rig`. `ATortugaCharacter::BeginPlay` la fuerza como clase de animación. Hereda de
`UTN_ProcAnimInstance`, así que los ajustes por hueso de los sistemas viejos se siguen aplicando al final.

## Cómo se monta la pose

La evaluación (`FTNTurtleAnimProxy::Evaluate`, que puede correr fuera del hilo de juego) hace, en orden:

1. **Locomoción con los clips.** `Old_Man_Idle`, `Walking` y `Drunk_Run_Forward`, mezclados por velocidad como
   `ABS_Walk`: andar entra hasta 1,5 m/s y correr entre 4,8 y 7,3 m/s. Las fases avanzan al ritmo de los pasos (andar
   sin patinar a 3,8 m/s y correr a 7,2 m/s) y la cadera queda en su sitio (sin avance propio de los clips).
   Los clips en bucle funden sus últimos 0,3 s con el principio (`SampleClip`, `LoopFadeSeconds`) salvo `Walking`, cuyo
   ciclo ya cierra (primer y último fotograma iguales): con el fundido se mezclaban dos momentos distintos de la
   zancada y uno de los dos pasos salía más corto.
2. **Fiesta** (emote 9): el clip `Yelling` con rebote.
3. **Poses de estado** sobre la postura en T, mezcladas con la de arriba por su peso (que entra y sale suave):
   salto (brazos arriba que aletean, piernas recogidas; más arriba al caer), panzazo en el aire (brazos por delante,
   piernas estiradas; el personaje ya tumba la malla) y arrastrándose sobre la tripa (ver «Panzazo: arrastre sobre la
   tripa»), nado (brazada y patada), llevar a otra tortuga en alto, ser llevada (patalea), tumbada (floja y con la
   cabeza caída) y los emotes 0-8 (saludar, aplauso, helicóptero, palmada potente, aplaudir, baile irlandés, flotar,
   señalar y modo loco).
4. **Capas encima:** inclinación hacia delante al correr y hacia dentro en las curvas, cansancio (se encorva y
   jadea), el golpe de brazos al lanzar y el caparazón: cabeza, brazos y patas encogen hacia el cuerpo y el cuerpo
   baja al suelo. Con caparazón físico (`bShellBody`, ver abajo) el cuerpo **no** baja: la malla ya va tumbada sobre la
   tripa y ese eje apunta hacia delante.
5. **Levantarse del ragdoll** (`BeginGetUp`): al acabar un derribo, la pose del ragdoll se mezcla hacia la pose de pie
   en 0,75 s (`GetUpW`, curva suave), con un empujón de brazos y rodillas dobladas a mitad de camino (`GetUpFlex`). El
   mismo empujón (`BellyGetUpW`, 0,45 s) al levantarse de la tripa tras el panzazo.

Las poses se escriben como giros alrededor de los ejes de la malla (mira a +Y, arriba +Z, su izquierda +X) en la
articulación de cada hueso; los hijos le siguen. Brazo izquierdo: abajo +Y, arriba -Y, adelante +Z (el derecho, al
revés en Y y en Z). Piernas: adelante +X, rodilla -X. Espalda hacia delante -X; cabeza arriba +X.

## Panzazo: arrastre sobre la tripa

Antes, al caer del panzazo la tortuga se paraba en seco (el frenado de andar la dejaba quieta en una décima) y se quedaba
tiesa hasta levantarse. Ahora se arrastra un poco sobre la tripa, con sonido (`Docs/Sonido_Tortuga.md`) y polvo.

### Decisión: arrastre dentro del movimiento, no física de verdad

Se descartó activar la física del cuerpo (ragdoll o una caja como la del caparazón, `ATN_ShellBody`):

- La física de cada máquina diverge (el ragdoll del derribo desactiva la réplica del movimiento y cada una simula lo
  suyo), y una caja replicada desde el servidor llega con retraso al cliente dueño: tirones al caer y al levantarse en
  cada panzazo, que es un gesto de todo el rato (huecos de panzazo del mapa).
- Al volver de la física hay que poner la cápsula de pie donde quedó el cuerpo, y ahí es donde se atraviesan paredes.

El arrastre es una fase del propio movimiento del personaje, `UTN_TurtleMovementComponent` (`Player/`), que sustituye al
`UCharacterMovementComponent` de serie (`ATortugaCharacter` lo pide con `SetDefaultSubobjectClass`; el Blueprint conserva
sus ajustes del movimiento: conviene abrir y guardar `BP_TortugaCharacter` una vez). Lo simulan igual el cliente dueño
(predicho) y el servidor; la cápsula barre como siempre (no atraviesa nada) y el resto de máquinas solo ven el
movimiento replicado.

### Fases (`ETNBellyPhase`)

1. **En el aire**: como antes (`Server_StartDive`: 350 cm/s en el BP más la velocidad del salto, bajando, sin control).
   El servidor sube `DiveSerial` (número del panzazo, replicado) al empezar cada uno.
2. **Arrastre** (`Slide`), al caer de tripa (`ProcessLanded`, antes del aterrizaje normal: el resto de ese movimiento ya
   se arrastra):
   - Inercia: la velocidad a lo largo del suelo tocado (en una bajada, parte de la caída se convierte en arrastre; en
     una subida se pierde), un 90 % (`BellyLandingKeep`) y como mucho 850 cm/s (`BellyMaxEntrySpeed`).
   - Frenado: rozamiento seco por superficie (`TNTurtleSurface`, la misma de los pasos: arena 800, tierra 480, roca 400,
     madera 310 y agua poco profunda o fango 220 cm/s²), más un freno por velocidad (`BellyDrag` 1,5/s). Con la entrada
     típica (720 cm/s) se para en 0,57 s y 1,8 m en arena, 0,79 s en tierra, 0,87 s en roca, 1 s en madera y 1,18 s
     y 3,1 m en el agua. Desde 1,2 s el rozamiento crece (×3,5 al segundo) y a los 2,6 s se levanta igualmente.
   - Pendientes: la gravedad a lo largo del suelo (×1,15, `BellySlopeGravity`): cuesta abajo acelera (hasta 1000 cm/s),
     cuesta arriba frena antes y en las suaves el rozamiento puede más y se queda quieta.
   - Rebote: contra paredes y obstáculos (y otras tortugas), si iba contra ellos a más de 120 cm/s, devuelve un 35 % de
     esa velocidad hacia fuera y conserva un 75 % de la que llevaba a lo largo (`HandleImpact` apunta la pared y
     `OnMovementUpdated` rebota al final del movimiento).
   - El cuerpo gira despacio hacia donde se desliza (220°/s) salvo tras un rebote hacia atrás (se aleja mirando la pared).
   - Sin control: el jugador no dirige. Si cae por un borde sigue sobre la tripa y al volver al suelo continúa.
3. **Levantarse**: pasados 0,3 s y por debajo de 60 cm/s, o a propósito por debajo de 180 cm/s (`BellyExitSpeed`)
   moviéndose (el movimiento solo llega entonces, `ATortugaCharacter::Move`) o saltando (un brinco: la cápsula se pone
   de pie y salta). Quien lleva el avance pulsado todo el panzazo se levanta ahí: se pierde solo el final lento. La cápsula vuelve a su altura como `UnCrouch` con la base fija: si se metería en algo (un techo
   bajo), no se levanta.
   - **Reptar** (`Rest`): sin sitio para ponerse de pie, sigue sobre la tripa y se mueve a 150 cm/s hasta que quepa. Ni
     salta ni atraviesa nada.
   - **De pie** (`GetUp`): 0,35 s con la velocidad máxima subiendo del 35 % a la normal. En cuanto el movimiento la pone
     de pie, el servidor acaba el panzazo (`TickDive` → `EndDive`); el dueño y el servidor quitan la pose al momento,
     las demás máquinas al llegar el fin del panzazo.

Seguridad: la fase, su tiempo y el número de panzazo del que viene viajan en cada movimiento guardado del cliente
(`FTNSavedMove_Turtle`) y se repiten tras una corrección, cápsula encogida incluida; `DiveSerial` impide volver a
arrastrarse con el mismo panzazo mientras llega su fin. Con `TN.Dive.Slide 0` todo vuelve a ser como antes. Tope de
seguridad de todo el panzazo: 12 s (`DiveMaxSeconds`). Ni más rápido que correr (entra a 850 como mucho y frena en
seguida: encadenando salto, panzazo, arrastre y levantarse se va a unos 3,9 m/s andando y 5,7 m/s esprintando, por
debajo de los 4,5 y 8 m/s de ir corriendo) ni atravesar paredes (barrido de la cápsula y comprobación de sitio al
levantarse).

### Pose

- `PoseBellySlide` (sobre la tripa en el suelo, `SlideW`): cabeza levantada mirando adelante y a los lados, brazos
  abiertos por delante que rozan el suelo y tiemblan con los baches, piernas algo abiertas con las rodillas dobladas y
  los pies arriba pataleando, la espalda arqueada y la cadera que rueda sobre la tripa y culea. Más deprisa
  (`SlideSpeed`, 0..1 a 7 m/s), más vibra; casi parada, rema con los brazos. Los golpes (`SlideImpact`: caer de tripa,
  chocar) sacuden brazos, pies y cabeza.
- En el aire sigue `PoseDive`; las dos se reparten el peso del panzazo por `SlideW`.
- Al levantarse del suelo, el empujón de brazos y rodillas (`PoseGetUpFlex`, `BellyGetUpW` 0,45 s) mientras
  `TickDive` endereza la malla algo más despacio (`DiveGetUpTiltSpeed` 7/s en vez de 12). La subida de la malla sobre la
  tripa se calcula con la cápsula que haya en cada momento: al ponerse de pie ya no pega un salto.
- `ATortugaCharacter::IsBellyPoseActive` (pose de panzazo hasta levantarse) e `IsBellyOnGround` (sobre la tripa en el
  suelo) es lo que leen la animación, el sonido y el polvo.

### Polvo

`UTN_TurtleDustComponent` (`Player/`, local y cosmético, nada en servidor dedicado): partículas de caras planas
(`TNAmbientFX`, las de los géiseres y el rebuscar) del color y el material de debajo de la tripa: nube clara y granos en
la arena, polvo marrón y terrones en la tierra, polvo gris y arenilla en la roca, serrín y astillas en la madera, rocío y
salpicaduras en el agua. En el terreno del mapa, arena, tierra y roca se tiñen con el camino (o la roca) del bioma.
Bocanada al caer de tripa (más grande cuanto más fuerte), rastro hacia atrás y arriba mientras se arrastra (más cuanto
más deprisa, a tope a 6,5 m/s) y bocanada pequeña al chocar. Hasta 8 emisores por tortuga que se crean al hacer falta y
solo se mueven con partículas vivas; nada a más de 50 m de la cámara. Ajustes: `DustAmount`, `MaxViewDistance` y
`FullDustSpeed`.

### Consola (afecta a la simulación: igual en el servidor y los clientes; en PIE es un solo proceso)

| Consola | Qué hace |
|---|---|
| `TN.Dive.Slide 0\|1` | 0 = se para en seco al caer, como antes |
| `TN.Dive.Friction <x>` | Multiplica el rozamiento en todas las superficies (0,5 = resbala el doble; 2 = se para antes) |
| `TN.Dive.Slope <x>` | Multiplica cuánto tiran las pendientes (0 = como en llano) |
| `TN.Dive.MaxTime <s>` | Tope de segundos arrastrándose (0 = el del componente, 2,6 s) |
| `TN.Dive.Debug 1` | Por cada tortuga simulada en esta máquina: fase, tiempo, velocidad, superficie y rozamiento; flecha verde de la velocidad y naranja de la pendiente |

Los ajustes finos son `UPROPERTY` del movimiento (`Belly Slide`: rozamientos, `BellyDrag`, entrada, topes, tiempos,
salida, rebote y giro) y del personaje (`DiveGetUpTiltSpeed`, `DiveMaxSeconds`).

### Probar en PIE

1. Escuchando más un cliente. `TN.Dive.Debug 1` y `TN.Voice.Debug 1`.
2. Panzazo en llano de arena (playa): cae de tripa con «plaf» y bocanada, se arrastra ~1,8 m con siseo y polvo claro,
   brazos y pies moviéndose, y se levanta con el empujón de brazos. Repetir en tierra (selva), roca (acantilados),
   tablones de un puente y agua poco profunda de la orilla: más o menos arrastre, otro sonido y otro polvo.
3. Panzazo cuesta abajo: se arrastra más; cuesta arriba, menos. En una cuesta suave se queda quieta.
4. Panzazo contra una pared: rebota un poco hacia atrás con un «tonc» y una bocanada.
5. Saltar o moverse casi parada: sale antes (el salto, con brinco). Deprisa, ni lo uno ni lo otro.
6. Panzazo que acabe bajo algo bajo (un tablón, una rampa): repta hasta salir y se levanta sin atravesar nada.
7. En el cliente: lo mismo sin tirones al caer ni al levantarse (con `TN.Dive.Debug 1`, la fase del cliente y la del
   servidor van a la par). Con latencia (`NetEmulation.PktLag 120`), el inicio del panzazo sigue teniendo el tirón de
   siempre (lo lanza el servidor), pero el arrastre y el levantarse no.
8. `TN.Dive.Friction 0.5` y `2`, `TN.Dive.Slide 0` para comparar con lo de antes.

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

Del personaje: velocidad, `IsFalling`/`IsSwimming` del movimiento, `IsBellyPoseActive` e `IsBellyOnGround` (el
panzazo en el aire y sobre la tripa), `IsInShell` (y si el caparazón tiene caja física), `IsKnockedDown`, la pose guardada al levantarse, el emote activo y su tiempo (`GetActiveEmoteIndex`,
`GetEmoteTime`); del `UTN_CarryComponent`, si lleva o la llevan (al soltar se hace el lanzamiento); del
`UTN_StaminaComponent`, si está agotada. La cara (`UTN_TurtleFaceComponent`) lee además la estamina, el sprint, la
velocidad, el giro, el chat rápido y la voz.

## Para añadir un clip

Cargarlo en `NativeInitializeAnimation`, pasarlo al proxy en `NativeUpdateAnimation` y mezclarlo en `Evaluate` con
`SampleClip` y `BlendInto` (como `Yelling` en la fiesta).

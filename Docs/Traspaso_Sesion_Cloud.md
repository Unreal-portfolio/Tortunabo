# Traspaso a la sesión cloud (26-09-2026)

Documento para que la sesión en la nube siga el trabajo de la sesión local sin perder nada. Contiene cómo trabajamos a
partir de ahora, qué está hecho (con dónde vive cada cosa), qué queda por hacer con todo el detalle que ha pedido el
usuario y los datos técnicos que hacen falta para no repetir investigaciones.

Proyecto: **Tortunavy** (nombre en clave y de código: Tortunabo), Unreal Engine 5.6, C++ (módulo `Tortunabo`).
Repositorio: `Unreal-portfolio/Tortunabo`, rama **`claude/elegant-fermi-6n1hxy`** (PR Unreal-portfolio/Tortunabo#8).

---

## 0. Cómo trabajamos a partir de ahora (obligatorio)

1. **El usuario (Mokius) es el tester.** La sesión cloud escribe el código, hace commit y push a la rama; el usuario
   hace pull en su máquina, compila (UBT o el propio editor), prueba en PIE y devuelve el feedback y los errores de
   compilación. Cada entrega debe terminar con una lista **exacta** de qué probar (qué hacer y qué debería verse),
   como la del apartado 6.
2. **La sesión cloud no puede compilar ni abrir el editor.** Hay que escribir con cuidado:
   - Usar APIs que ya use el repo siempre que se pueda. Si hace falta una nueva, comprobar la firma exacta de UE 5.6.
   - Commits pequeños por funcionalidad, para que un error de compilación sea fácil de localizar.
   - Si el usuario pega un error de compilación, arreglarlo antes de seguir con otra cosa.
3. **Commits como Mokius y sin «Co-Authored-By»:**
   `git -c user.name=Mokius -c user.email=jose.motalucas@gmail.com commit ...`
   Mensajes en español técnico y con la ortografía completa, como los que ya hay (`feat(lobby): ...`, `fix(tortuga): ...`).
4. **No subir nunca** `Content/Maps/Lobby/LVL_HQ.umap`, `Content/ProcMap/` ni `Content/Maps/Run/LVL_ProcMap.umap`: son
   locales del usuario.
5. **Los binarios (.uasset/.umap) no se pueden editar desde la nube.** Todo lo que necesite contenido nuevo se genera
   en ejecución, por código, como ya se hace con los puestos, las botellas, el mapa procedural o los cascos. Si no hay
   más remedio, se le pide al usuario con los pasos exactos. `Scripts/build_cosmetics.py` y el resto de scripts de
   Python se ejecutan **dentro del editor**: se pueden modificar desde la nube, pero los ejecuta el usuario.
6. **Compilación unity** (errores habituales que hay que evitar):
   - Nada de nombres sueltos en el namespace global ni en namespaces anónimos que puedan chocar: cada archivo usa su
     propio namespace (`TNShopKeeperDetail`, `TNBoothDetail`, `TNTurtleAnim`...).
   - Los avisos **C4456–C4459** (una variable que oculta otra) son **errores**. No llamar a variables locales `Text`,
     `Panel`, `Edge`, `Accent`, `Sand`, `Coral`, `Bar`, `Font` ni `Rounded` (existen en `TNHUDStyle`), ni `Make`,
     `MakeText`, `Place` o `CardMargin` (existen en el namespace del HUD).
   - Cuidado con los `using namespace` dentro de funciones: en unity build pueden chocar con los de otro `.cpp`.
7. **Idioma y tono**:
   - Chat con el usuario: español de España, escueto, una línea por punto («resultado + cifra»).
   - Código, comentarios, commits y documentación: español técnico con tildes, ñ, ¿? y ¡!.
   - En lo visible, el juego se llama «Tortunavy»; «Tortunabo» queda en lo técnico.
8. **Autonomía creativa:** el usuario delega las decisiones de diseño. Se decide y se avanza; solo se pregunta lo
   irreversible.
9. Después de cada lote hay que actualizar la documentación que toque (`Docs/Tienda_Probador.md`,
   `Docs/Animacion_Tortuga.md`, `Docs/Mapa_Procedural.md`...) y este traspaso si cambia el plan.

---

## 1. Estado del repositorio al traspasar

Commits de la sesión local encima de lo que ya estaba en la PR, del más reciente al más antiguo:

| Commit | Qué hace |
|---|---|
| `2b3026e` | Música sintetizada de tienda y probador; el aviso de interacción sale solo cuando pulsar funciona. |
| `f0831c5` | Paso sin patinar, carrera del sprint, emotes corregidos, panzazo apoyado y aplastado, ragdoll con la malla nueva, miembros dentro del caparazón, probador con el culo petaloide y etiqueta. |
| `f4e675c` | Tienda de Don Tortugo, probadores de botella, cosméticos reales, HUD de interacción, animación de la tortuga y lo del mapa procedural de esa tanda (poza, fauna, sonido ambiente, modo solo terreno, TNStorm). |
| `4f5a9c5` y anteriores | Mapa procedural, HUD, marca, etc. (ya descritos en `Docs/Mapa_Procedural.md` y `Docs/Mapa_Procedural_Traspaso.md`). |

Todo lo commiteado **compila** (UBT verde en local) y el usuario lo ha probado en su mayoría (apartado 6).

**En el árbol local, sin commitear**: tres agentes en segundo plano estaban escribiendo archivos nuevos cuando se
redactó esto (apartado 3). Si la sesión local llega a integrarlos y compilarlos, los subirá con su commit. Si no, el
usuario los encontrará sin commitear en su máquina; la sesión cloud los tendrá que rehacer o pedirlos.

---

## 2. Qué está hecho y dónde vive

### 2.1 Tortuga: malla, esqueleto y convenciones
- Malla del jugador: `/Game/Meshses/Characters/Player/TotugaDemo_Rig` (esqueleto Mixamo). La carpeta se llama
  «Meshses», con esa errata.
- Tiene 2 ranuras de material: `lambert2` (casco rojo de serie, lengua y correas) y `lambert4` (cuerpo, caparazón y
  ojos juntos). No tiene sockets propios, salvo `Sombrero` si lo trae la malla unificada.
- En `BP_TortugaCharacter` (`/Game/Blueprints/Characters/`):
  - Malla: escala 2,5, posición relativa (0, 0, -70) y yaw -90 (la +Y de la malla apunta a la +X del actor).
  - Cápsula: semialtura 70; en el panzazo encoge a `DiveCapsuleHalfHeight` = 35.
  - `DiveTiltAxis` = (0, -1, 0) en el BP.
- Ejes de la malla: mira a **+Y**, arriba **+Z** y **su izquierda es +X** (unidades de malla, 53 de alto).
- Huesos principales, en unidades de malla (postura en T):
  - Tronco: `Hips` (0, -0,4, 24,6), `Spine1` (0, 1,1, 29,2), `Neck` (0, 3,1, 35,3), `Head` (0, 4,2, 38,0) y
    `HeadTop_End` (0, 10,1, 52,6).
  - Brazo izquierdo: `LeftArm` (6,2, 3,1, 33,6), `LeftForeArm` (15,4, 2,2, 32,8) y `LeftHand` (27,6, 1,3, 34,1).
  - Pierna izquierda: `LeftUpLeg` (3,1, -0,3, 23,4), `LeftLeg` (3,2, -0,3, 13,0) y `LeftFoot` (3,9, -0,6, 3,8).
  - La cabeza es grande: x ±10,5, y de -3,4 a 21,4 (hocico) y z de 37,5 a 53,4.
- **Herramienta sin editor**: `Scripts/tools/fk_sim.py` reproduce `Turn()` de la animación sobre la postura de
  referencia (datos en `Scripts/tools/data/`). Así se diseñan y comprueban las poses (manos, holgura con la cabeza)
  antes de escribir C++. Con ella se ajustaron los emotes de esta sesión.

### 2.2 Animación: `UTN_TurtleAnimInstance` (`Player/TN_TurtleAnimInstance.*`) — ver `Docs/Animacion_Tortuga.md`
- Sin AnimBP. `ATortugaCharacter::BeginPlay` la fuerza con `GetMesh()->SetAnimInstanceClass(...)`.
- Hereda de `UTN_ProcAnimInstance`. El proxy propio (`FTNTurtleAnimProxy::Evaluate`) monta la pose.
- Locomoción:
  - Clips: `Old_Man_Idle` y `Walking`, en `/Game/Animations/Character/TortugaDemo/Anim/`.
  - Andar: la zancada del clip se alarga un 25 % (`Amplify`) y el ritmo sigue a la velocidad (`WalkNaturalUnits` =
    51 u/s × escala), con tope de 4,5×.
  - Correr (`PoseRun`, en código): entra con el sprint (`UTN_StaminaComponent::IsSprinting`) o al pasar un 8 % de la
    velocidad de andar del nivel (en el lobby es 200 cm/s; en C++ vale 450 y el sprint 800). Ritmo de 2 a 3,4 ciclos/s
    y amplitud del paso calculada para que el pie apoyado no patine.
- Capas de estado (pose sobre la postura en T mezclada por peso): salto, panzazo (reman los brazos y los pies apoyan),
  nado, llevar, ser llevada, tumbada y emotes.
- Capas finales: inclinación, cansancio, lanzamiento y caparazón (miembros al 8 % y metidos hacia `Spine1`; el cuerpo
  baja 19 u).
- Emotes (id → nombre en `DA_EmoteWheelCatalog`): 0 WAZAAA, 1 HAPPIE, 2 PARACOPTER, 3 SAX-O, 5 RUN, 6 SUPERKIRK,
  8 MISTIK y 9 PATRICK (el clip `Yelling`). Los 4 y 7 no están en la rueda. El usuario ha validado el 8 y el 9 tal
  cual; los 0, 1, 2, 3, 5 y 6 están corregidos según su feedback y **pendientes de su confirmación**.

### 2.3 Personaje: panzazo, derribo y ragdoll
- Panzazo (`TortugaCharacter_Dive.cpp`):
  - La malla se inclina -80° alrededor de sus pies.
  - Se sube `(DiveBellyPivotHeight − DiveCapsuleHalfHeight − Z por defecto)` para apoyarse en la tripa (antes se
    hundía entera). `DiveBellyPivotHeight` vale 11.
  - Se aplasta con `DiveSquash` = (1,08; 0,8; 1,05) en los ejes locales de la malla.
  - Todo se deshace al acabar y en los caminos de derribo o muerte.
  - `DiveMeshDefaultLoc/Rot/Scale` se guardan en BeginPlay; las instantáneas del derribo guardan la posición y la
    escala por defecto.
- **Ragdoll**:
  - El BP tenía un `PhysicsAssetOverride` de la malla vieja (`SKM_MERGED_TORTUGANIGGER_C_1_Physics`) sin huesos en
    común con la nueva, así que no se creaba ningún cuerpo («InitArticulated: Could not find root physics body»).
  - `ATortugaCharacter::BeginPlay` lo detecta y usa `TotugaDemo_Rig_PhysicsAsset`, que tiene 5 cuerpos: Hips, Spine,
    Neck, LeftUpLeg y RightUpLeg.
  - El usuario confirma que **el plátano ya funciona**.
- Derribo (`TortugaCharacter_Knockdown.cpp`): `ApplyKnockdownVisual` (ruta A: ragdoll con la velocidad del CMC como
  velocidad inicial) y `EnterRagdollState` / `ExitRagdollState` (muerte).

### 2.4 Cosméticos, tienda y probador — ver `Docs/Tienda_Probador.md`
- Tipos: `Core/TN_CosmeticsTypes.h` (`FTN_HelmetData`, `FTN_SkinData`, `FTN_TurtleLook` con casco, caparazón y color).
- `Core/TN_CosmeticLook.*` viste a una tortuga con un `FTN_TurtleLook`. Lo usan el personaje, el tendero y las vistas
  previas.
- Contenido generado por `Scripts/build_cosmetics.py` (dentro del editor):
  - Materiales: `M_CosmeticVertexColor`, `M_TurtleBody` (máscaras por posición antes del skinning), `M_TurtleHelmetSlot`
    y `M_UI_Preview`.
  - Doce cascos: recetas en `Scripts/cosmetics_meshes.py`.
  - `DT_Helmets` (12 filas) y `DT_Skins` (10 caparazones y 12 colores).
- `Lobby/TN_ShopKeeper.*`: Don Tortugo, el puesto «La Concha Dorada» con su radio de música 3D y el punto de
  interacción delante del mostrador.
- `Lobby/TN_ChangingBooth.*`: media botella boca abajo con el culo petaloide de 5 pies, etiqueta naranja y crema con
  «PROBADOR» letra a letra sobre la curva, y la chapa roja como puerta (más pegada). `Occupant` está replicado.
- `Lobby/TN_CosmeticPreview.*`: escaparate con captura.
- `UI/Shop/TN_ShopWidgets.*` y `TN_ShopArt.h`: menús de tienda y probador, con la música 2D del menú.
- `ATN_HQGameMode::SpawnLobbyShops` coloca tienda y probadores sobre la maqueta de `LVL_Lobby`.
- **Música** (`Audio/TN_MusicSynthComponent.*` y `TN_MusicSynthDSP.h`): sintetizada.
  - API: `PlayTrack(ETNMusicTrack::Shop | Booth, Fade)`, `StopMusic`, `SetMusicVolume`, `AttachMusic2D(Actor)` y
    `AttachMusic3D(...)`.
  - La radio del puesto baja al 15 % con un menú abierto (`ATN_ShopKeeper::SetRadiosDucked`).
- **Aviso de interacción** (`UTN_RunHUDWidget::TickPrompt`): `ATN_InteractableBase::GetInteractionPoint()` (virtual)
  hace que el escaneo (`TortugaCharacter_Interaction.cpp`) y `ServerTryInteract` midan lo mismo.

### 2.5 Mapa procedural (resumen; detalle en `Docs/Mapa_Procedural.md`)
- Poza de las cascadas a ras del suelo y con 1 m de fondo (`TNProcMap::SlidePoolOf` / `PoolDims`).
- Fauna (`TN_ProcFauna.*`).
- Sonido ambiente sintetizado (`Audio/TN_AmbientSynthComponent.*` y `TN_AmbientSoundscape.*`).
- Modo solo terreno (`bTerrainOnly` y `Game/TN_TerrainViewGameMode.*`; falta crear el nivel `LVL_ProcMap_Terrain`,
  que necesita el editor, así que lo hace el usuario).
- Prueba de tormenta: `TNStorm <Bioma|Geiser|Cascada|Off> [distancia]`.

### 2.6 Lobby (`LVL_Lobby`): maqueta actual (coordenadas en cm)
El jugador aparece en (0, 0). Mirando hacia +Y (hacia la zona de salida), la **izquierda es +X** y la **derecha es -X**.

- **Suelo**: 50 × 50 m, entre ±2500. Las **paredes** son el actor «Extrude» en (186, -16), de 52 × 51 m y unos 10 m
  de alto, con textura de cuadrícula.
- **Zona de salida** (`TN_LobbyReadyZone`, en (0, 2620), de 14 × 7 m): recinto de vallas `BP_Fence*` con 4 torres
  `BP_Tower_*` en (500, 2199), (-899, 2199), (-900, 2999) y (500, 2999). Tiene puertas `BP_FenceDoor_*` en y = 2199 y
  en y = 2999.
- **Tienda**: el tendero de la maqueta está en (510, 1805) y la carpa `SM_ChangingTent_0` en (1130, 1760), a la
  izquierda.
- **General de la maqueta** (a la derecha): un `SkeletalMeshActor` («TotugaDemo_Rig2») en (-892, 1479). La mesa son
  los `StaticMeshActor` «Boolean», en (-1000, 1570), y «Boolean2», en (-807, 1410).
- **Probadores**: botellas `BP_VestidorBotella_*` en (-1400, 1100), (-1800, 700), (-1600, 200) y (-1800, -300). La
  puerta de prueba `BP_ShellDoor_0` está en (-1412, 914).
- **Huevos**: cuatro `StaticMeshActor` «Capsule*» en el centro, cerca de (0, -250).
- `PlayerStart` en (0, 0, 92). Además hay un poste `SM_Palo` en (820, 1340).

---

## 3. En curso al traspasar (agentes locales en segundo plano)

Escriben solo archivos nuevos y no compilan. Si la sesión local no los integra antes de cerrarse, quedan sin commitear
en la máquina del usuario.

1. **Pantalla de carga del huevo**
   - Archivos: `UI/Loading/`, más «MoviePlayer» en el `Build.cs` si hace falta.
   - `UTN_LoadingScreenSubsystem` (un `UGameInstanceSubsystem`, así que no toca el GameInstance) y el widget Slate
     `STN_EggLoadingScreen`.
   - Secuencia: las dos mitades del huevo se cierran desde arriba y desde abajo, cuatro tortugas 2D caminan (turquesa,
     coral, dorado y morado) y, con el mapa y el terreno listos, el huevo tiembla y se rompe con crujido y «¡pum!»
     sintetizados.
   - MoviePlayer fuera de PIE y overlay en el viewport en PIE.
   - Pruebas: `TN.Loading.Test`, `TN.Loading.Test.Hold` y `TN.Loading.Test.Break`.
2. **General del cuartel**
   - Archivos: `Lobby/TN_GeneralBriefing.*` y `UI/Briefing/TN_BriefingWidget.*`, más `ClientOpenBriefing` en
     `AMP_GamePlayerController`.
   - NPC con el patrón del tendero y una mesa de madera con un mini cuartel hecho en código.
   - Pestañas «Cómo se juega», «Modos de juego», «Reglas» y «Controles» (teclas reales de Enhanced Input).
   - Integración pendiente: llamar a su colocación desde `ATN_HQGameMode::SpawnLobbyShops`, sobre la maqueta del
     general.
3. **Pajaritos del mareo** (`Player/TN_DizzyBirdsComponent.*`, con su synth)
   - Tres pajaritos low-poly y estrellitas que dan vueltas sobre la cabeza.
   - Sonido sintetizado: piar y «cuerdas mareadas» de dibujos animados.
   - API: `SetDizzy(bool)`.
   - Integración pendiente: crearlo en `ATortugaCharacter`, adjuntarlo al hueso `Head` de la malla y encenderlo con
     `bIsKnockedDown` en todas las máquinas (en `ApplyKnockdownVisual` y en su OnRep).

---

## 4. Pendiente, por prioridad (con el detalle del usuario)

### P1. Caparazón con física propia (pedido explícito)
Pedido del usuario, textual: «cuando se hace caparazón… las haría pequeñas y las metería internamente [hecho] y haz
que caiga al suelo totalmente, que tenga física propia ese caparazón, ya que como no lo controlas, que cuando tú
entres en caparazón entres en modo física… y recuerda que tus otros compañeros te deberían poder coger y lanzar en ese
estado».

Diseño decidido (sin escribir aún):
- **Actor nuevo `ATN_ShellBody`** (`Player/TN_ShellBody.*`), replicado:
  - `bReplicates` y `SetReplicatingMovement(true)`, con `SetNetUpdateFrequency(30)`.
  - Raíz: un `UBoxComponent` que simula física. Extent (27,5; 23; 21) cm: largo cola-cabeza, ancho y alto
    tripa-lomo, sacados del tronco de la malla (x ±10; y de -8,5 a 9; z de 16 a 38, en unidades).
  - Perfil `PhysicsActor`, que ignora `ECC_Camera`. Masa de unos 38 kg (`BodyInstance.SetMassOverride`),
    `LinearDamping` 0,25, `AngularDamping` 1,1 y CCD.
  - Material físico resbaladizo creado en ejecución: fricción 0,25 y restitución 0,35. Al cambiar sus valores en
    ejecución hay que refrescar el material de Chaos con `FPhysicsInterface::UpdateMaterial(Mat->GetPhysicsMaterial(), Mat)`.
  - Tick en `TG_PostPhysics`, en todas las máquinas. Sigue a la tortuga con dos movimientos:
    1. Pone la cápsula de pie sobre la caja: `SetActorLocation(centro + (0, 0, 70 − 21))`, teleport y sin barrido.
    2. Pone la malla con `SetWorldTransform(MeshLocal × CajaMundo)`. `MeshLocal` lleva la rotación de la malla
       tumbada sobre la tripa, el origen de la malla en (-67,5; 0; 0,6) cm respecto del centro de la caja y la escala
       por defecto (2,5). La rotación se construye así: `FQuat(FMatrix(FPlane(0,-1,0,0), FPlane(0,0,-1,0), FPlane(1,0,0,0), FPlane(0,0,0,1)))`
       (la cabeza, +Z de la malla, va a +X de la caja; la tripa, +Y, abajo; y su izquierda, +X, a -Y).
  - En el servidor:
    - Si viene de un **lanzamiento**, cuando se para (velocidad < 60 cm/s y giro < 1,5 rad/s durante 0,35 s)
      desbloquea y hace `ForceExitShell`. Así se conserva el diseño de «rebota y sale».
    - Si cae al agua (recorrer los `APhysicsVolume` con `bWaterVolume` y usar `IsOverlapInVolume(*Body)`; el agua del
      mapa es `ATN_ProcWaterVolume`), sale del caparazón y nada.
  - En `EndPlay` del servidor, si se destruye solo (por ejemplo, al caer del mundo), avisa al componente de caparazón
    para que la tortuga no se quede sin movimiento.
- **`UTN_ShellComponent`**:
  - `UPROPERTY(ReplicatedUsing=OnRep_Body) TObjectPtr<ATN_ShellBody> Body`.
  - `StartBody(Velocity, bThrown)` (servidor). Al entrar voluntariamente, la caja nace **de pie**
    (`FRotator(90, Yaw, 0)`) en el tronco (ubicación del actor − 2,5 cm en Z) y con una velocidad angular de
    `RightVector × 3 rad/s` para que **se vuelque hacia delante y caiga sobre la tripa**. Si es un lanzamiento, nace
    tumbada con giro de `RightVector × 7`. En los dos casos hereda la velocidad.
  - `StopBody()`: coloca a la tortuga de pie donde está el caparazón (traza al suelo), con el yaw hacia donde apunta la
    cabeza, y destruye la caja.
  - `ForceEnterShell(bool bPhysics = true)`: coger a alguien lo mete en el caparazón **sin** cuerpo físico.
  - `ApplyShellState(true)` en el servidor llama a `StartBody` si no la llevan; `ApplyShellState(false)` llama a
    `StopBody`.
  - `ApplyBodyLocalState(bool)` corre en todas las máquinas (desde el servidor y desde `OnRep_Body`):
    - Al activarse:
      - CMC: `StopMovementImmediately` y `DisableMovement`, tick apagado y `NetworkSmoothingMode` en Disabled.
      - Cápsula en QueryOnly con **overlap a todo** (las zonas y disparadores la siguen viendo), sin bloquear, e
        ignorando Camera y Visibility.
      - En el servidor, `SetReplicateMovement(false)`: cada máquina sigue localmente a la caja, que ya se replica sola.
    - Al desactivarse:
      - Restaurar la colisión de la cápsula: el perfil guardado o sus respuestas si era «Custom».
      - Restaurar la malla con `ResetMeshTransform()`, función nueva en el personaje que usa
        `DiveMeshDefaultLoc/Rot/Scale`.
      - Volver a activar el tick del CMC. Si **no** la llevan, `MOVE_Falling`.
      - En el servidor, `SetReplicateMovement(true)`.
- **`UTN_CarryComponent`**:
  - `ServerGrab`: `ForceEnterShell(false)`, luego `StopBody()` (si estaba suelta como cuerpo) y luego
    `SetExitLocked(true)` antes del attach de siempre.
  - `Release`: si la llevada está en el caparazón, `bAwaitingBounce = false`, `SetExitLocked(bThrown)` y
    `StartBody(Velocity, bThrown)`, en vez de `LaunchCharacter`.
  - `ClientApplyThrow`: no lanzar con el CMC si está en el caparazón (la caja ya llega replicada).
  - Tanto el rebote viejo (`NotifyLanded`) como la entrada al agua quedan para cuando no está en el caparazón.
- **Animación**: añadir `bool bShellBody` a `FTNTurtleAnimFrame` (el personaje expone si el caparazón tiene cuerpo).
  Con cuerpo **no** se aplica el `Lift(-19)` del caparazón, porque tumbada ese eje apunta hacia delante.
- Riesgos: el auto-caparazón al caer de altura (`TickFallRules`) ahora dará un cuerpo que cae con física, y no habrá
  muerte por caída mientras esté en el caparazón (es lo esperado). La muerte y el derribo llaman a `ForceExitShell`,
  que ya pasa por `StopBody`.

### P2. Derribo: ragdoll, quedarse quieta, levantarse con animación y pajaritos
Pedido del usuario: «trata de hacer una animación para que se note más natural el que te levantes, porque ahora queda
muy tieso que te caigas y te levantes al instante… cuando te noqueen (que no es lo mismo que te maten o que te hagan
meterte en un caparazón) te ocurre lo mismo: activa el ragdoll y te quedas tieso ahí un momentito, y los pajaritos
dando vueltas con sus soniditos y unas cuerdas de fondo, exactamente igual que en los dibujos animados, mientras estás
noqueado».
- Comprobar que **todas** las fuentes de derribo (plátano, golpes, tinta…) pasan por la ruta del ragdoll de
  `ApplyKnockdownVisual` y que el cuerpo se queda quieto en el suelo el tiempo del derribo antes de levantarse.
- **Levantarse** (en `UTN_TurtleAnimInstance`):
  1. Justo antes de apagar la física (`ApplyKnockdownVisual(false)`), guardar la transformación en mundo de todos los
     huesos.
  2. Con la malla ya devuelta a la cápsula, pasarlas al espacio del componente nuevo y de ahí a locales (la cadera
     respecto del componente).
  3. En `Evaluate`, mezclar desde esa pose hacia la pose normal durante unos 0,7 s con una curva suave. Añadir una
     capa corta de «flexión» al principio: brazos empujando el suelo y rodillas dobladas.
  - Así la tortuga gira desde el suelo hasta ponerse de pie en vez de aparecer de golpe.
- **Pajaritos**: `UTN_DizzyBirdsComponent` (agente; apartado 3). Si no llega, rehacerlo con esa misma especificación.

### P3. HUD: textos pegados arriba y contador de conchas
Pedido del usuario: «en la señal de arriba (sala, zona), en los bocadillos cuando mando un mensaje u otra gente, o
hablo por voz, y en el número de conchas, el texto está demasiado pegado arriba, sin margen, muy apretujado; hazlo un
pelín más grande… el contador de conchas debe ser dinámico para estirarse cuantas más tenga y que no atraviesen los
números».
- Cartel «Sala: n/n | Zona: n/n»: `Private/UI/HUD/TN_CoopFlowHUDWidget.cpp` (hacia la línea 504) y su tarjeta.
- Bocadillo de chat: `Private/UI/HUD/TN_RunHUDWidget.cpp`, `MakeChatBubble`, relleno `FMargin(22, 11, 14, 20)` y
  `ChatBubbleMargin`. El margen de arriba es el que falta.
- Bocadillo de voz: `MakeTalkBubble`.
- Contador de conchas: `ScoreText` dentro de `MakeCard(... SandTagTexture ..., FMargin(66, 16, 30, 16))`. Que la
  tarjeta crezca con el número (sin SizeBox de ancho fijo) y con los márgenes de la textura bien puestos.
- Subir un poco la letra y el margen superior en los cuatro.

### P4. Lobby como castillo de arena (el grueso de lo que queda)
Pedido del usuario, textual: «todo lo que es la lobby tiene pensado ser un castillo de arena: todas las formas que tú
ves son un castillo de arena alrededor, las puertas como si fuera un castillo de arena con una puerta enorme que se
abre y entras a una sala de espera en la que te preparas para ir a la batalla, que es como meterte en los huevitos de
los que luego sales cuando spawneas… pon el conjunto de huevos en el centro; las botellas como te he comentado; los
jugadores a la derecha; la tienda a la izquierda, donde está; a la derecha un general con un mini cuartel en una mesa
de madera… y en el resto del mapa un mini parkour por todo el castillo de arena, zonas y cosas para practicar los
saltos, con elementos del paisaje, dentro de este espacio cerrado, para que se diviertan mientras llegan los compis».

Propuesta: un actor procedural `ATN_SandCastleLobby`, que coloca `ATN_HQGameMode` como hace con la tienda, y que
esconde las piezas de la maqueta que sustituye. Lleva:
- **Murallas** de arena con almenas alrededor de todo el recinto (sustituyen visualmente a «Extrude»), con marcas de
  cubo y conchas incrustadas.
- **Torres** de cubo en las esquinas y a media muralla.
- **Puerta enorme** de castillo en la zona de salida (y ≈ 2199), que se abre al empezar la cuenta atrás.
- **Sala de espera**: la zona de salida con los **huevos**. Cada jugador se mete en uno para estar listo. Hay que
  reutilizar o ampliar `TN_LobbyReadyZone` y `ATN_HQGameMode::SetPlayerReadyState`, y dejar el lugar de los huevos de
  la maqueta (0, -250) o moverlos dentro, según quede mejor.
- Tienda a la izquierda (+X) y general y probadores a la derecha (-X), como ahora.
- **Mini parkour**: escalones de arena, pilares de cubo, una pasarela de palos de polo, una rampa de concha y
  plataformas sobre las murallas. Sirve para practicar el salto y el panzazo mientras esperan.
- Todo con el kit de mallas (`TN_ProcMapMeshKit.h`, `TNProcRuntimeMesh::MakeStaticMesh`) y colisión, porque sobre el
  parkour hay que poder andar. Para la colisión basta con una malla con `bBoxCollision` o con UBoxComponent aparte,
  como hacen `CounterBlock` y `Walls` en el puesto y el probador.

### P5. Pantalla de carga del huevo, general y pajaritos
Lo de los agentes del apartado 3: integrar, compilar y probar. Si no ha llegado, rehacerlo con esa especificación.

### P6. Ojos de verdad y tipos de ojo (cosmético)
Pedido del usuario: «los ojos se ven igual que la piel… ponle distintos tipos de ojos (van a tener animaciones), por
darles un color u otro al interior del ojo o de una manera u otra».
- Hoy `M_TurtleBody` pinta toda la ranura `lambert4`, ojos incluidos, con el color del cuerpo. Hay que añadir una
  máscara de ojos por posición antes del skinning, igual que la del caparazón. Los vértices están en
  `Scripts/tools/data/turtle_geo.json` para localizar las esferas de los ojos: delante de la cabeza, y > 14 y z entre
  unos 42 y 52.
- Nueva categoría de cosmético «Ojos»: `ETNCosmeticCategory::Eyes`, filas en `DT_Skins` o en una tabla nueva,
  `FTN_TurtleLook::EyesId`, replicación en el PlayerState, pestaña o fila en tienda y probador y guardado en el
  SaveGame.
- Tipos: iris de colores, pupilas de estrella o de corazón, ojos de «dibujo», brillo…

### P7. Pendiente del mapa procedural (de antes; ver `Docs/Mapa_Procedural_Traspaso.md`)
- Obstáculos obligatorios de parkour:
  - Puentes rotos que obliguen a rodear.
  - Tramos hundidos en los puentes colosales, con pasos estrechos, saltos, vigas o repisas.
  - Huecos que obliguen al salto y al panzazo.
- Probar la poza, la fauna y el sonido ambiente en PIE.
- Crear el nivel `LVL_ProcMap_Terrain`: duplicar `LVL_ProcMap` y poner el GameMode `ATN_TerrainViewGameMode`. Lo hace
  el usuario en el editor.

### P8. Documentación
Al cerrar cada tema, actualizar `Docs/Tienda_Probador.md`, `Docs/Animacion_Tortuga.md` y `Docs/Mapa_Procedural.md`, y
crear docs nuevos si hace falta (lobby castillo, carga, general).

---

## 5. Referencias técnicas rápidas

- **Kit de mallas en ejecución** (`Private/World/ProcMap/TN_ProcMapMeshKit.h`):
  - `AddTri` y `AddQuad(A, B, C, D, Hint, Color)` orientan la cara visible hacia `Hint`. En UE la cara frontal de
    (A, B, C) es la de normal (C−A)×(B−A).
  - Normales planas; para curvas lisas, ver `SmoothQuad` en `TN_ChangingBooth.cpp`, con normales por vértice.
  - `TNProcRuntimeMesh::MakeStaticMesh(Outer, Buffers, Material)` decodifica el color de vértice una vez: se le pasan
    colores de `Pal()` ya decodificados una vez.
  - Material de color de vértice: `/Game/Cosmetics/Materials/M_CosmeticVertexColor`.
- **Interactuables**: heredar de `ATN_DirectInteractableBase`, sobrescribir `OnInteracted_Implementation` (servidor) y,
  si el actor es grande, `GetInteractionPoint()`. Los menús se abren con una RPC cliente del PlayerController
  (`ClientOpenShop` / `ClientOpenBooth`).
- **Música**: `UTN_MusicSynthComponent::AttachMusic2D(PC)->PlayTrack(ETNMusicTrack::Shop)`. Se pueden añadir pistas
  nuevas (lobby, cuartel del general) en `TN_MusicSynthDSP.h`, en el namespace `TNMusic`, con el mismo formato.
- **Sonido ambiente y síntesis**: `Audio/TN_AmbientSynthComponent.*` es el patrón de `USynthComponent`
  (Init/CreateSoundGenerator).
- **HUD**: estilos en `Private/UI/HUD/TN_HUDStyle.h`, arte en `TN_HUDArt.h` y caras en `TN_HUDFaces.h`. Todo se crea
  en código, sin UMG en el editor.
- **Stamina**: `UTN_StaminaComponent` (`GetWalkSpeed`, `IsSprinting`, `SetSpeedCap`/`ClearSpeedCap`; es el único punto
  que escribe `MaxWalkSpeed`).
- **Llevar y lanzar**: `UTN_CarryComponent`. Se puede coger a quien está en el caparazón o derribado; el lanzamiento
  usa `ThrowSpeed` y un ángulo por la cámara.
- **Caparazón**: `UTN_ShellComponent`. `bIsInShell` está replicado, las condiciones van en `TN_ShellDecisions.h`,
  hay `SetExitLocked` y el frenado va por el tope de velocidad.

---

## 6. Qué tiene que probar el usuario del build actual (commit `2b3026e`)

1. **Andar** sin sprint: los pies no patinan.
2. **Sprint**: entra la carrera, con codos doblados y zancada larga con fase en el aire. En el primer intento no
   entraba porque el sprint del lobby es lento; ya está corregido y **pendiente de confirmar**.
3. **Panzazo**: tumbada sobre la tripa, algo aplastada, remando con los brazos y pataleando con los pies en el suelo.
4. **Emotes**:
   - WAZAAA: la mano por fuera de la cabeza.
   - HAPPIE: palmadas encima de la cabeza.
   - PARACOPTER: más rápido.
   - SAX-O: las manos juntas en el centro.
   - RUN: los brazos atrás.
   - SUPERKIRK: todo el cuerpo a 45°.
5. **Caparazón**: cabeza, brazos y patas se meten dentro y el cuerpo baja al suelo. La física es la P1.
6. **Probador**:
   - El tapón pegado a la botella.
   - La etiqueta «PROBADOR» sobre la curva.
   - El techo con 5 pies.
   - El vidrio liso.
   - Al entrar suena la música del probador.
7. **Tienda**:
   - La radio del puesto suena cerca y se atenúa al alejarse.
   - Al abrir la tienda suena su música y la radio baja; al cerrar vuelve.
8. **Aviso de interacción**: con el tendero y con el probador, el aviso aparece justo cuando pulsar funciona. Antes
   salía antes de estar a tiro.
9. **Plátano**: ragdoll con impulso (ya confirmado).

# B · Buggy de HellYeah en Tortunabo: port, asientos y sincronización

Base: HellYeah `feat/f0-f1` (b552481) y Tortunabo `main` (41a0ee8a5), los dos con UE 5.6. Análisis de solo lectura. Lo marcado como *inferido* no se ha comprobado en ejecución.

## 1. Plan de port

**Código.** Renombrados: `HY`→`TN_`, `HELLYEAH_API`→`TORTUNABO_API`, `LogHellYeah`→`LogTortunabo`, `hy.Buggy.Telemetry`→`tn.Buggy.Telemetry`. Todo va a `Public|Private/Vehicles`.

- `HYBuggy` (239/751 líneas) → `ATN_Buggy`:
  - Fuera `Cargo`, `CargoCollision`, la soldadura de `BeginPlay` y el include de fichas.
  - El asiento del pasajero sale de las constantes del suelo de la caja.
  - El `.cpp` se parte en dos (el de entrada aparte).
- `UTN_BuggyData`: sin los campos `Spill*`. Se queda `RunOverSpeed`.
- `Wheel`, `BumpMath`, `DriftMath` y `BoostMath` se copian tal cual, con sus specs.
- `BuggySpec`: se quitan los 3 casos de carga (líneas 105–137).
- `HYSpikeDriver/Probe`, `buggy_measure.ps1` y `playtest.ps1` se portan sin fichas, sobre un mapa de prueba.

**Rutas.** Las rutas fijas del constructor (`SkeletonPath`, `BodyMeshPath`, `DataPath`, `InputRoot`) pasan a `UPROPERTY(EditDefaultsOnly)` en `BP_TN_Buggy`. `FindGeneratedAsset` se conserva solo para el modo headless.

**Assets** (con *Migrate* del editor, para conservar referencias):

- `SKM_Offroad` con su PhysicsAsset y Skeleton, `Offroad_AnimBP`, `Offroad_CtrlRig` y los `MI_OffroadCar_*` que arrastran.
- `SM_BuggyBody` y `SM_BuggyTire`, regenerados con `buggy.py`. Sin `SM_BuggyCargoCollision`.
- `DA_Buggy`, `IMC_Buggy` y los 8 `IA_*`.
- El IMC del buggy entra con prioridad mayor y el de la tortuga sale al poseer el buggy: Ctrl ya es el caparazón.

**Build.** Plugin `ChaosVehiclesPlugin` en el `.uproject`. `ChaosVehicles` y `ChaosVehiclesCore` como dependencias públicas, porque la cabecera expone `AWheeledVehiclePawn`. `NetCore` no hace falta.

**Choque de configuración de física (verificado).** `DefaultEngine.ini` de Tortunabo no tiene `[/Script/Engine.PhysicsSettings]`, así que usa física síncrona y sin predicción.

- `bEnablePhysicsPrediction=False`: igual que en HellYeah. No hay choque.
- `bTickPhysicsAsync=True`: sí choca. Afectaría a:
  - `ATN_ShellBody` con `PredictiveInterpolation` (`TN_ShellBody.cpp:80`) y las velocidades que se escriben desde el game thread: catapulta (`TN_BeachCatapult.cpp:743`) y trampolín (`TN_BeachTrampoline.cpp:717`). Irían un paso por detrás y cambiaría el ajuste.
  - El ragdoll congelado (`TortugaCharacter_Knockdown.cpp:917-960`), que pasaría a leer una pose interpolada.
  - Las tortugas de pie sobre `TN_PhysicsObjectActor`, que temblarían más.
  - Los `OnComponentHit`, que llegarían más tarde.
- No he encontrado en el código una dependencia dura de la física asíncrona. El motivo probable es el paso fijo de 60 Hz (*inferido*).
- **Recomendación:** física síncrona con `bSubstepping=True` (`MaxSubstepDeltaTime=0.016667`) y medir con `buggy_measure`. Solo si el manejo no cuadra, pasar a asíncrona y repasar Player, Knockdown, Caparazón y Objetos físicos en el checklist de playtest.

## 2. Asientos para tortugas

**Carrera: el pawn es el buggy.** La tortuga del asiento es solo visual: un `USkeletalMeshComponent` al que `UTN_CosmeticLook` aplica el `FTN_TurtleLook` del PlayerState. No hay cambio de posesión, no se pierde el PlayerState y morir es reaparecer en el checkpoint.

**Coop, con `ATortugaCharacter` real:**

- **Estado:** `Seats` con `ReplicatedUsing`, escrito solo por el servidor (0 = conductor).
  - Al replicarse, cada máquina aplica el patrón de `TN_CarryComponent.cpp:457-466`: `DisableMovement`, `IgnoreActorWhenMoving` en los dos sentidos y attach al socket del asiento.
  - El servidor pone `bIgnoreClientMovementErrorChecksAndCorrection` mientras va sentada, como en `TN_BeachEnemy.cpp:1221-1225`, y lo restaura al bajar con `RestoreReleasedTurtle` (:1261).
- **Conductor:** el PC posee el buggy.
  - Riesgo: `UnPossessed` borra el PlayerState de la tortuga, y el código lo lee a menudo (por ejemplo `TN_QuadActor.cpp:147`). Mientras conduce, los peligros deben golpear al buggy.
  - `PawnLeavingGame` debe soltar el buggy para que una desconexión no lo destruya.
- **Pasajeros:** siguen poseyendo su tortuga y su cámara. Van en la caja metidos en el caparazón sin física (`ForceEnterShell(false)`). Un choque por encima de un umbral los expulsa con `StartBody(velocidad del buggy)`.
- **Bajar:** barrido de la cápsula en 3 puntos; si ninguno está libre, se baja por arriba.
- **Caparazón y agarre:** sentada no puede alternar el caparazón a mano, no se la puede coger, y la gaviota y el pelícano deben descartarla.
- **Muerte:** `ATN_DeathZoneVolume` solo mira `ECC_Pawn`, así que el chasis (perfil Vehicle) no dispara nada. El servidor tiene que comprobar la caja del mar (`kill_boxes_uu`) y el KillZ en el buggy, expulsar a todas y luego llamar a `RequestKill`. El rescate funciona igual.
- **Vuelco:** `ServerSelfRight` ya se revalida en el servidor. Si pasa más de 4 s volcado, se expulsa a todas.

## 3. Sincronización de red

**Diagnóstico de los 2,4 m.** `spike-f0.md:136-139` compara la posición **en mundo** del buggy en el cliente con la del servidor, usando un reloj estimado, y el propio documento la llama orientativa. A 22–28 m/s, 2,4 m son 86–109 ms de retraso: la latencia de ida, el búfer de `PredictiveInterpolation`, el intervalo de red de 1/60 s y el error del reloj. Es el retraso normal de un proxy simulado, no un fallo.

Lo que falta por medir:

- las correcciones que sufre el conductor;
- los choques entre buggies (el rival se ve 1–2,4 m por detrás de donde está).

**Opciones:**

| Opción | Veredicto |
|---|---|
| A. Actual: servidor con autoridad, entradas por `ServerUpdateState`, `PredictiveInterpolation` | Funciona. Vale para coop |
| B. Predicción de física de Chaos con resimulación | Beta. Exige física asíncrona, resimula 8 vehículos en el anfitrión y en HellYeah dejó sin entradas al cliente. Descartada |
| C. Mover 2.0 | Experimental y sin vehículos Chaos. Descartada |
| D. Suavizado en el cliente | `PredictiveInterpolation` ya extrapola. Solo quedan ajustes de cvars |
| **E. Autoridad del cliente conductor con validación del servidor** | **Recomendada para la carrera de 8** (20–30 h) |

Cómo sería E:

- El dueño simula y envía su estado (posición, rotación, velocidades y marca de tiempo) a 30 Hz por RPC no fiable.
- El servidor valida la velocidad máxima, el salto máximo por paso, el orden de los checkpoints y un barrido contra el terreno. Después lo aplica en cinemático y lo replica con extrapolación.
- Choques entre buggies: el servidor manda el impulso a los dos dueños por RPC `Client`.
- Resultados, ítems y muertes siguen en el servidor.

**Ancho de banda (*estimado*).** Unos 85 B por actualización a 60 Hz dan ≈5 KB/s por buggy, lo que cuadra con los 5 KB/s que midió HellYeah.

- Cada cliente recibe ≈40 KB/s, por debajo del límite de 200 KB/s.
- El anfitrión sube 7×40 ≈ **280 KB/s (2,2 Mbit/s)**. Es demasiado para un listen server doméstico.
- Con 30 Hz cerca y 10 Hz lejos baja a ≈100–140 KB/s.
- Como `net.UseAdaptiveNetUpdateFrequency=1` está activo, hay que llamar a `ForceNetUpdate` en choques, boost y enderezado.

## 4. Auditoría de sync de mecánicas

| Mecánica | Autoridad y riesgo | Arreglo |
|---|---|---|
| Catapulta `TN_BeachCatapult.cpp:325, 401-402, 733-745, 1070` | Tiempos del servidor y animación con reloj suavizado; la bola se lanza con la velocidad del servidor. **Bajo**: el temblor ya es solo visual (:1060) | Nada |
| Trampolín `TN_BeachTrampoline.cpp:640-671, 683, 846-848` | `LaunchCharacter` en el servidor y en el dueño a partir de un hit o solape. **Medio**: si el solape no cae en el mismo movimiento en las dos máquinas, hay una corrección de más de 1000 cm/s | Detectar el rebote dentro del CMC o dar tolerancia tras el rebote |
| Plataforma móvil `TN_BeachMovingPlatform.cpp:208-232, 600` | Determinista con el reloj del servidor (`TN_BeachTrapCommon.cpp:83-98`); la base tiene nombre estable. **Bajo** | Nada |
| Plataforma tambaleante `TN_BeachWobblyPlatform.cpp:405-470` | Muelle local con las posiciones que ve cada máquina; solo se replica `BrokenAt` (:297). **Medio**: hasta 9,8° (:467) sobre 110–150 cm dan 19–25 cm de desfase en el borde de una base de movimiento | El servidor replica alabeo y cabeceo en int8 a 15 Hz y el cliente aplica un muelle hacia ese valor |
| **Puente tambaleante** `TN_WobblyBridge.cpp:40-51, 331-345, 754-767, 796` | Solo se replica la configuración. `Excitation` y `Dips` son locales y mueven la colisión de los tablones. La agitación de hasta 2,4 multiplica un vaivén de 24 cm y 22° (tope 58°). **Alto**: tablones distintos en cada máquina, con correcciones y caídas | `Excitation` del servidor, replicada en uint8 a 10 Hz; los `Dips` solo en la malla visual |
| Quad `TN_BeachQuadLane.cpp:223-302, 341` | El servidor decide con la posición que él tiene de la tortuga, y el dueño va 25–40 cm por delante. El margen es de +45 cm (:249) y la parte visual usa `ServerNow` sin suavizar. **Bajo-medio** | Compensación de lag; `FTNTrapClock` en lo visual |
| `ATN_QuadActor` `TN_QuadActor.cpp:18, 133` | `SetActorLocation` sin barrido: puede atravesar sin detectar solapes. **Bajo** | Retirarlo en favor de QuadLane |
| Gaviota `TN_BeachGullZone.cpp:1141-1266`, sujeción `TN_BeachEnemy.cpp:1174-1407` | La tortuga se coloca de forma determinista en cada máquina, sin movimiento, sin suavizado y sin correcciones. **Bajo**; sirve de plantilla para los asientos | Nada |
| Minas `TN_BeachMine.cpp:522-523, 584-587` | Empujón en el servidor y **otra vez** en el dueño al llegar el multicast. **Medio (*inferido*)**: corrección y segundo empujón | Quitar el empujón local y verificar con `p.NetShowCorrections 1` |
| Ítems de carrera `TN_RaceItemComponent.cpp:196-211` | El multiplicador se aplica en `OnRep` pero no viaja en `FTNSavedMove_Turtle` (`TN_TurtleMovementComponent.cpp:66-127`). **Medio**: corrección de ≈30 cm al empezar y al acabar | Meter el multiplicador en el movimiento guardado o predecir el boost al usar el ítem |
| Pelícano taxi `TN_RacePelicanTaxi.cpp:797-857` | Mismo patrón de sujeción; el cliente anticipa la suelta (:808). **Bajo** | Nada |

## 5. Terreno para buggies

**Requisitos** (rueda de 0,51 m, batalla de 3,03 m):

| Parámetro | Valor |
|---|---|
| Pendiente | ≤12° sostenida, ≤20° en tramos cortos |
| Peralte lateral | ≤5° (8° en curvas) |
| Escalón | ≤0,4 m máximo, ideal ≤0,1 m |
| Radio de cresta | ≥v²/g: 23 m a 54 km/h, 75 m a 97 km/h |
| Radio de curva | ≥25 m a 60 km/h, ≥50 m a 90 km/h |
| Ancho de pista | ≥12 m (16 m en la salida) |

**E01 medido** con un script de solo lectura sobre `global_top(build_all(SpainModel))`, en muestras de 1 m:

- La tierra ocupa el **31 % de los 600×600 m**.
- Pendiente p50 **18°**, p75 31°, p90 45°. Solo el 54 % está por debajo de 20°.
- El 38 % de los saltos entre muestras vecinas supera 0,4 m.
- Roncesvalles→Santiago: **287 m** en línea recta, con pendiente p90 de 42°. A 90 km/h son **11 s**.

**Conclusión:** E01 no se puede conducir y se queda corto.

**Variante `E01B`:**

1. Exageración del relieve de ×20 a ×5–8.
2. Escala 1 m = 0,5–0,7 km: de 324 a 576 trozos, con streaming.
3. Corredor tallado con un spline por el Camino: gaussiano de σ 8–10 m, pendiente ≤12°, 12 m de pista y 10 m de arcén.
4. Validador del spline más un piloto IA en headless que cuente atascos y vuelcos.

Alternativa más barata: buggy a escala de tortuga (1,6 m, 40–50 km/h) en un circuito a vueltas.

## 6. Tareas ordenadas

| # | Tarea | h | Riesgo |
|---|---|---|---|
| 1 | Port del núcleo (C++, plugin, assets, IMC) | 6–8 | Referencias rotas |
| 2 | Tests y herramientas de medida | 3–4 | — |
| 3 | Subpasos o física asíncrona, y medir el manejo | 4–6 | Regresión en ragdoll y caparazón |
| 4 | Pawn buggy en carrera, con la tortuga visual | 8–12 | — |
| 5 | Muerte y reaparición (mar, KillZ, canal Vehicle) | 4–6 | Muertes silenciosas |
| 6 | Sync: puente 4, plataforma 3, boost 4, mina 2, trampolín 4, quad 2 | 19 | Puente |
| 7 | Terreno `E01B` y validador | 10–14 | Streaming |
| 8 | Red con 8 buggies: sonda de correcciones, 30/10 Hz | 6 | Subida del anfitrión |
| 9 | Opción E | 20–30 | Choques entre buggies |
| 10 | Asientos en coop | 12–16 | PlayerState nulo |

## 7. Skins y pulido del buggy

**Skins:**

- `FTN_BuggySkinData` en `DT_BuggySkins`: Id, precio, icono, `Color`/`Color2`, `Shine` y `Pattern`.
  - `Pattern` reutiliza `ETNShellPattern` (escamas, olas, sandía): la temática de tortuga sale gratis.
  - Llantas y decoración como `TSoftObjectPtr<UStaticMesh>` más su socket.
- Categorías nuevas `BuggyPaint`, `BuggyWheels` y `BuggyDeco` en `ETNCosmeticCategory`.
- `FTN_BuggyLook` se replica en el PlayerState junto a `FTN_TurtleLook`.
- El `Tint` de HellYeah pasa a ser un `PaintId` replicado, con copia en el buggy para cuando va sin conductor.
- `UTN_CosmeticSaveGame` suma `UnlockedBuggyIds` y los equipados.
- `M_BuggyPaint` con los parámetros de `M_TurtleBody`, para que lo pinte `UTN_CosmeticLook`.
- Pestaña «Buggy» en `UTN_ShopWidget` y `BP_BuggyStatue` en el probador.
- Coste: datos, save y réplica 5–6 h; material 3–4 h; tienda 6–8 h.

**Pipeline de Blender (`buggy.py`)**, 6–8 h más 1–2 h por variante:

- `build(manifest, variant)` parametrizado.
- Llanta separada del neumático.
- Decoración (techo-caparazón, aletas, mascarón) con empties `SOCKET_Deco_*`.
- Ranuras fijas: `M_BuggyPaint`, `M_BuggyTrim`, `M_BuggyRubber` y `M_BuggyRim`.
- UV por proyección de caja.
- `validate.py`: carrocería ≤3k triángulos, decoración ≤800 y sin invadir las ruedas.
- Iconos con `render_preview.py`.

**Pulido, por prioridad:**

| # | Qué | h |
|---|---|---|
| 1 | Colisiones: el chasis ignora `ECC_Pawn` y el servidor atropella (`KnockDownTurtle`/`StunTurtle`) por encima de `RunOverSpeed`; rebote arcade entre buggies | 6–8 |
| 2 | Conducción: fricción por material (arena seca, mojada), estabilización en el aire y anti-vuelco | 8–12 |
| 3 | Enderezado automático a los 4 s y reaparición al caer al agua | 4–6 |
| 4 | Cámara: mirar atrás, yaw por velocidad en los saltos y sacudida en los impactos | 4–5 |
| 5 | Audio: motor sintetizado según RPM, derrape, impactos, bocina (`OnHorn` está vacío) y boost; en C++, sin BIE | 8–10 |
| 6 | VFX: polvo según patinaje (`TN_TurtleDustComponent`), salpicaduras con `TN_Water`, estela del boost y marcas | 6–8 |
| 7 | HUD: velocímetro, boost y posición | 3–4 |

# Tienda y probador del lobby

Sistema real de cosméticos de Tortunavy: la tienda de Don Tortugo (catálogo y compra) y los probadores de botella
(elegir lo desbloqueado y ponérselo). Sustituye al catálogo de «pulsar y aplicar» y a las estatuas de prueba de
`LVL_HQ` (`BP_SkinStatue`, `BP_HatStatue`), que quedan como prototipos.

## Flujo

1. **Tienda** (`ATN_ShopKeeper`). Al hablar con el tendero se abre `UTN_ShopWidget` en el cliente que interactúa:
   a la izquierda, tu tortuga posando y girando en una peana (arrastrar con el ratón la gira); a la derecha, el
   tendero habla en su bocadillo, tres pestañas (cascos, caparazones, colores) y el catálogo con miniaturas y precio.
   Al elegir algo, la tortuga se lo prueba encima de lo que lleva y saluda. **Comprar** desbloquea el cosmético
   (hoy todo cuesta 0 conchas) y lo guarda en el `SaveGame` local; el servidor recibe la lista de desbloqueados
   (`ServerSyncUnlockedHelmets` / `ServerSyncUnlockedSkins`).
2. **Probador** (`ATN_ChangingBooth`): media botella de cristal de mar puesta boca abajo (el culo con su hundido hace
   de techo) cuya puerta redonda es el tapón, una chapa de corona roja con estrella. Al entrar, el servidor mete a la
   tortuga dentro y cierra la chapa (`Occupant` replicado: todos la ven cerrarse y la botella se menea mientras se
   cambia). El cliente aleja la cámara
   a la de la botella y abre `UTN_BoothWidget`: tres filas (casco, caparazón, color) que se cambian con las flechas;
   solo salen los cosméticos desbloqueados. **¡Listo!** pide al servidor los cambios (`RequestEquipHelmet`,
   `RequestEquipShell`, `RequestEquipSkin`); **Cancelar** sale sin cambios. En los dos casos la puerta se abre y la
   tortuga sale de un saltito (el servidor y el cliente dueño la lanzan a la vez).
3. **Replicación del aspecto.** El `PlayerState` replica `EquippedHelmetId`, `EquippedShellId` y `EquippedSkinId`; cada
   `OnRep` llama al personaje, que guarda el conjunto (`FTN_TurtleLook`) y lo aplica entero con `UTN_CosmeticLook`.

Teclado y mando: flechas o WASD para elegir, Q/E (o gatillos) para las pestañas, Intro para comprar o aceptar y
Escape para salir. El menú bloquea el control del juego mientras está abierto (`FInputModeUIOnly`).

Al acercarse a cualquier interactuable (tendero, probador, selectores, objetos), el HUD enseña abajo un cartel con la
tecla de la acción de interactuar (la que tenga asignada en Enhanced Input) y el texto del interactuable
(`UTN_RunHUDWidget::TickPrompt`). Consola de pruebas: `TNShop` abre la tienda y `TNBooth` entra en el probador libre
más cercano.

## Clases

| Clase | Archivo | Qué hace |
|---|---|---|
| `UTN_CosmeticLook` | `Core/TN_CosmeticLook` | Viste a una tortuga con un `FTN_TurtleLook`: casco en la cabeza, color y caparazón. Lo usan el personaje, el tendero y las vistas previas. |
| `ATN_CosmeticPreview` | `Lobby/TN_CosmeticPreview` | Escaparate local (no se replica): tortuga en una peana con luces de estudio y cámara que pinta en una textura. Hace las miniaturas del catálogo. |
| `ATN_ShopKeeper` | `Lobby/TN_ShopKeeper` | Tendero con su conjunto; puesto con mostrador, toldo de rayas con volante, cartel de pie y guirnalda de banderines. Se gira hacia el jugador local y le saluda. |
| `ATN_ChangingBooth` | `Lobby/TN_ChangingBooth` | Media botella boca abajo con la chapa de puerta; mete y saca a la tortuga. |
| `UTN_ShopWidget`, `UTN_BoothWidget` | `UI/Shop/TN_ShopWidgets` | Pantallas de la tienda y del probador, hechas en código con el estilo del HUD. |

`ATN_HQGameMode::SpawnLobbyShops` coloca la tienda y los probadores si el nivel no los trae puestos. Busca primero
actores con la etiqueta `TN_ShopAnchor` o `TN_BoothAnchor`; si no hay, usa la maqueta de `LVL_Lobby`: el tendero es la
tortuga grande (`SkeletalMeshActor` con `TotugaDemo_Rig` y escala ≥ 3,2) más cercana a la carpa, y los probadores son
las botellas `BP_VestidorBotella_*` (la puerta mira a la puerta de prueba `BP_ShellDoor` si hay una al lado y, si no,
al `PlayerStart`). Las piezas de maqueta sustituidas se esconden en cada máquina. Para colocarlos a mano basta con
arrastrar `TN_ShopKeeper` o `TN_ChangingBooth` al nivel.

## Contenido

`Scripts/build_cosmetics.py` (se ejecuta dentro del editor) crea o rehace:

- `/Game/Cosmetics/Materials/M_CosmeticVertexColor`: cascos y mallas del puesto (color de vértice; el alfa es el brillo metálico).
- `/Game/Cosmetics/Materials/M_TurtleBody`: cuerpo y caparazón de la tortuga de demo (ver abajo).
- `/Game/Cosmetics/Materials/M_TurtleHelmetSlot`: recorta el casco rojo de serie y deja la lengua.
- `/Game/UI/Shop/M_UI_Preview`: pinta en la UI las capturas del escaparate.
- `/Game/Cosmetics/Helmets/SM_Helmet_*`: los doce cascos, modelados en `Scripts/cosmetics_meshes.py` (Python puro)
  sobre la coronilla de la tortuga: sombrero de paja, tricornio, corona, gorra de capitán, gorro de marinero, gorro de
  fiesta, gorro de hélice, flor tropical, estrella de mar, cangrejo, sombrero medusa y aureola.
- Las filas de `DT_Helmets` y `DT_Skins` (diez caparazones y doce colores), con precio 0 y la frase del tendero.

### La tortuga de demo

`TotugaDemo_Rig` tiene dos ranuras de material: `lambert2` (casco rojo de serie y lengua) y `lambert4` (cuerpo, ojos
y caparazón juntos). Por eso el color y el caparazón se pintan con un solo material, `M_TurtleBody`, que separa las
zonas por la posición local antes del skinning (unidades de la malla; mira a +Y y mide unos 53 de alto):

- **Caparazón:** detrás del torso (`y` menor que un frente que va de 0,4 a 3,2 según la altura), entre `z` 22,3 y 38,4
  y con `|x|` < 7,2. Coincide con la pieza del caparazón de la malla.
- **Barriga:** delante del torso, entre `z` 21 y 36,6.
- **Lengua** (en `M_TurtleHelmetSlot`): `|x|` < 2,3, `y` > 9,6 y `z` entre 39,8 y 43,4; el resto de la ranura es el
  casco de serie y sus correas.

El casco va en el socket `Sombrero` de la malla si existe; si no, en el hueso `Head`, colocado en la coronilla de la
postura de referencia (0; 5,5; 51) para que siga la animación de la cabeza. Con la malla unificada de cinco ranuras
(barriga, brillo de ojos, ojos y boca, piel, caparazón) se usan los materiales por ranura de las filas.

## Añadir un cosmético

- **Casco:** añadir una receta en `Scripts/cosmetics_meshes.py` (lista `HELMETS`) y volver a ejecutar
  `Scripts/build_cosmetics.py`; o crear una fila en `DT_Helmets` con una malla de arte (`DisplayMesh`) y ajustar
  `MeshOffset`, `MeshRotation` y `MeshScale` desde la coronilla.
- **Caparazón o color:** añadir una fila a `SHELLS` o `BODIES` en `Scripts/build_cosmetics.py` (o directamente en
  `DT_Skins`) con `Category`, `Color`, `Color2`, `Pattern`, `PatternScale`, `Shine` y `Glow`.

El precio (`Price`) ya se descuenta de las conchas acumuladas (`AccumulatedRaceScore`); la economía de conchas o
estrellas queda para más adelante.

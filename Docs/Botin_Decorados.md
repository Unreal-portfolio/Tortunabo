# Botín en los decorados del mapa procedural (mantener E para rebuscar)

En el mapa procedural se puede rebuscar en muchos de los decorados que hay en mitad de los caminos: estatuas,
cabezas de piedra, la tortuga colosal, rocas grandes, barcas, carros, cajas, tinajas, restos de barcos y de guerra...
Se mantiene pulsada la tecla de interactuar poco más de un segundo; al terminar, con suerte, sale un objeto de los de
siempre de un saltito («¡puf!»); si no, una nubecilla del color del suelo del bioma («¡pof!»).

## Cómo se juega

1. Al acercarse al borde de un decorado buscable (por cualquier lado, a 3,5 m como mucho) sale el aviso
   **«Mantén para rebuscar»** con la tecla dentro de un aro vacío. Los buscables tienen alguna chispita dorada al pie
   cuando la cámara está a menos de 18 m.
2. Al **mantener E** el aro se llena en dorado (1,3 s), saltan tierra y piedrecitas del borde hacia la tortuga y suena
   un rebuscar de arena y chinitas (lo oyen los que estén cerca). **Soltar antes cancela** (y se puede volver a
   empezar). También se corta si la tortuga se aleja del decorado, se mete en el caparazón, queda tumbada, muere o la
   cogen en brazos.
3. Al completarse, el servidor sortea: con un **55 %** sale un objeto de `DT_Items`, con una nubecilla blanca,
   chispas doradas y un «¡puf!» con campanita; el objeto asoma pequeño, da un saltito corto girando (como la tortuga
   al salir del probador, pero más cerca) y queda en el suelo a algo más de un metro, hacia la tortuga y un poco de lado.
   Se recoge como cualquier objeto (E). Si no hay suerte: nube de polvo del bioma (arena en la playa y el desierto,
   tierra en la selva y el manglar, polvo de roca en los acantilados, ceniza en el volcán) y un «¡pof!» sordo con un
   «buuu» bajito.
4. **Cada decorado se rebusca una vez para todo el grupo** (decisión de diseño): el botín queda en el suelo para quien
   lo coja, así que rebuscar es una decisión de equipo (¿quién se para?) y el mapa no se llena de objetos. Mientras
   una tortuga rebusca, a las demás no les sale el aviso. Ya buscado, el decorado no tiene aviso ni chispas.

El inventario lleno no impide rebuscar: el objeto se queda en el suelo.

## Qué decorados se pueden rebuscar

Los elige `ATN_ProcMapGenerator::SpawnSearchSpots` (`TN_ProcMapGenerator_Spawn.cpp`) entre los decorados del layout,
a **70 m como mínimo entre sí** y con una probabilidad por tipo; primero las formaciones, luego los objetos del
camino, las agujas de roca y los peñascos. Semilla propia (derivada de la del mapa): el mismo mapa da los mismos
buscables. Nada en el modo de solo terreno (`bTerrainOnly`).

| Decorado (`EFeature`) | Tipos | Probabilidad |
|---|---|---|
| Formación de explanada (`Formation`) | barco varado, cabeza de piedra, basalto, chimenea de hadas, peñasco en equilibrio, carreta, cañón, sacos terreros, búnker, torre de vigía, carro de combate, caracola gigante, ancla, círculo de piedras (su altar), obelisco, cráneo fósil, tortuga colosal, depósito de agua | 100 % (agujas de obsidiana, 70 %) |
| Objetos del camino (`PathProp`) | cajas, barriles, pacas, castillo de arena, barca, sombrilla y tumbonas, tótem, columna en ruinas, roca calavera, tinajas, cristales, mojón, vagoneta, nasas, puesto de mercado | 45–80 % según el tipo |
| Aguja o mogote de roca (`RockSpire`) | todas | 45 % |
| Peñasco (`Boulder`) | solo los grandes (radio ≥ 1,4 m) | 30 % |

Fuera: arcos sobre el camino, hitos lejanos, vegetación (troncos, secuoyas, setas gigantes), la fumarola, vallas y
filas de conos.

La **huella** de cada decorado es una cápsula en planta a lo largo de su `Dir` (radio y semilargo aproximados a su
malla: `TNSearchSpotPlan::SearchableOf`): la barca, la vagoneta, el barco, la carreta, el cañón, el carro de combate y
el cráneo son alargados; el resto, redondos.

## Arquitectura

| Pieza | Qué hace |
|---|---|
| `ATN_ProcSearchSpot` (`World/ProcMap/TN_ProcSearchSpot.h/.cpp`) | Actor ligero y replicado (dormido casi siempre) en el suelo del centro del decorado. Hereda de `ATN_InteractableBase`; sin malla ni widget 3D; una esfera invisible (`WorldDynamic`, solo consultas) que envuelve la huella para el escaneo de interactuables. |
| `UTN_SearchSynthComponent` (mismo fichero) | Sonidos sintetizados sin archivos (patrón de `UTN_PlaygroundSynthComponent`): puñado de arena y piedrecitas, «¡puf!» y «¡pof!». Se crea al primer sonido de cada decorado y se para callado. |
| `ATN_InteractableBase` | API nueva de **interacción de mantener**: `GetHoldDuration`, `BeginHoldInteract`, `EndHoldInteract`, `GetHoldProgress`, y `GetInteractionPointFor(Pawn)` (el punto del borde más cercano; por defecto, `GetInteractionPoint()`). |
| `ATortugaCharacter` | Si el interactuable al alcance es de mantener, E manda `ServerBeginHoldInteract` en vez de `ServerTryInteract`; al soltar (`Completed`/`Canceled` de `IA_Interact`) manda `ServerEndHoldInteract`. El escaneo y la validación usan `GetInteractionPointFor(this)`. |
| `UTN_HoldRingWidget` (`UI/HUD`) | Aro de progreso pintado en código alrededor de la tecla del aviso: pista azul marino con filo crema y relleno dorado. `UTN_RunHUDWidget::TickPrompt` lo enseña en los interactuables de mantener (vacío nada más pulsar, mientras llega la respuesta del servidor). |

### Red

- **Servidor autoritativo.** El cliente solo avisa de que empieza (`ServerBeginHoldInteract`, con la misma
  validación de distancia y holgura por ping que `ServerTryInteract`) y de que suelta. El tiempo lo cuenta el
  decorado en el servidor (su tick, cada fotograma mientras alguien rebusca), que además vigila que la tortuga siga a
  su alcance (+1,2 m) y en condiciones. El cliente no puede adelantar el final.
- **Estado replicado** (`FTNSearchSpotState`, un solo struct para que llegue entero): quién rebusca y desde qué hora
  del servidor (el aro de cada cliente se calcula con `GetServerWorldTimeSeconds`), el resultado y su hora, de dónde
  sale y dónde cae el objeto, y el pickup. Los efectos salen de los cambios de estado en cada máquina (`OnRep`; el
  anfitrión los aplica al cambiarlo), **sin multicast**: no se pierden, y quien entra en alcance más tarde (más de
  1,5 s después) solo ve el decorado ya buscado.
- La huella y el color del polvo se replican una vez (`COND_InitialOnly`). El actor duerme (`DORM_DormantAll`) y se
  despierta solo al cambiar de estado; vuelve a dormir 3 s después.
- **El objeto**: el servidor crea el pickup de la fila (`PickupActorClass` + `InitializeFromInventoryItem`, igual que
  `ATN_ItemSpawnZone` y soltar lo equipado) directamente en su sitio final. El saltito lo anima cada máquina con
  pantalla sobre el propio pickup (sin movimiento replicado), desde el borde del decorado hasta ese sitio, y acaba
  siempre en él; si una máquina lo recibe tarde, lo deja directamente en el suelo. Los objetos que nadie recoge se
  destruyen con el decorado al regenerarse el mapa.
- **Catálogo**: todas las filas de `DT_Items` con `PickupActorClass` y un uso (`UseType` distinto de `None`), a
  sorteo con peso 1; `LootWeights` cambia el peso por nombre de fila o `ItemId` (el tótem, que revive, va a 0,3; un
  peso 0 quita un objeto).

### Efectos

Partículas de `TNAmbientFX` (instancias de una malla, sin Niagara, nada en servidor dedicado): chispitas doradas,
nube de polvo del color del camino del bioma (el de `ResolveBiomeColors`, algo más claro; ceniza en el volcán),
piedrecitas opacas y la nubecilla blanca del «¡puf!». Los emisores de cada decorado se crean cuando hacen falta y se
quitan cuando ya no queda nada vivo y la cámara se ha ido (o el decorado ya está buscado), así que solo hay instancias
en los pocos decorados cercanos. El actor solo va a cada fotograma cuando la cámara está cerca de uno por buscar,
alguien rebusca, hay saltito o quedan partículas; si no, su tick va cada 0,3 s.

## Pruebas en PIE

1. Abrir `LVL_ProcMap` (Coop, Carrera o 2vs2) y jugar con 1 y con 2 jugadores (cliente con escucha).
2. Consola: `tn.Search.Show 1` → baliza vertical y huella de cada buscable a menos de 300 m (dorado = por buscar,
   naranja = rebuscando, verde = salió algo, gris = vacío). El log del servidor dice cuántos hay:
   `[ProcMap] Decorados para rebuscar: N de M candidatos`.
3. Ir a uno: sale «Mantén para rebuscar» al llegar a su borde por cualquier lado. Pulsar y soltar enseguida:
   el aro empieza y se corta. Mantener: aro dorado, tierra y sonido; al llenarse, «¡puf!» u «¡pof!».
4. `tn.Search.Luck 1` fuerza que siempre salga algo (`0`, que nunca; `-1`, lo normal). `tn.Search.Seconds 3` alarga
   la búsqueda para probar los cortes (alejarse, meterse en el caparazón). `TN.Debug.Interaction 1` registra en el
   servidor cada empiece, corte y resultado (`[Search] ...`).
5. Con dos jugadores: mientras uno rebusca, al otro no le sale el aviso y ve y oye la tierra; los dos ven el «¡puf!» y
   el saltito, y el objeto está en el mismo sitio para ambos. El que llega más tarde a un decorado ya buscado no ve
   efectos.
6. Regenerar el mapa (siguiente ronda): los objetos que nadie cogió desaparecen con él.

## Límites conocidos

- `IA_Interact` debe seguir con el disparador implícito (pulsada mientras se mantiene). Con un disparador *Pressed*,
  `Completed` llegaría enseguida y rebuscar se cortaría siempre.
- Si el servidor rechaza el empiece (p. ej. otro jugador empezó a la vez), el aro se queda vacío mientras se
  mantenga la tecla; al soltar y volver a pulsar se reintenta.
- La tortuga no hace pose ni meneo al rebuscar (los emotes van por el sistema de la rueda y el revive): lo cuentan la
  tierra que salta, el sonido y el aro.
- Las huellas son aproximadas a las mallas; con decorados muy irregulares el aviso puede salir algo antes o después
  del borde real.
- La densidad (70 m) y las probabilidades están en `TNSearchSpotPlan` (`TN_ProcMapGenerator_Spawn.cpp`); la duración,
  la suerte y los pesos en `ATN_ProcSearchSpot` (`SearchSeconds`, `LootChance`, `LootWeights`).

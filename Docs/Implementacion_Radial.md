# Implementacion_Radial

Guía paso a paso para terminar en **Editor/Blueprint** la implementación de las dos ruedas radiales ya soportadas en C++:

- **Rueda radial de Emotes**
- **Rueda radial de Quick Chat**

> Alcance de este documento: **solo trabajo de Editor / UMG / asignación de assets**.  
> La lógica de apertura/cierre, selección angular, validación server-side, rate limit, replicación y ejecución ya vive en C++.

---

## 0. Qué ya está resuelto en C++

No necesitas reimplementar nada de esto en Blueprint:

- `AMP_GamePlayerController`
  - abre y cierra las ruedas
  - confirma al soltar
  - usa mouse o stick
  - alimenta la rueda con un vector de navegación
- `UTN_RadialWheelWidgetBase`
  - selección angular (`atan2`)
  - deadzone
  - `SelectedIndex`
  - eventos visuales para Blueprint
- `UTN_EmoteWheelDataAsset`
  - catálogo local de emotes
- `UTN_QuickChatWheelDataAsset`
  - catálogo local de quick chat
- `ATortugaCharacter`
  - validación server-side del emote
  - cooldown server-side
  - ejecución local y replicación
  - soporte para `AnimMontage` desde el catálogo
- `ATN_CoopGameState`
  - historial replicado de quick chat
  - soporte para join-in-progress
- `UTN_CoopFlowHUDWidget`
  - emite el evento visual `OnQuickChatEntryReceived(...)` para que el HUD pinte el feed sin lógica de red en BP

---

## 1. Rutas importantes

Usa estas rutas exactas para evitar problemas con soft references ya configuradas en C++.

### Input Actions
Crear en:

`/Game/Blueprints/Gameplay/Controls/`

### DataAssets
Recomendado crear en:

`/Game/Blueprints/Gameplay/Data/`

### Widgets radiales
Recomendado crear en:

`/Game/UI/HUD/Radial/`

### Blueprints que debes editar
- `Content/Blueprints/Gameplay/Controllers/BP_GamePlayerController`
- `Content/Blueprints/Characters/BP_TortugaCharacter`
- tu HUD derivado de `UTN_CoopFlowHUDWidget`

---

## 2. Crear los Input Actions faltantes

Ir a:

`Content/Blueprints/Gameplay/Controls`

### 2.1 Crear `IA_OpenEmoteWheel`
1. Click derecho en la carpeta.
2. `Input > Input Action`
3. Nombre exacto: `IA_OpenEmoteWheel`
4. Abrir el asset.
5. En **Details**:
   - `Value Type = Boolean`

#### Importante
- **No** añadir triggers especiales.
- **No** añadir `Hold Trigger`.

#### Motivo
El C++ ya hace:
- `Started` → abre rueda
- `Completed / Canceled` → confirma y cierra

Si añades triggers de hold en el asset, cambias el timing y te complicas el flujo.

---

### 2.2 Crear `IA_OpenChatWheel`
Mismo proceso:
1. Click derecho
2. `Input > Input Action`
3. Nombre exacto: `IA_OpenChatWheel`
4. En **Details**:
   - `Value Type = Boolean`

#### Importante
- Sin triggers especiales.

---

### 2.3 Crear `IA_RadialNavigate`
1. Click derecho
2. `Input > Input Action`
3. Nombre exacto: `IA_RadialNavigate`
4. En **Details**:
   - `Value Type = Axis2D`

#### Qué hace
- En **gamepad** usa el stick
- En **mouse/teclado** el C++ calcula el vector usando cursor vs centro de pantalla

---

## 3. Configurar `IMC_Player`

Abrir:

`Content/Blueprints/Gameplay/Controls/IMC_Player`

Aquí vas a añadir los mappings nuevos.

### 3.1 Añadir rueda de emotes
Añadir un mapping para `IA_OpenEmoteWheel`.

#### Recomendación teclado
- `Q`
- o `Left Alt`

#### Recomendación gamepad
- `Gamepad Left Shoulder`

Usa un botón cómodo de mantener/soltar.

---

### 3.2 Añadir rueda de quick chat
Añadir un mapping para `IA_OpenChatWheel`.

#### Recomendación teclado
- `T`
- o `C`

#### Recomendación gamepad
- `Gamepad Right Shoulder`

---

### 3.3 Añadir navegación radial
Añadir un mapping para `IA_RadialNavigate`.

#### Recomendación gamepad
- `Gamepad Right Thumbstick 2D`

Es la opción más natural para rueda.

---

### 3.4 Qué hacer con los emotes antiguos `IA_Emote`, `IA_Emote1..9`
Tienes dos opciones:

#### Opción A — mantenerlos temporalmente
No pasa nada. Seguirán funcionando además de la rueda.

#### Opción B — dejar solo la rueda
En `IMC_Player`, quita las teclas asignadas a:
- `IA_Emote`
- `IA_Emote1`
- `IA_Emote2`
- ...
- `IA_Emote9`

#### Importante
- **No borres los assets**
- Solo quita sus mappings si no quieres accesos directos

El código del personaje aún los referencia, pero si no están mapeados, no disparan nada.

---

## 4. Crear el catálogo de emotes

Recomendado:

`Content/Blueprints/Gameplay/Data/`

Si la carpeta no existe, créala.

### 4.1 Crear `DA_EmoteWheelCatalog`
1. Click derecho
2. `Miscellaneous > Data Asset`
3. Buscar clase: `TN_EmoteWheelDataAsset`
4. Crear el asset
5. Nombre: `DA_EmoteWheelCatalog`

---

### 4.2 Rellenar `DA_EmoteWheelCatalog`
Abrir el asset.

Verás un array `Entries`.

Añadir una entrada por emote.

#### Campos por entrada
- `EmoteID` → `uint8`
- `Name` → texto visible en rueda
- `Icon` → icono UMG
- `Montage` → animación real si la tienes
- `Cooldown` → segundos

---

### 4.3 Recomendación de IDs
Usar IDs compactos consecutivos:
- `0`
- `1`
- `2`
- `3`
- `4`
- `5`
- `6`
- `7`

O hasta `9` si quieres diez emotes.

#### Recomendación práctica
Si tus emotes antiguos ya estaban asociados a `0–9`, mantén eso.

---

### 4.4 Cómo decidir el orden visual
El orden del array `Entries` es el orden que la rueda recibe.

La base C++ usa:
- `SelectionAngleOffsetDegrees = 90`
- índice `0` empieza arriba

#### Recomendación
Para la primera pasada, usa nombres que te ayuden a validar orientación:
- `0 Arriba`
- `1 Arriba-Izq`
- `2 Izquierda`
- etc.

Así sabrás rápido si el orden visual cuadra con tu rueda.

---

### 4.5 Si no tienes montages todavía
Puedes dejar:
- `Montage = None`

El sistema seguirá funcionando con el fallback actual del personaje.

Más adelante puedes asignar montages aquí sin tocar código.

---

## 5. Crear el catálogo de quick chat

### 5.1 Crear `DA_QuickChatWheelCatalog`
1. Click derecho
2. `Miscellaneous > Data Asset`
3. Buscar clase: `TN_QuickChatWheelDataAsset`
4. Crear asset
5. Nombre: `DA_QuickChatWheelCatalog`

---

### 5.2 Rellenar `Entries`
Cada entrada tiene:
- `MessageID`
- `Text`
- `Icon`
- `CooldownOverride`

---

### 5.3 Recomendación inicial de mensajes
Puedes empezar con 8 mensajes:

- `0` → `¡Vamos!`
- `1` → `Buen juego`
- `2` → `Lo siento`
- `3` → `Gracias`
- `4` → `¡Necesito ayuda!`
- `5` → `¡Cuidado!`
- `6` → `Nos vemos en la meta`
- `7` → `¡Eso estuvo bien!`

Esto encaja con la documentación actual del proyecto.

---

### 5.4 `CooldownOverride`
Si quieres que todos usen el cooldown global del controller:
- deja `CooldownOverride = 0`

Si quieres que un mensaje concreto tenga más cooldown:
- por ejemplo `4.0`

---

## 6. Crear `WBP_EmoteWheel`

Recomendado crear en:

`Content/UI/HUD/Radial/`

### 6.1 Crear widget
1. Click derecho
2. `User Interface > Widget Blueprint`
3. En la selección de clase padre, buscar: `TN_RadialWheelWidgetBase`
4. Nombre: `WBP_EmoteWheel`

---

### 6.2 Diseño recomendado
Hazlo a pantalla completa.

#### Estructura sugerida
- `Canvas Panel` raíz
  - `Overlay` fullscreen
    - fondo opcional oscurecido
    - contenedor centrado de rueda
    - slices
    - iconos
    - highlight
    - label central opcional

#### Recomendaciones visuales
- anchor del contenedor principal al centro
- rueda centrada exacta
- tamaño fijo aprox. `500x500` o `650x650`
- iconos bien separados
- highlight claro por slice
- texto central opcional con el emote seleccionado

---

### 6.3 Ajustes en `Class Defaults`
Con `WBP_EmoteWheel` abierto:

Ir a **Class Defaults** y buscar propiedades heredadas:
- `SelectionDeadzone`
- `SelectionAngleOffsetDegrees`

#### Valores recomendados para empezar
- `SelectionDeadzone = 0.35`
- `SelectionAngleOffsetDegrees = 90`

#### Qué hace cada una
- `SelectionDeadzone`: si el vector es muy pequeño, no selecciona nada
- `SelectionAngleOffsetDegrees = 90`: el slice 0 arranca arriba

---

### 6.4 Eventos BP que sí debes usar
En el Graph del widget, usa solo estos eventos de presentación:
- `BP_OnEntriesSet`
- `BP_OnSelectionChanged`
- `BP_OnSelectionCleared`
- `BP_OnSelectionConfirmed`

#### `BP_OnEntriesSet`
Solo visual:
- colocar iconos
- colocar labels
- activar/desactivar slices según cantidad
- refrescar brushes/materiales

#### `BP_OnSelectionChanged`
Solo visual:
- mover o mostrar highlight
- actualizar texto central
- reproducir animación de hover si quieres

#### `BP_OnSelectionCleared`
Solo visual:
- quitar highlight
- borrar texto central
- dejar estado neutral

#### `BP_OnSelectionConfirmed`
Solo visual:
- animación breve de confirmación
- flash
- sonido UI local si quieres

### Importante
No llames gameplay ni RPC aquí.  
El C++ ya confirma y ejecuta.

---

### 6.5 Forma práctica de montarlo
Tienes dos enfoques:

#### Enfoque simple
Crear 8 slices fijos en el diseñador:
- `Slice0 ... Slice7`
- `Icon0 ... Icon7`

Y en `BP_OnEntriesSet`:
- llenas solo los que existan
- ocultas los sobrantes

#### Enfoque flexible
Crear hijos dinámicamente en BP al recibir `Entries`.

#### Recomendación
Si quieres minimizar lógica en BP, usa el **enfoque simple**.

---

## 7. Crear `WBP_QuickChatWheel`

Mismo proceso que la rueda de emotes.

### 7.1 Crear widget
1. `User Interface > Widget Blueprint`
2. Parent class: `TN_RadialWheelWidgetBase`
3. Nombre: `WBP_QuickChatWheel`

---

### 7.2 Diseño recomendado
Hazla parecida a la de emotes para consistencia visual, pero con:
- icono por mensaje
- texto más visible
- quizá menos slices si usarás 6–8 mensajes

---

### 7.3 `Class Defaults`
Igual que emotes:
- `SelectionDeadzone = 0.35`
- `SelectionAngleOffsetDegrees = 90`

---

### 7.4 Eventos visuales
Usa los mismos eventos:
- `BP_OnEntriesSet`
- `BP_OnSelectionChanged`
- `BP_OnSelectionCleared`
- `BP_OnSelectionConfirmed`

#### Diferencia
Aquí el texto suele ser más importante que el icono.

---

## 8. Conectar el feed visual de quick chat al HUD

No hace falta lógica de red en Blueprint.

El C++ del HUD ya te entrega:
- `Sequence`
- `SenderName`
- `MessageText`
- `Icon`
- `ServerTimeSeconds`

Tú solo pintas.

---

### 8.1 Identifica tu widget HUD cooperativo
Tienes dos casos:

#### Caso A — ya tienes un widget BP asignado a `CoopFlowWidgetClass`
Edita ese widget.

#### Caso B — no tienes uno
Crea uno heredando de:
- `TN_CoopFlowHUDWidget`

Nombre sugerido:
- `WBP_CoopFlowHUD`

---

### 8.2 Si creas uno nuevo
Crear `Widget Blueprint` con clase padre:
- `TN_CoopFlowHUDWidget`

Si además va a llevar resultados, respeta los nombres bind ya esperados por C++:
- `ResultsOverlay`
- `ResultsTitle`
- `ResultsRankText`
- `ResultsTimeText`
- `ResultsCountdown`
- `SpectatorHint`

---

### 8.3 Añadir el contenedor visual del feed
En el diseñador del HUD, añadir por ejemplo:
- un `Overlay` o `Canvas Panel`
- dentro, un `ScrollBox` o `VerticalBox` anclado a una esquina

#### Recomendación de colocación
- esquina superior izquierda o inferior izquierda
- ancho aprox. `400–550`
- alto aprox. `250–350`

---

### 8.4 Crear una fila visual de quick chat
Recomendado crear otro widget:
- `WBP_QuickChatFeedEntry`

#### Contenido sugerido
- `Image` para icono
- `TextBlock Sender`
- `TextBlock Message`

Este widget solo representa una línea del feed.

---

### 8.5 Implementar el evento visual del HUD
En el Graph de tu HUD BP, implementa:
- `Event OnQuickChatEntryReceived`

Dentro de ese evento:
1. `Create Widget` → `WBP_QuickChatFeedEntry`
2. setear:
   - `SenderName`
   - `MessageText`
   - `Icon`
3. añadirlo al `ScrollBox` o `VerticalBox`
4. opcional:
   - autoscroll al final
   - animación de fade-in
   - límite visual de filas

### Importante
No hacer aquí:
- resolución de IDs
- búsqueda de `GameState`
- RPCs
- timers de red

---

## 9. Asignar todo en `BP_GamePlayerController`

Abrir:

`Content/Blueprints/Gameplay/Controllers/BP_GamePlayerController`

### 9.1 En `Class Defaults`
Buscar y asignar:

#### UI radial
- `EmoteWheelWidgetClass` → `WBP_EmoteWheel`
- `QuickChatWheelWidgetClass` → `WBP_QuickChatWheel`

#### Data radial
- `EmoteWheelDataAsset` → `DA_EmoteWheelCatalog`
- `QuickChatWheelDataAsset` → `DA_QuickChatWheelCatalog`

#### HUD
- `CoopFlowWidgetClass` → tu HUD BP (`WBP_CoopFlowHUD` o el que ya uses)

---

### 9.2 No hace falta tocar nada más del controller
El C++ ya:
- crea widgets
- los añade al viewport
- abre/cierra ruedas
- lanza quick chat
- confirma emotes

---

## 10. Asignar catálogo de emotes en `BP_TortugaCharacter`

Abrir:

`Content/Blueprints/Characters/BP_TortugaCharacter`

### 10.1 En `Class Defaults`
Buscar:
- `EmoteWheelDataAsset`

Asignar:
- `DA_EmoteWheelCatalog`

### Importante
No es solo para UI:
- el servidor valida emotes desde el character
- el character también resuelve `Montage` desde ahí

Si esto no está asignado, el catálogo visual puede existir en el controller, pero el personaje no tendrá el mismo catálogo para ejecutar/validar correctamente.

---

## 11. Ajustes visuales recomendados

### 11.1 Cursor / centro de rueda
El C++ al abrir la rueda:
- muestra cursor
- centra el mouse
- al cerrar, restaura la posición previa

#### Qué implica
Diseña la rueda exactamente centrada en pantalla.

---

### 11.2 Orden de slices
Si al probar notas que:
- el highlight cae en el slice de al lado
- o la rueda gira al revés respecto a lo esperado

Haz esto en orden:
1. reordena visualmente los slices para que coincidan con el orden del DataAsset
2. si aún no cuadra, ajusta `SelectionAngleOffsetDegrees`

#### Recomendación
Empieza con `90`.

---

### 11.3 Deadzone
Si notas que:
- se confirma demasiado fácil
- o cuesta demasiado seleccionar

Ajusta en ambos widgets:
- `SelectionDeadzone`

#### Valores típicos
- `0.25` = más sensible
- `0.35` = equilibrado
- `0.45` = más exigente

#### Recomendación
Empieza con `0.35`.

---

### 11.4 Confirmación en deadzone
Ya está resuelto por C++:
- si sueltas dentro de deadzone
- no hay selección válida
- se cancela

No necesitas hacer nada extra.

---

## 12. Project Settings a revisar

### 12.1 Enhanced Input
Ir a:

`Project Settings > Input`

Verificar:
- `Default Player Input Class = EnhancedPlayerInput`
- `Default Input Component Class = EnhancedInputComponent`

Según la configuración del proyecto, esto debería estar ya correcto, pero conviene revisarlo.

---

### 12.2 Mouse / viewport
No hace falta tocar nada raro globalmente si el `PlayerController` ya cambia `InputMode`.

Si notas comportamiento extraño del cursor:
- primero prueba en **Standalone**
- no cambies `Project Settings` a ciegas

---

## 13. Prueba mínima en PIE

### 13.1 Configuración básica
En el desplegable de Play:
- `Number of Players = 2`

Haz primero una prueba **listen server**.

#### Objetivo
Validar que:
- host abre ruedas
- cliente abre ruedas
- ambos pueden confirmar
- quick chat aparece en ambos
- emote se ve en ambos

---

### 13.2 Casos concretos a probar

#### Emote wheel
1. mantener botón
2. mover mouse fuera de deadzone
3. soltar
4. debe ejecutarse el emote

#### Cancelación
1. mantener botón
2. no mover o volver al centro
3. soltar
4. no debe ejecutar nada

#### Quick chat
1. abrir rueda
2. seleccionar mensaje
3. soltar
4. debe aparecer en tu HUD y en el del otro jugador

#### Rate limit
- spamear quick chat
- no deberían pasar todos

#### Emote cooldown
- repetir el mismo emote muy rápido
- el servidor debe limitarlo

---

## 14. Probar en Dedicated Server

Haz una pasada adicional en dedicated, no solo en listen.

En Play settings:
- `Number of Players = 2 o 3`
- `Run Dedicated Server = true`

#### Qué validar
- quick chat funciona sin depender del host local
- emotes replican igual
- el HUD sigue recibiendo quick chat

---

## 15. Probar tras travel non-seamless

Tu proyecto usa **non-seamless travel**, así que esta prueba es obligatoria.

### Qué probar
1. entrar al lobby
2. abrir rueda y usarla
3. empezar partida
4. viajar a Run
5. volver a abrir rueda
6. quick chat sigue apareciendo
7. emotes siguen funcionando
8. al volver al lobby, sigue funcionando

#### Qué valida realmente
Que al recrearse:
- `PlayerController`
- `PlayerState`
- `Pawn`
- widgets

las referencias de `Class Defaults` bastan para reenganchar el sistema entero.

---

## 16. Emulación de red

Objetivo de prueba:
- `80–120 ms` de ping aproximado
- `1–3%` de pérdida

### Opción A — desde PIE
Si tu configuración de Play muestra `Network Emulation`:
- activarla
- poner lag aprox. `100 ms`
- packet loss `2%`

### Opción B — por consola en las ventanas de juego
Puedes probar comandos tipo:

```text
Net PktLag=100
Net PktLagVariance=20
Net PktLoss=2
```

### Qué testear bajo lag/loss
- abrir/cerrar rápido la rueda
- quick chat repetido
- confirmar quick chat y emote varias veces
- cliente que entra tarde después de varios quick chats
- viaje `lobby → run → lobby`

---

## 17. Orden exacto recomendado

### Fase 1 — Input
1. crear `IA_OpenEmoteWheel`
2. crear `IA_OpenChatWheel`
3. crear `IA_RadialNavigate`
4. mapearlos en `IMC_Player`

### Fase 2 — Datos
5. crear `DA_EmoteWheelCatalog`
6. rellenar emotes
7. crear `DA_QuickChatWheelCatalog`
8. rellenar mensajes

### Fase 3 — UI radial
9. crear `WBP_EmoteWheel`
10. crear `WBP_QuickChatWheel`
11. ajustar deadzone/offset
12. implementar solo eventos visuales

### Fase 4 — HUD
13. abrir o crear `WBP_CoopFlowHUD`
14. añadir feed visual
15. implementar `OnQuickChatEntryReceived`

### Fase 5 — Asignaciones
16. abrir `BP_GamePlayerController`
17. asignar widgets radiales
18. asignar ambos DataAssets
19. asignar `CoopFlowWidgetClass`
20. abrir `BP_TortugaCharacter`
21. asignar `EmoteWheelDataAsset`

### Fase 6 — QA
22. probar en PIE listen
23. probar en PIE dedicated
24. probar con lag/loss
25. probar con travel

---

## 18. Qué NO tienes que hacer

- no tienes que crear lógica de selección angular en Blueprint
- no tienes que replicar nada en widgets
- no tienes que usar `GameMode` para quick chat
- no tienes que replicar texto, iconos o montages
- no tienes que hacer tick en UMG para la rueda
- no tienes que crear 20 input actions nuevas

---

## 19. Señales de que algo está mal

### La rueda no abre
Revisar:
- nombres exactos de assets:
  - `IA_OpenEmoteWheel`
  - `IA_OpenChatWheel`
  - `IA_RadialNavigate`
- ruta exacta:
  - `/Game/Blueprints/Gameplay/Controls/`
- mappings en `IMC_Player`

---

### La rueda abre pero no selecciona bien
Revisar:
- `SelectionDeadzone`
- `SelectionAngleOffsetDegrees`
- layout centrado en pantalla
- stick asignado como `Axis2D`

---

### Se ve la rueda pero no ejecuta emote
Revisar:
- `BP_GamePlayerController` tiene `EmoteWheelDataAsset`
- `BP_TortugaCharacter` tiene `EmoteWheelDataAsset`
- el `EmoteID` existe en el catálogo

---

### Se envía quick chat pero no lo ves en HUD
Revisar:
- `CoopFlowWidgetClass` apunta a tu widget BP derivado de `TN_CoopFlowHUDWidget`
- implementaste `OnQuickChatEntryReceived`
- tu contenedor visual realmente añade la fila creada

---

### Se ejecuta emote pero sin animación montage
Revisar:
- `Montage` asignado en `DA_EmoteWheelCatalog`
- el personaje tiene `AnimInstance` o setup compatible
- el `EmoteID` correcto apunta al montage correcto

---

## 20. Recomendación práctica final

Haz una primera versión pequeña y validable.

### Emotes
Empieza con:
- 4 emotes
- 4 iconos
- montages solo en 1 o 2

### Quick chat
Empieza con:
- 4 mensajes
- feed visual básico
- sin animaciones complejas al principio

### Luego
Cuando todo funcione:
- amplías a 8
- pules highlight
- metes animaciones UMG
- afinas deadzone
- afinas orden angular

---

## 21. Resumen corto

Si sigues este documento, el cierre correcto del sistema es:

- **Input**: 3 acciones nuevas y mapeadas en `IMC_Player`
- **Datos**: 2 DataAssets con IDs compactos
- **UI**: 2 widgets radiales visuales heredando de la base C++
- **HUD**: feed visual conectado solo por el evento `OnQuickChatEntryReceived`
- **Asignaciones**: todo enlazado en `BP_GamePlayerController` y `BP_TortugaCharacter`
- **QA**: probar listen, dedicated, lag/loss y travel non-seamless


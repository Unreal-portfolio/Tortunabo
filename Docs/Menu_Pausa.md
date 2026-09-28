# Menú de pausa y ajustes

Tortunavy tiene un menú de pausa para todos los modos (lobby, mapa procedural, carrera en la playa y nivel de solo
terreno). No pausa nada: la partida es en red y sigue en marcha, pero tu tortuga se queda quieta mientras lo miras. Desde
él se vuelve a la partida, se cambian los ajustes (gráficos, sonido, voz, controles y accesibilidad), se consulta la
lista de controles y se sale (al lobby, al menú principal o al escritorio). Todo es código: la interfaz se monta en C++
con el estilo del HUD (`TN_HUDArt`, `TN_HUDStyle`, `TN_ShopArt`) y los ajustes se aplican sin assets nuevos.

## Piezas

| Pieza | Archivo | Qué hace |
|---|---|---|
| `UTN_GameSettingsSubsystem` | `Settings/TN_GameSettingsSubsystem.*` | `UGameInstanceSubsystem` + `FTickableGameObject`. Carga, aplica y guarda los ajustes; mete la tecla del menú en el PlayerController; abre y cierra el menú; getters de cámara para cualquier cámara (el espectador incluido). |
| `FTNGameSettings`, `UTN_SettingsSaveGame` | `Settings/TN_SettingsSaveGame.h` | Los ajustes que no son de `UGameUserSettings` y su ranura de guardado. |
| `UTN_PauseMenuWidget` | `UI/Pause/TN_PauseMenuWidget.*` | El menú: cabecera, portada, ajustes en cinco pestañas, lista de controles y cuadro de confirmación. |
| `UTN_PauseRow` | `UI/Pause/TN_PauseMenuWidget.*` | Fila enfocable: botón, deslizador, lista de opciones, texto o medidor. |
| `UTN_FpsCounterWidget` | `UI/Pause/TN_PauseMenuWidget.*` | Contador de FPS (ajuste «Mostrar FPS»). |
| `TNPauseArt` | `Private/UI/Pause/TN_PauseArt.h` | Iconos pintados en código: los de los botones de la portada, altavoz y micrófono (tachados si están silenciados) y la corona del anfitrión. |
| `UProximityVoiceComponent` | `Voice/ProximityVoiceComponent.*` | Añadido: `GetMicLevel`, `IsCapturing` y `SetTransmitEnabled` (medidor, silenciarse y pulsar para hablar). |
| `ATN_RunGameMode::ReturnToLobbyNow` | `Game/TN_RunGameMode.h` | Entrada pública para que el anfitrión lleve a todos al lobby (la misma vuelta que al acabar la ronda). |

No hace falta ningún módulo nuevo en `Tortunabo.Build.cs` (UMG, Slate, EnhancedInput, AudioMixer, OnlineSubsystem y
Engine ya estaban; `DeveloperSettings` y `GameplayTags` llegan por Engine).

## Abrir y cerrar

- **Escape** en el juego empaquetado o en standalone; **Tabulador** en el editor, donde Escape corta la partida (en el
  editor también vale Escape si le llega al juego); **Start** (Menú) del mando.
- No es el PlayerController quien lo escucha: `UTN_GameSettingsSubsystem` mete un `UInputComponent` propio (prioridad 100)
  en la pila de entrada del `AMP_GamePlayerController` local con `PushInputComponent`. Así vale jugando y de espectador,
  no hay que tocar el PlayerController y los viajes sin cortes lo conservan (tras uno con corte se vuelve a meter). En el
  menú principal (`MP_MenuPlayerController`) no hay menú de pausa.
- **No se abre** si ya hay otra interfaz con el ratón a la vista (tienda, probador, general, ruedas de bailes y frases,
  campeón de la carrera...: esa manda y se cierra con su propio Escape), con la pantalla de carga del huevo a la vista
  (también el «¡ADELANTE!») o durante un viaje. Encima del recuento de la carrera sí (no tiene ratón ni teclas).
- **Se cierra** con Tabulador o Start (del todo), Escape, B o Retroceso (atrás: en la portada, cierra) o «Continuar».
- **Mientras está abierto**: modo de entrada interfaz y juego (`FInputModeGameAndUI`) con cursor, la tortuga quieta
  (`SetIgnoreMoveInput` y `SetIgnoreLookInput`, contados para devolverlos igual) y se sueltan las teclas que hubiera
  pulsadas (`FlushPressedKeys`, para que no siga corriendo). Las teclas se quedan en el menú (no salta, no se mete en el
  caparazón, no abre la tienda) salvo la consola y la tecla de pulsar para hablar; el stick izquierdo mueve el foco y
  los gatillos y el stick derecho no llegan al juego. Si algo le quita el cursor o el foco (reaparecer, cambiar de
  ventana), lo recupera solo.
- **Al cerrarse** (también si un viaje lo quita de la pantalla) devuelve la entrada: modo juego sin cursor o, si mientras
  tanto ha salido otra pantalla que se puede pulsar (p. ej. el campeón de la carrera), interfaz y juego con cursor.
- Capa 60 del viewport: por encima del HUD (4-10), las pantallas de la carrera (20-21), las ruedas (30-31) y la tienda
  (40); por debajo de la pantalla de carga (20000). El contador de FPS va en la 70.

## Qué hay

**Cabecera**: cinta «PAUSA» sobre un cartel azul marino con el mapa o modo (lobby del castillo o del cuartel; carrera
en la playa con su ronda y las conchas para ganar; cooperativo con ronda, dificultad y semilla del mapa; 2 contra 2;
solo terreno; carrera clásica), la sesión (partida de quién, sala —los 6 últimos caracteres del id de la sesión— y
tortugas conectadas; sin sesión, si eres el anfitrión, un invitado o una partida local) y los jugadores con su cara, su
nombre, «Tú», «Anfitrión» (corona) o su ping, y su icono de voz: micrófono para ti y altavoz para los demás, que late
cuando habla y sale tachado si está silenciado.

**Portada**: Continuar, Ajustes, Controles, Volver al lobby, Menú principal (anfitrión) o Salir de la partida
(invitado) y Salir al escritorio. Abajo, la ayuda de la opción enfocada y los atajos.

| Opción | Anfitrión | Invitado |
|---|---|---|
| Volver al lobby | Solo en una partida (modos de `ATN_RunGameMode`: Run, mapa procedural, carrera). Confirma y llama a `ReturnToLobbyNow`: peones fuera y viaje sin cortes al lobby del que se salió; los invitados van detrás. | Al grupo lo mueve el anfitrión: el cuadro lo explica y, si confirma, sale él solo de la sesión al menú principal (lo mismo que «Salir de la partida»). |
| Menú principal / Salir de la partida | «Menú principal»: confirma y `UMP_GameInstance::HandleReturnToMenu` cierra la sesión para todos. | «Salir de la partida»: confirma y sale él solo de la sesión al menú; los demás siguen. |
| Salir al escritorio | Confirma (avisa de que la partida se acaba para todos si hay invitados) y `QuitGame`. | Confirma y `QuitGame`. |

En el lobby no sale «Volver al lobby» (ya se está) y en el nivel de solo terreno tampoco (no sale de un lobby).

**Controles**: lista de teclas del juego leída de `IMC_Player` en ejecución (siempre la de verdad), con teclado y ratón y
mando por separado y los nombres de las teclas en español, más el menú de pausa, hablar, cambiar de cámara de espectador
y moverse por los menús.

**Moverse por el menú**: flechas, WASD, cruceta o stick izquierdo; Intro, Espacio o A pulsan; izquierda y derecha (A y
D) cambian los deslizadores y las listas; Q y E o LB y RB cambian de pestaña; el ratón enfoca al pasar por encima,
pulsa con clic y arrastra los deslizadores; la rueda desplaza las listas. Cada fila suena al enfocarla («pom») y al
pulsar («plin»): los sonidos de las conchas (`UTN_ScoreShellSynthComponent`); al mover un deslizador el «pom» sube de
tono con el valor.

**Medidas**: todo está pensado a 1080 p de referencia; la escala de la interfaz del motor lo encoge a 720 p (cabe entero en
1280×720) y lo agranda en 4K.

## Ajustes

Se aplican al momento. Los propios (`FTNGameSettings`) se guardan en `Saved/SaveGames/TN_Settings.sav` al cerrar el menú
(o a los 3 s del último cambio con el menú cerrado) y se cargan al crearse la GameInstance, es decir, al arrancar el
juego. Los gráficos van en `UGameUserSettings` (`Saved/Config/<Plataforma>/GameUserSettings.ini`), que el motor aplica
solo al arrancar.

### Gráficos

| Ajuste | Cómo se aplica |
|---|---|
| Modo de ventana (pantalla completa, sin bordes, ventana) | `SetFullscreenMode` + `ApplyResolutionSettings`; sin bordes va a la resolución del escritorio. Sale un cuadro «¿Mantener esta pantalla?» y, si no se confirma en 12 s, se deshace (`RevertVideoMode`). Una resolución sin confirmar no se guarda. |
| Resolución | Pantalla completa: `GetSupportedFullscreenResolutions`; ventana: `GetConvenientWindowedResolutions`. Igual que el modo, con confirmación. |
| Escala de resolución | `SetResolutionScaleValueEx` (del mínimo al máximo que da `GetResolutionScaleInformationEx`). |
| Sincronización vertical | `SetVSyncEnabled`. |
| Límite de fotogramas | `SetFrameRateLimit`: 30, 60, 90, 120, 144, 165, 240 o sin límite. |
| Brillo | `GEngine->DisplayGamma` (la gamma que usa el tonemapper): 50 % es la de serie, cada extremo la mueve 0,7. |
| Mostrar FPS | `UTN_FpsCounterWidget` abajo a la derecha: FPS y el peor fotograma del último medio segundo en ms; verde, dorado o coral según vaya. Se vuelve a poner tras cada viaje. |
| Calidad general (Baja, Media, Alta, Épica) | `SetOverallScalabilityLevel`; si luego se cambia una parte, sale «Personalizada» (y «Cine» si algo está en 4). |
| Sombras, efectos, vegetación, distancia de visión, antialiasing, texturas, postprocesado, iluminación global y reflejos | `Set...Quality` de cada una. |
| Calidad recomendada | `RunHardwareBenchmark` + `ApplyHardwareBenchmarkResults` (la imagen se congela un momento). |

Cada cambio llama a `ApplyNonResolutionSettings` y se guarda con `SaveSettings` al cerrar el menú.

### Sonido

| Ajuste | Cómo se aplica |
|---|---|
| General | Volumen principal del dispositivo de audio del mundo (`FAudioDevice::SetTransientPrimaryVolume`): todo, voces incluidas. |
| Música | Clase de sonido `TN_Music` creada en ejecución (`Properties.Volume`). |
| Ambiente | Clase de sonido `TN_Ambient`. |
| Efectos | Mezcla `TN_EffectsMix` creada en ejecución y empujada en el dispositivo de cada mundo, con `SetSoundMixClassOverride` sobre la clase de sonido por defecto del motor (y sus hijas). |

**Qué cae en cada categoría.** El proyecto sintetiza casi todo en código y no hay clases de sonido en el contenido, así
que `UTN_GameSettingsSubsystem` reparte cada fotograma los sonidos generados en código (los `UAudioComponent` cuyo
sonido no es un asset: los `USynthSound` de los sintetizadores y las ondas procedurales de la voz) poniendo
`SoundClassObject` en su sonido; el dispositivo lee la clase en cada actualización, así que vale aunque ya estén sonando:

| Categoría | Qué |
|---|---|
| Música | `UTN_MusicSynthComponent`: tienda, probador, victoria, derrota, eliminado y las radios 3D del tendero. |
| Ambiente | `UTN_AmbientSynthComponent`: el paisaje sonoro por bioma (`TN_AmbientSoundscape`), cascadas y fuentes puntuales. Además, si el `UTN_AmbientSoundscapeComponent` del PlayerController no trae clase propia, se le pone la de Ambiente en su `SoundClassOverride`, así que sus sonidos de sustitución (assets de `AmbienceData`, hoy ninguno) también van aquí. |
| Voz | La voz de los compañeros (`USoundWaveProcedural` del grupo `SOUNDGROUP_Voice` de `UProximityVoiceComponent`). |
| Efectos | Todo lo demás, que se queda en la clase por defecto: pasos y ruidos de la tortuga (`TN_TurtleFoleyComponent`), trampas y enemigos de la playa, rebuscar, tos de la tormenta, pájaros del mareo, piezas del patio del lobby, conchas y los sonidos de los menús, el huevo de la pantalla de carga y los sonidos de asset (saltos, bailes, lanzar, pisadas en arena). |

**Qué queda fuera.** Los sonidos de asset con su propia clase de sonido no se tocan (hoy no hay ninguno), los assets
nunca se modifican y, si alguien pone su propia clase en el `SoundClassOverride` del paisaje sonoro, se respeta la suya
(no baja con Ambiente). Un sintetizador nuevo cae solo en Efectos; si es música o ambiente, que herede de
`UTN_MusicSynthComponent` o `UTN_AmbientSynthComponent` o que se añada su clase en `UTN_GameSettingsSubsystem::ClassFor`.
Un sonido 2D de la interfaz que se cree y se acabe dentro del mismo fotograma podría sonar ese fotograma sin su clase (no
pasa con los sintetizadores del proyecto, que viven mucho más).

### Voz

| Ajuste | Cómo se aplica |
|---|---|
| Voz de los compañeros | Clase `TN_Voice` (todas las voces). |
| Voz de cada compañero (0-200 %) y silenciarlo | Multiplicador de volumen del componente de reproducción de su voz (`PlaybackVolume` × el tuyo; 0 si está silenciado), cada fotograma. Se guarda por su id de la plataforma (Steam) o, si no hay, por su nombre, así que se recuerda entre partidas. |
| Silenciar mi micrófono | `UProximityVoiceComponent::SetTransmitEnabled(false)`: se sigue capturando (el medidor vive) pero no se envía nada y la tortuga deja de «hablar» en el acto. |
| Modo: voz abierta o pulsar para hablar | Con pulsar para hablar, la salida solo se abre con la tecla pulsada (`IsInputKeyDown`); encima sigue haciendo falta superar el umbral. |
| Tecla para hablar | V (por defecto), T, B, Bloq Mayús o los botones laterales del ratón: ninguna hace nada más en el juego. No existía: es nueva. |
| Botón del mando para hablar | Cruceta abajo (por defecto), cruceta arriba, clic del stick derecho o B. |
| Sensibilidad | `SpeakingThreshold` del componente propio: de -20 dB (0 %) a -60 dB (100 %); 50 % es el umbral de siempre (0,01 RMS, -40 dB). |
| Ganancia | `VoiceGain` del componente propio (la de serie × 25-300 %). |
| Nivel del micrófono | Medidor en vivo con `GetMicLevel` (RMS del último bloque, con la ganancia) en dB de -60 a 0, con la raya dorada del umbral; dice «¡Se te oye!», «En silencio», «Silenciado», «Mantén V» o «Sin micrófono». |

Las variables `voice.*` del motor no sirven aquí: son del VOIP del motor, que está apagado (`[Voice] bEnabled=false`);
el proyecto usa su propio `UProximityVoiceComponent`. Se usa el micrófono predeterminado de Windows (cambiar de
micrófono en caliente reabriría la captura WASAPI, que es lo que se rompe; ver el aviso del componente).

### Controles

| Ajuste | Cómo se aplica |
|---|---|
| Sensibilidad del ratón y del mando (20-300 %) | Escalas de giro del PlayerController (`InputYawScale_DEPRECATED` e `InputPitchScale_DEPRECATED`, que se usan porque `bEnableLegacyInputScales=True` en `DefaultInput.ini`), multiplicando las de su clase; la del ratón o la del mando según el último aparato usado (`UInputDeviceSubsystem::GetMostRecentlyUsedHardwareDevice`). |
| Invertir eje Y (ratón / mando) | La misma escala de cabeceo, en negativo. |

Valen para todo lo que gira con `AddControllerYawInput`/`AddControllerPitchInput`: la tortuga (que además aplica su
`LookSensitivityX/Y`) y la cámara del espectador. Para una cámara que gire a mano con el valor crudo de la acción, el
subsistema da `GetLookSensitivity`, `IsLookYInverted`, `IsUsingGamepad` y `ApplyLookSettings`; `IsCameraShakeEnabled` y
`GetFieldOfViewOffset` para el temblor y el campo de visión (`UTN_GameSettingsSubsystem::Get(this)`). Ojo: si la cámara
ya pasa por `AddYawInput`/`AddPitchInput`, la sensibilidad va aplicada y no hay que multiplicarla otra vez.

**Reasignar teclas: no.** `UEnhancedInputUserSettings` solo reasigna acciones con `PlayerMappableKeySettings`, y ni las
`IA_*` ni `IMC_Player` los tienen (habría que editar los assets en el editor y activar `bEnableUserSettings` en los
ajustes de Enhanced Input); además la tortuga rehace sus contextos con `ClearAllMappings` al poseerse. Queda la lista
clara. Para añadirlo: marcar cada `IA_*` como reasignable con un nombre, activar los ajustes de usuario, registrar
`IMC_Player` con `RegisterInputMappingContext` y añadir en la pestaña de controles filas que escuchen la tecla siguiente.

### Juego y accesibilidad

| Ajuste | Cómo se aplica |
|---|---|
| Temblor de cámara | Apagado, el subsistema desactiva cada fotograma (`DisableModifier`) los modificadores del `PlayerCameraManager` cuya clase se llama «...Shake...»: el de serie del motor (`UCameraModifier_CameraShake`) y el de la playa (`UTN_BeachCameraShake`: quads, cangrejo, tormenta...), sin depender de él. Encendido otra vez, los reactiva. |
| Campo de visión (-15 a +20°) | `CameraFOVDefault` y `CameraFOVSprint` de la tortuga local = los de su clase + el desplazamiento (al correr se abre lo mismo que antes). Se enseña en grados. |
| Filtro para daltónicos (deuteranopía, protanopía, tritanopía) e intensidad | `UWidgetBlueprintLibrary::SetColorVisionDeficiencyType` en modo corrección: Slate lo aplica a toda la ventana, juego incluido. |
| Idioma | Solo hay español: se enseña, no se cambia. |

Cada pestaña (menos la de gráficos) tiene «Restablecer». En gráficos, «Calidad recomendada».

## En el editor

- La resolución y el modo de ventana salen apagados (la ventana es la del editor).
- Al acabar la partida se devuelven la calidad gráfica (`Scalability::SetQualityLevels`), el límite de fotogramas, la
  sincronización vertical, la gamma, el filtro de color y el volumen principal que tenía el editor: se pueden probar
  los gráficos en PIE sin que el editor se quede así. Los gráficos del juego (`GameUserSettings.ini` del editor) se
  guardan igual.
- Con varios jugadores en un proceso, cada ventana tiene su GameInstance, su subsistema y su menú; los ajustes propios
  comparten archivo (el último que guarda manda) y la gamma y el filtro de color son del proceso entero.

## Fuera y por qué

- **Reasignar teclas**: ver Controles.
- **Elegir micrófono o salida de audio**: reabrir la captura WASAPI en caliente es justo lo que rompe el componente de
  voz; se usa el predeterminado de Windows.
- **Vibración del mando**: el juego no usa vibración (no hay `ForceFeedback`).
- **Idioma**: solo hay español.
- **Escala de la interfaz**: cambiaría toda la interfaz de Slate (en el editor, la del editor) y podría sacar el menú
  de 1280×720.

## Aviso encontrado

`IA_Quit` está en `IMC_Player` (probablemente en Retroceso) y `BP_GamePlayerController` lo usa como `ReturnToMenuAction`:
pulsarla jugando vuelve al menú sin preguntar y, en el anfitrión, cierra la partida para todos. Con el menú de pausa
(que pide confirmación) quizá convenga quitarla o moverla. Mientras el menú está abierto, Retroceso solo va hacia atrás.

## Pruebas

1. PIE con 1 jugador en el lobby: Tabulador abre y cierra; Start del mando también; la tortuga no se mueve ni gira la
   cámara con el menú abierto y vuelve a moverse al cerrarlo; Escape dentro del menú va hacia atrás y cierra desde la
   portada.
2. Standalone (o el juego empaquetado): Escape abre y cierra.
3. Tienda, probador y general: con uno abierto, Escape y Tabulador siguen siendo suyos; el menú de pausa no sale.
4. Ruedas de bailes y frases: con la rueda abierta no sale.
5. Mapa procedural, carrera y solo terreno: se abre; en la carrera sale la ronda y las conchas; en el cooperativo, la
   semilla; en solo terreno, sin «Volver al lobby».
6. Espectador (eliminado o tras la meta): se abre y se cierra; la cámara del espectador vuelve a girar al cerrarlo.
7. Ajustes de sonido: bajar Música en la tienda (la canción baja), Ambiente en el mapa procedural (olas y viento),
   Efectos (pasos, saltos, conchas y los propios «pom» del menú) y General (todo).
8. Voz con 2 jugadores (PIE, dos ventanas, «Play As Listen Server» o dos standalone): el medidor se mueve al hablar y
   la raya dorada se mueve con la sensibilidad; «Silenciar mi micrófono» hace que el otro deje de oírte y de ver el
   bocadillo; pulsar para hablar con V; «Voz de X» al 0 % o «Silenciar a X» en el otro; los iconos de voz de la
   cabecera laten al hablar.
9. Controles: sensibilidad del ratón al 300 % y al 20 % (la cámara gira más o menos), invertir Y; con mando, su
   propia sensibilidad; la lista de controles coincide con lo que hace cada tecla.
10. Juego: temblor de cámara apagado en la carrera (quads y cangrejo sin sacudida); campo de visión a +20° y a -15°;
    filtro para daltónicos.
11. Gráficos: calidad general Baja y Épica (se nota), una parte suelta (sale «Personalizada»), escala de resolución,
    límite de 30 FPS, brillo, Mostrar FPS (se mantiene tras un viaje). Fuera del editor: pantalla completa y otra
    resolución, dejar pasar los 12 s (se deshace) y luego «Mantener».
12. Cerrar el juego y abrirlo: todo lo anterior sigue igual.
13. Anfitrión en una partida con 2 jugadores: «Volver al lobby» lleva a los dos al lobby; en el invitado, «Volver al
    lobby» y «Salir de la partida» avisan y lo sacan solo a él al menú principal (el anfitrión sigue).
14. «Menú principal» en el anfitrión cierra la partida para los dos; «Salir al escritorio» cierra el juego.
15. Abrir el menú con el recuento de la carrera en pantalla y cerrar cuando sale el campeón: al cerrar, el cursor sigue
    para pulsar sus botones.

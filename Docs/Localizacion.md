# Localización: idiomas, traducciones y fuentes

Tortunavy se escribe en **español de España** (es-ES, el idioma origen) y se puede traducir a los idiomas de la lista de
abajo con el canal de localización del motor (`FText` + objetivo «Game» + archivos `.po`). Este documento es la **fase 1**
(el canal, el ajuste de idioma, los nombres de sala y las fuentes). La **fase 2** —auditar que todos los textos visibles son
`FText` y traducirlos— está por hacer; al final hay lo que ya se sabe que falta.

## Qué hay

| Pieza | Dónde | Qué hace |
|---|---|---|
| Lista de idiomas | `Config/DefaultGame.ini` → `[/Script/Tortunabo.TN_LanguageSettings]`; clase `UTN_LanguageSettings` en `Settings/TN_LanguageSettings.*` | Los idiomas que se pueden elegir, en orden, con su fuente de reserva si la tienen. Se edita sin tocar código (también en Ajustes del proyecto > Tortunavy > Idiomas). |
| `TNLanguage` | `Settings/TN_LanguageSettings.*` | Idioma del sistema en términos de la lista, cambio en caliente de la cultura, idioma activo. |
| Ajuste «Idioma» | Menú de pausa > Ajustes > Juego (primera fila) y `FTNGameSettings::Language` | Elegirlo; se guarda con los demás ajustes (`Saved/SaveGames/TN_Settings.sav`). |
| Objetivo de localización «Game» | `Config/DefaultEditor.ini` (`[/Script/Localization.LocalizationSettings]`) y `Config/Localization/Game_*.ini` | Qué se recoge, qué culturas se generan, cómo se exporta, importa y compila. |
| Scripts | `Scripts/localization_gather_export.bat`, `Scripts/localization_import_compile.bat` | Recoger → exportar `.po`; importar `.po` → compilar `.locres`. |
| Datos | `Content/Localization/Game/` (lo crea el primer script) | `Game.manifest`, `Game.locmeta`, `<cultura>/Game.archive`, `<cultura>/Game.po`, `<cultura>/Game.locres`. |
| Empaquetado | `Config/DefaultGame.ini` → `[/Script/UnrealEd.ProjectPackagingSettings]` | `+CulturesToStage` de las 13 culturas, `InternationalizationPreset=All` y las fuentes de reserva como archivos sueltos. |
| Nombres de sala | `Multiplayer/TN_RoomNames.*`, `Tools/Localization/room_names_en.csv` | 242 nombres con clave estable `TNRoomNames` / `Room_000`; inglés de antes en el CSV. |
| Fuentes | `Private/UI/HUD/TN_HUDFonts.*` (usado por `TNHUDStyle::Font`), `Scripts/tools/check_font_coverage.py` | Fuente compuesta con reserva por idioma; comprobación de caracteres. |
| Pruebas | `Tortunabo.Settings.Language.*`, `Tortunabo.Multiplayer.RoomNames.*` | Lista de idiomas, resolución del idioma, nombres de sala y su CSV inglés. |

## Idiomas

| Cultura | Nombre en el ajuste | Notas para traducir |
|---|---|---|
| `es-ES` | Español (España) | Origen. Trato de «tú» («Pulsa», «Apágalo»), tono cercano y con guiños. |
| `en` | English | Inglés neutro (ni US ni UK marcados). |
| `fr` | Français | «tu» imperativo; comillas francesas «...» con espacios. |
| `de` | Deutsch | «du»; compuestos largos: ojo con el ancho. Sin la ẞ mayúscula (ver Fuentes). |
| `it` | Italiano | «tu»; comillas «...». |
| `pt-BR` | Português (Brasil) | Portugués de Brasil, «você»; no portugués de Portugal. |
| `ru` | Русский | «ты»; los plurales usan tres formas (ver Reglas). |
| `pl` | Polski | «ty»/imperativo; tres formas de plural. |
| `tr` | Türkçe | «sen»/imperativo; con la İ/ı turcas al pasar a mayúsculas. |
| `ja` | 日本語 | Cortés breve (です・ます), poco kanji raro; los guiños se adaptan. |
| `ko` | 한국어 | 해요체 (cortés informal). |
| `zh-Hans` | 简体中文 | Chino simplificado; puntuación de ancho completo （，。！？）. |
| `zh-Hant` | 繁體中文 | Chino tradicional de Taiwán (no Hong Kong); misma puntuación. |

**Qué idioma se usa.** Al crearse la `GameInstance` (antes de que salga ningún menú), `UTN_GameSettingsSubsystem` aplica el
idioma guardado. Sin elegir, el **idioma del sistema** si está en la lista —en términos de la lista: es-MX → es-ES, pt-PT →
pt-BR, en-GB → en, zh-TW/HK/MO → zh-Hant, zh-CN → zh-Hans— y, si no, el **español**. El cambio en el menú es en caliente
(`TNLanguage::Apply`):

- **Juego** (empaquetado o `-game`): `UKismetInternationalizationLibrary::SetCurrentCulture(cultura, false)`. Sin guardarla en
  `GameUserSettings.ini`: manda el ajuste propio, así no hay dos fuentes de verdad. Los textos `FText` se vuelven a resolver solos
  (Slate repinta todo al cambiar la revisión de textos) y la caché de fuentes se rehace sola (`OnCultureChanged`).
- **Editor**: solo cambian los textos del **juego** (`FTextLocalizationManager::EnableGameLocalizationPreview`, la
  «previsualización del idioma del juego» de las preferencias del editor). El idioma del editor no se toca, y al acabar PIE el
  propio editor apaga la previsualización. Solo funciona cuando ya existe `Content/Localization/Game/Game.locmeta` (lo crea el
  paso de compilar); antes de eso, todo sale en español, que es lo normal. Las fuentes de reserva por idioma **no** se eligen en
  el editor (el motor las elige por el idioma del editor): allí se ve la reserva CJK del motor.
- **Línea de comandos**: `-culture=xx` o `-language=xx` mandan sobre el ajuste (útil para probar). Las culturas de prueba del
  motor, en compilaciones que no son Shipping: `-culture=LEET` pone todos los textos localizables en «leet» (los que se ven
  normales **no** son localizables: lista rápida de lo que falta en la fase 2) y `-culture=keys` enseña la clave de cada texto.

**Restablecer.** «Restablecer esta pestaña» (Juego) y «Restablecer todos los ajustes» dejan el idioma en «sin elegir» (el del
sistema). Es la salida si alguien elige un idioma que no lee: la fila se llama «Idioma / Language» en todos los idiomas y el título
de la sección «IDIOMA / LANGUAGE» (que las traducciones **no** deben cambiar).

## Añadir un idioma

1. **Lista de idiomas**: una línea más en `[/Script/Tortunabo.TN_LanguageSettings]` de `Config/DefaultGame.ini`:
   `+Languages=(Culture="ar")`. Si el nombre no está entre los trece conocidos (`KnownName` en `TN_LanguageSettings.cpp`), se
   usa el que da el motor para esa cultura, o se pone a mano: `NativeName="العربية"` (guardar el ini como UTF-8).
2. **Empaquetado**: `+CulturesToStage=ar` en `[/Script/UnrealEd.ProjectPackagingSettings]` (`InternationalizationPreset=All` ya
   trae los datos de todas las culturas).
3. **Objetivo de localización**: `CulturesToGenerate=ar` en los cinco `Config/Localization/Game_*.ini` y
   `(CultureName="ar")` en `SupportedCulturesStatistics` de `+GameTargetsSettings` en `Config/DefaultEditor.ini` (o, más fácil,
   abrir Herramientas > Panel de localización (Tools > Localization Dashboard) > Game > añadir idioma, que reescribe los `.ini`).
4. **Fuente**, si su escritura no la cubre Roboto ni la reserva CJK del motor (árabe, tailandés, hindi...): dejar el archivo en
   `Content/Slate/Fonts` y poner `FontRegular` / `FontBold` / `FontScript` en su línea (ver Fuentes; `FontScript` entiende `CJK`,
   `Cyrillic` y `Latin`: otra escritura, añadir sus bloques en `AddScriptRanges`, `TN_HUDFonts.cpp`).
5. Ejecutar el flujo (siguiente apartado) y traducir su `Game.po`.
6. Correr `Tortunabo.Settings.Language.*` (comprueba que el motor conoce la cultura) y `Scripts/tools/check_font_coverage.py`.

## Flujo: recoger → traducir → compilar

Con el editor **cerrado** (los scripts abren `UnrealEditor-Cmd.exe`; se puede fijar otra carpeta del motor con `UE_ROOT`):

1. `Scripts\localization_gather_export.bat` → `GatherText` con `Game_Gather.ini` + `Game_Export.ini`. Recoge los `NSLOCTEXT` y
   `LOCTEXT` de `Source/Tortunabo` (menos `Private/Tests`) y los `FText` de los assets de `Content` (blueprints, widgets, tablas,
   mapas), escribe `Game.manifest` y los `Game.archive`, y exporta `Content/Localization/Game/<cultura>/Game.po` para las 13
   culturas. Informes: `Game.csv` (palabras) y `Game_Conflicts.txt` (mismo espacio y clave con textos origen distintos: **debe
   salir vacío**; un conflicto es un error de código, dos `NSLOCTEXT` con la misma clave y distinto texto).
2. **Traducir** los `Game.po` (apartado siguiente). El origen (`msgid`) es el español; `msgstr` es la traducción; `msgctxt` lleva
   «espacio,clave». No tocar `msgid` ni `msgctxt`.
3. `Scripts\localization_import_compile.bat` → `Game_Import.ini` (lee los `.po` y actualiza los `.archive`) + `Game_Compile.ini`
   (genera `Game.locmeta` y `<cultura>/Game.locres`, y valida los patrones de formato: `{0}`, plurales) + `Game_GenerateReports.ini`.
4. Abrir el juego: elegir el idioma en el menú de pausa.

**Qué se guarda en git**: `Game.manifest`, `Game.locmeta`, y por cultura `Game.archive`, `Game.po` y `Game.locres` (el `.locres`
es lo que lee el juego empaquetado). Los informes (`Game.csv`, `Game_Conflicts.txt`) no hace falta. El archivo `.po` de un idioma
sin traducir tiene `msgstr ""`: en el juego cae al español.

**Cuándo volver a recoger**: al añadir o cambiar textos (el origen cambiado deja la traducción vieja «obsoleta» y se avisa). Las
traducciones ya hechas viven en los `.archive`/`.po` y se conservan al volver a recoger.

## Escribir textos nuevos (para que se puedan traducir)

- Todo texto que se vea en pantalla es un `FText`: `NSLOCTEXT("Espacio", "Clave", "Texto en español")` o `LOCTEXT` con
  `LOCTEXT_NAMESPACE`. Nada de `FText::FromString("...")` con un literal, ni `FString` que acabe en un `TextBlock`. Espacios de
  nombres que ya hay: `TNPause` (menú de pausa), `TNSettings` (ajustes y teclas), `TNRooms` (salas), `TNRoomNames` (nombres de sala).
- Clave única y estable por texto (en un mismo espacio, misma clave = mismo texto origen: si no, sale en `Game_Conflicts.txt`).
- Nada de `+` para montar frases: `FText::Format(NSLOCTEXT(..., "Te quedan {0} intentos"), FText::AsNumber(N))`. Números, fechas y
  porcentajes con `FText::AsNumber`, `AsPercent`, `AsDate`... (formato de la cultura). Plurales: `{0}|plural(one=intento,other=intentos)`.
- El recolector solo entiende llamadas **literales**: `NSLOCTEXT("A", "B", "C")` con tres cadenas escritas ahí. Un macro que
  componga la clave o una tabla que llame a `FText::FromStringTable` desde código no se recoge (por eso los 242 nombres de sala
  están escritos uno a uno).
- Nombres de teclas (`KeyDisplayName`), texto de los HUD y avisos compuestos con `FString`: fase 2.

## Traducir con Claude (por bloques, con glosario)

La forma más económica es traducir cada `Game.po` en bloques (por espacio de nombres: `TNPause`, `TNSettings`, `TNRooms`,
`TNRoomNames`, el resto) con este glosario y estas reglas, y una **revisión nativa** después para los idiomas que importen
(inglés, francés, alemán, portugués de Brasil, japonés, chino simplificado, ruso).

**Regla de oro: adaptar el humor, no traducir literal.** Un juego de palabras se cambia por otro que funcione en el idioma de
destino («Tortugas al horno» → «Slow Roasted Shells»); una frase hecha se sustituye por su equivalente; si no hay equivalente, se
inventa un chiste del mismo tamaño y tono. El tono es cercano, gamberro y afectuoso, para todos los públicos.

**Reglas técnicas**

- Se conservan tal cual los marcadores `{0}`, `{1}`, los saltos de línea y las etiquetas; se puede cambiar su orden.
- Nombres de sala: como mucho **28 caracteres**, sin comillas ni emojis, sin repetidos dentro del idioma. Los datos de partida
  (`room_names_en.csv`) son el inglés ya adaptado.
- Los rótulos de botones y filas caben en una línea de 52 px a 19-20 pt; el alemán, el ruso y el polaco se acortan (abreviar antes que
  romper): «Ajustes» → «Einst.» solo si no cabe.
- No traducir: **Tortunavy** (la marca), «Tortunabo» (guiño técnico), los códigos de sala (`K7M2P`), las teclas físicas cuando se
  escriben como tecla (Esc, Tab, Intro se traducen solo si el teclado local lo hace), «IDIOMA / LANGUAGE» e «Idioma / Language».
- Guillemets: «...» en español, francés, italiano, ruso; “...” en inglés, alemán („...“), portugués, polaco; 「...」 en japonés y
  chino; se mantiene el estilo del idioma, con caracteres que existen en Roboto (ver Fuentes).
- Texto pensado para **oír** hablar a la tortuga (frases rápidas): breve, máximo 4 palabras.

**Glosario**

| Español | Sentido en el juego | Notas |
|---|---|---|
| Tortunavy | Nombre del juego (tortuga + navy: la tropa de tortugas) | No se traduce. |
| tortuga / cría | La protagonista, una cría de ~5 cm | Género: femenino cuando se pueda. |
| caparazón | La concha: se mete dentro (`IA_Shell`) y rueda como bola | «shell» en inglés; no confundir con «concha». |
| concha | El punto de la carrera (se ganan enteras y medias) | «seashell» / «shell» según contexto; distinto de caparazón. |
| plancha | Tirarse de barriga al suelo para esquivar o deslizarse | «belly flop / dive»; es una maniobra, no una plancha de cocina. |
| zambullida | Saltar de cabeza al agua en la meta | |
| huevo | Salida y refugio: de ahí salen, ahí esperan, el negro revive | «egg». |
| fantasma / espectador | Quien cae o llega y mira a los demás | «ghost» / «spectator». |
| ronda / sprint final / campeón | Rondas de la carrera y su desempate | |
| sala / código de sala / anfitrión / invitado | Partida en línea, su código de 5 caracteres, quien la crea / quien entra | «lobby» solo para el castillo. |
| lobby (del castillo) / puesto de mando / el General Galápago | Zona previa, con el general que da consejos | Nombre propio: «General Galápago» se adapta con el chiste (galápago = tortuga de agua). |
| gaviota, cangrejo, pulpo, gusano gigante, bañistas | Enemigos y amenazas de la playa | Los bañistas son humanos que dejan cosas (toallas, cubos): «beachgoers». |
| objeto / caja / cofre / rebuscar | Objetos de carrera (tipo karts) y de dónde salen | |
| tienda / probador / cosméticos | Personalización de la tortuga | |
| expulsar / cerrar la sala | Acciones del anfitrión | |

## Fuentes

**Qué usa la interfaz.** Todos los widgets de código (HUD, menús de pausa, salas, tienda, briefing, carrera) sacan la fuente de
`TNHUDStyle::Font` → `TNHUDFonts::Make`: la **fuente compuesta del motor** (`FCoreStyle::GetDefaultFont`) con estos pesos: Regular,
Bold, Black, Medium, Light... El motor la monta así (`Engine/Content/Slate/Fonts`, viene en el motor instalado; los `.ttf` no están
en el repositorio):

- **Roboto** (Regular, Bold, Black, Medium, Light e itálicas; ≈ 160 KB cada una): latín, latín extendido y cirílico.
- **Droid Sans Fallback** (`DroidSansFallback.ttf`, 3,9 MB, 34.492 caracteres): fuente de reserva de último recurso (japonés,
  coreano, chino), con un solo peso.
- Noto Naskh Arabic UI (árabe) y las de signos de recuadro/anchura completa.
- Las fuentes de japonés/coreano/chino simplificado que trae el editor (`GenEiGothicPro`, `NanumGothic`) **no** salen en el juego
  empaquetado (son de `Engine/Content/Editor` y solo el editor las carga).

**Cobertura comprobada** con `fontTools` 4.62 sobre esos archivos (y con `Scripts/tools/check_font_coverage.py`, que se puede
repetir cuando haya `.po`):

| Idioma | Roboto | Reserva del motor (Droid Sans Fallback) | Resultado |
|---|---|---|---|
| es, en, fr, de, it, pt-BR | Todas las letras y signos (á é í ó ú ü ñ ¿ ¡ « » — … €, à â ç è ê î ô œ ù û ÿ, ä ö ü ß, ã õ...) | – | Cubierto. Único hueco: **ẞ** (U+1E9E, ß mayúscula), que no sale en ninguna: evitarla (`ToUpper` deja la ß). |
| pl, tr | Todas (ą ć ę ł ń ó ś ź ż; ç ğ ı İ ö ş ü) | – | Cubierto. |
| ru | Alfabeto completo (А-я, Ёё) | – | Cubierto. |
| ja, ko, zh-Hans, zh-Hant | Nada | Kana, hangul y hanzi de las muestras: todo | **Se ven, pero con la reserva del motor**: un solo peso (las negritas salen finas), un único diseño para los hanzi (sin variantes japonesa, coreana o china) y trazo pobre a tamaños pequeños. Correcto para probar y para salir del paso; para publicar conviene una fuente propia. |

**Lo que está preparado.** `TNHUDFonts` monta la fuente compuesta de la interfaz: copia la del motor y, por cada idioma de la
lista con `FontRegular`, **si el archivo existe** en `Content/Slate/Fonts`, añade una fuente de reserva para esos caracteres y
solo cuando el juego está en ese idioma (`FCompositeSubFont::Cultures` + `CharacterRanges`; el motor da prioridad a las
fuentes de la cultura actual y deja la reserva del motor para los caracteres que falten). Sin archivo no cambia nada. La
lista de serie ya apunta a `NotoSansJP/KR/SC/TC-Regular.ttf` y `-Bold.ttf`; basta dejarlos en `Content/Slate/Fonts` (el
empaquetado ya los copia como archivos sueltos: `DirectoriesToAlwaysStageAsNonUFS` en `DefaultGame.ini`) y reiniciar. Para
comprobarlo: en el log sale «[Fuentes] ja: fuente de reserva NotoSansJP-Regular.ttf». Quedan fuera (usan la fuente del motor
directamente): la pantalla de carga del huevo (`STN_EggLoadingScreen`) y el «PUM» del huevo fantasma (`TN_GhostHatchWidget`); si
alguna vez enseñan texto traducido en CJK, habría que pasarlos por `TNHUDFonts::Make`.

**Fuentes que harían falta (nada descargado; pedir permiso antes).** Todas con licencia **SIL Open Font License 1.1** (uso
comercial y redistribución permitidos, sin pagar; hay que llevar el texto de la licencia en los créditos):

| Fuente | Para | Archivos | Tamaño aprox. | De dónde |
|---|---|---|---|---|
| Noto Sans JP | Japonés | `NotoSansJP-Regular.ttf`, `NotoSansJP-Bold.ttf` | 5-6 MB cada uno | Google Fonts (fonts.google.com/noto/specimen/Noto+Sans+JP, «Get font» → carpeta `static`) o el repositorio `notofonts/noto-cjk` |
| Noto Sans KR | Coreano | `NotoSansKR-Regular.ttf`, `NotoSansKR-Bold.ttf` | 5-6 MB cada uno | Igual, Noto+Sans+KR |
| Noto Sans SC | Chino simplificado | `NotoSansSC-Regular.ttf`, `NotoSansSC-Bold.ttf` | 8-10 MB cada uno | Igual, Noto+Sans+SC |
| Noto Sans TC | Chino tradicional | `NotoSansTC-Regular.ttf`, `NotoSansTC-Bold.ttf` | 6-8 MB cada uno | Igual, Noto+Sans+TC |

En total unos **45-65 MB** para los ocho archivos. Para no engordar el juego: recortarlos con `pyftsubset` (de `fontTools`, que ya
está instalado) a los caracteres que salgan en las traducciones más los de uso común (unos 2.500-4.000 caracteres): quedan en
0,5-1,5 MB cada uno y los caracteres raros (un nombre de jugador escrito en chino) siguen saliendo con la reserva del motor. Opciones
más ligeras si no gustan: *M PLUS Rounded 1c* (japonés, redondeada, OFL; encaja con el estilo del juego) y *Nanum Gothic* (coreano,
OFL, 4 MB). Latín extendido y cirílico **no** hacen falta (Roboto los cubre). Para una escritura nueva (árabe con formas, tailandés,
hindi) habría que ver *Noto Sans Arabic*, etc.

## Pendiente de la fase 2 (auditoría de textos)

Ya se sabe que no se traduce todavía, porque no pasa por `FText` localizable:

- `UTN_GameSettingsSubsystem::KeyDisplayName`: nombres de teclas («Espacio», «Clic izquierdo», «Stick izquierdo»...) con `FText::FromString`.
- `UTN_GameSettingsSubsystem::ActionLabel`: las acciones sin nombre conocido usan el nombre del asset.
- Textos con `FString` que acaban en pantalla (`ShowLoadingScreen`, `UpdateStatus` y otros en `MP_GameInstance`), la pantalla de carga del huevo,
  los HUD de la carrera y el cooperativo, la tienda, el briefing y los `TextBlock` de los blueprints (los recoge el paso de assets).
- Números y fechas escritos con `FString::Printf` en lugar de `FText::AsNumber`.
- Las pruebas automáticas (`Private/Tests`) no se recogen a propósito: no salen en pantalla.

La forma de encontrarlos: `-culture=LEET` (todo lo que se vea «normal» no es localizable) y `Game_Conflicts.txt`.

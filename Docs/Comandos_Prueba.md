# Comandos de prueba

Chuleta para probar cada cosa sin tener que llegar a ella. Se escriben en la consola de la ventana del **anfitrión**
(en PIE con varios jugadores, la del servidor). Los índices de jugador empiezan en 0 (0 = el anfitrión). Estado:
rama `claude/modo-carrera` a 28-09-2026.

## Abrir y moverse

| Comando | Qué hace |
|---|---|
| `open LVL_BeachRace?BeachSeed=42` | Carrera en la playa sin pasar por el lobby, con una semilla fija (misma ronda siempre). |
| `open LVL_BeachRace?BeachWins=1` | Partida a 1 concha (se llega antes al campeón). Se combinan: `?BeachSeed=42?BeachWins=1`. |
| `TN.Mode Coop` / `TN.Mode Race` | Modo de la próxima partida que salga del lobby (sin argumento dice el actual). También en la pestaña «Misión» del general. |
| `fly` / `walk` | Volar con la tortuga para ir a cualquier sitio y volver a andar (trucos del motor, en PIE). |
| `ghost` | Volar atravesando todo. |
| `teleport` | Te lleva a donde miras. |
| `slomo 0.3` / `slomo 1` | Cámara lenta para ver saltos, gusanos o gaviotas; `1` vuelve a velocidad normal. |
| `stat fps` / `stat unit` | Fotogramas y tiempos. |

## Reparto y dificultad de la playa

Van en la ventana del anfitrión o en la de un cliente del PIE (el servidor mueve su tortuga). A un cliente remoto de
verdad no le llegan: escríbelos en el anfitrión con el índice del jugador. La ronda se monta por partes: lo que la
necesita espera a que esté lista.

| Comando | Qué hace |
|---|---|
| `TN.Beach.Go <sitio> [jugador]` | Lleva tu tortuga (o la de ese jugador) allí, sobre arena libre y mirando a lo que toca: `salida` (su huevo), `fortaleza` (delante de la puerta de la siguiente por delante; si no hay más, la primera), `trinchera` (12 m antes de la siguiente), `poza` (en su orilla), `cresta` (al pie de la cara empinada de la siguiente con cornisa), `sprint` (su sitio de la línea del sprint final), `acantilado` (25 m antes del filo), `meta` (a 3,5 m del filo: un paso y al agua) o un número: esos metros desde la salida (`TN.Beach.Go 600`). La saca de la bola o del caparazón. |
| `TN.Race.Difficulty <Easy\|Normal\|Hard>` | Dificultad del reparto desde la próxima ronda (también `fácil`, `normal`, `difícil`; sin argumento dice la de la ronda y la siguiente). Fácil: ayudas x1,6, trampas x0,7, enemigos x0,6; Difícil: enemigos x2,5, trampas x1,8, ayudas x1,4. |
| `TN.Beach.Reroll [semilla]` | Rehace la ronda ya con la dificultad actual (semilla al azar si no se da). Los huevos vuelven a cerrarse; cuando está lista, dice cuántos elementos, fortalezas y cofres tiene. `TN.Race.Difficulty Hard` y luego `TN.Beach.Reroll 42` para ver la misma semilla en Difícil. |
| `TN.Beach.ShowFootprints 1` | Huellas del reparto en el suelo (colores en `Docs/Modo_Carrera.md`, «Nivel y capturas»). |

## Poner cualquier pieza de la playa delante de ti

`TN.Beach.Place <Elemento|número> [Tamaño=1] [Extent=0] [Semilla]` crea el elemento delante de tu tortuga;
`TN.Beach.Place clear` borra los creados así. `TN.Beach.ShowFootprints 1` enseña las huellas del reparto.

- **Trampas:**
  - `BarbedWire`: alambre de espino (Extent = largo en cm).
  - `Seaweed`: algas.
  - `WobblyPlatform`: plataforma sobre un hoyo.
  - `BrokenBucket`: cubo roto.
  - `SpadeRamp`: pala.
  - `SandDungeon`: castillo con salas.
  - `ShellGate`: puerta de conchas.
  - `ClamTrap`: concha que atrapa.
  - `MovingPlatform`: plataforma móvil. Con semilla par sale la balsa (`TN.Beach.Place MovingPlatform 1 0 2`) y con impar, el ascensor con catapulta (`… 1 0 3`).
  - `Catapult`: catapulta (de un solo uso: tras el primer disparo queda partida hasta la ronda siguiente; para otra, ponla otra vez).
  - `Mine`: mina.
  - `Trampoline`: trampolín (4 variantes por semilla).
  - `FortressMedium`, `FortressLarge`, `FortressColossal`: fortaleza de arena con premio en la cima (lanzador potenciado, cofre y conchas de 50 y 100). Mira hacia donde miras (lanza hacia allí) y sale lejos: la colosal, a ~54 m. Por ejemplo `TN.Beach.Place FortressColossal 1 0 7`; semillas seguidas cambian catapulta o trampolín y el lado de la espiral.
- **Lanzadores potenciados** (los de la cima de las fortalezas): `TN.Beach.PlaceBoosted <Catapult|Trampoline> [Tamaño=1] [Semilla]`, delante de ti y mirando hacia donde miras. `TN.Beach.Place clear` también los borra.
- **Subir a la cima:** `TN.Beach.Fortress.Top [jugador=0]` lleva a esa tortuga a la cima de la fortaleza más cercana, detrás del lanzador y mirando hacia él.
  - `TreasureChest`: cofre (mira hacia donde mira tu tortuga; `TN.Beach.Chest` lo pone con el frente hacia ti).
- **Enemigos:**
  - `GiantCrab`: cangrejo gigante.
  - `SeaUrchin`: erizo.
  - `Lizard`: lagarto.
  - `QuadLane`: paso de quads (Extent = ancho).
  - `GullZone`: zona de gaviotas.
  - `HermitCrab`: cangrejo ermitaño bola (Extent = largo de su calle; 0 = 40 m). La calle va hacia donde miras, centrada 13 m delante; lo alto (donde espera) es el extremo más cercano a ti. Con `TN.Beach.Place HermitCrab 1 2000` queda 3 m delante y la calle llega a 23 m: rodéalo y ponte en su calle, delante de él.
  - `PoolOctopus`: pulpo de poza. Dentro de una poza vive en ella (si cae a menos del 70 % de la orilla, se queda ahí); en la arena hace su propio charco de 7 m de radio y cualquier tortuga que entre cuenta como nadando.
  - `SandFleas`: enjambre de pulgas de arena.
  - `ToyTank`: tanque de juguete (Extent = tramo de patrulla; 0 = 24 m, hacia donde miras).
- **Decorado militar:** `Sandbags`, `AmmoCrate`, `TankTrap`, `MilitaryHelmet`, `CamoNet`, `Jerrycan`, `ToySoldiers`.
- **Decorado:**
  - Castillos y estructuras: `SandCastleSmall`, `SandCastleHuge`, `ShipSailWreck`, `Boardwalk`, `WoodenPostPath`.
  - Piedras y restos de playa: `Coconut`, `Rock`, `RockCluster`, `MossyLog`, `OldPlanks`, `FishingNet`, `Driftwood`.
  - Basura y objetos: `PlasticCup`, `Bottle`, `SodaCan`, `BeachBall`, `ToyBucket`, `BeachChair`, `PlantedUmbrella`, `RubberDuck`… (lista completa en `TN_BeachTypes.h`).

## Enemigos, tormenta y gusano

| Comando | Qué hace |
|---|---|
| `TN.Beach.Gull.Attack 1` | Cada zona de gaviotas suelta una cagada sobre la tortuga más cercana (ragdoll y mancha). |
| `TN.Beach.Gull.Attack 2` | Picado con agarre: te sube pataleando y te suelta en bola. Si te apartas (o te metes en bola), baja igual, pica la arena y vuelve a subir. Sin número, al azar. |
| `TN.Beach.Quad.Now` | Todos los pasos de quads avisan y pasan ya. |
| `TN.Beach.Storm.Start [metros detrás=30] [cm/s=180]` | Arranca la tormenta de bañistas. |
| `TN.Beach.Storm.Here` | Pone el frente de la tormenta 4 m por delante de tu tortuga: te quedas dentro y un bañista te da la patada (en bola, 20 m por delante del frente). Sin tormenta, crea una. |
| `TN.Beach.Storm.Stop` | La para. |
| `TN.Beach.Storm.Info` | Distancia y velocidad de la tormenta respecto a la última tortuga. |
| `TN.Beach.Lizard <huidizo\|generoso\|mordedor>` | Un lagarto de ese carácter 22 m delante de ti, mirándote. El generoso (motas doradas) deja premio al huir; el mordedor (cresta roja) se lanza a morderte. `TN.Beach.Place clear` lo quita. |
| `TN.Beach.StunNearest [segundos=3]` | Marea al enemigo más cercano a tu tortuga (pajaritos, sin atacar). Los quads, no. Con el ermitaño rodando lo para en seco; con el pulpo agarrando, suelta; las pulgas se dispersan; el tanque echa humo y la antena da vueltas. |
| `TN.Beach.Worm [jugador=0]` | Un gusano de arena se come ya a esa tortuga. |
| `TN.Beach.Enemy.Stats` | Cuántos enemigos hay y cuántos van a ritmo lento por estar lejos. |
| `TN.Beach.Enemy.Debug 1` | Dibuja radios de visión, oído y patrulla, y estados. |

## Carrera: rondas, meta y pantallas

| Comando | Qué hace |
|---|---|
| `TN.Race.WinRound [jugador=0]` | Ese jugador «toca el agua». La primera gana la concha y arranca la cuenta de 10 s; repítelo con otro índice para la media concha; deja a alguien sin llegar para ver el gusano. |
| `TN.Race.Sprint [jugador] [jugador]…` | Empate forzado a 3 conchas y sprint final (por defecto, 0 y 1). |
| `TN.Race.Champion [jugador=0]` | Salta directo a la pantalla del campeón con ese ganador. |
| `TN.Race.PlayAgain` / `TN.Race.ChangeMode` / `TN.Race.Menu` | Los botones de la pantalla del campeón. |
| `TN.Race.Stun [segundos=3] [jugador=0]` | Aturde en bola a esa tortuga. |
| `TN.Race.Kill [jugador=0]` | Pasa por la ruta de «muerte» (en la carrera, aturde). |
| `TN.Race.Void [jugador=0]` | La tira al vacío: vuelve a su último sitio seguro aturdida. |
| `TN.Race.Splash [tamaño=1]` | Chapuzón de meta delante de ti (solo en tu pantalla). |
| `TN.Beach.Egg` | Cierra otra vez los huevos (de la salida o del sprint) con cada tortuga dentro y a los 1,5 s repite la salida: se rompen, 1 s en el huevo (de pie, sacudiéndose la cáscara y mirando al mar) y salen lanzadas. Sin cambiar de ronda. |

### Vistas previas sin jugar

| Comando | Qué enseña |
|---|---|
| `TN.Race.CountdownPreview` | La cuenta de 10 s con su «¡TIEMPO!». |
| `TN.Race.Tally [ganador 0-5, -1 nadie] [jugadores 1-6] [1 = corona y podio] [medias=1]` | El recuento, con las medias conchas; por ejemplo `TN.Race.Tally 0 4 0 2`. |
| `TN.Race.SprintPreview [finalistas 2-6]` | El título del sprint final con el «VS». |
| `TN.Race.Podium [jugadores 1-3]` | La pantalla del campeón con el podio animado. |
| `TN.Race.PreviewOff` | Cierra cualquier vista previa. |

## Botín, brillo y conchas

| Comando | Qué hace |
|---|---|
| `tn.Search.Luck 1` | Rebuscar siempre da objeto; con `0` nunca da. `-1` vuelve a la probabilidad del actor. |
| `tn.Search.Seconds 0.3` | Rebuscar dura 0,3 s en lugar de lo normal; `-1` vuelve a lo del actor. |
| `tn.Search.Show 1` | Baliza y huella en cada decorado rebuscable a menos de 300 m (en la playa, solo los que tienen su actor ahora: los que están cerca de alguna tortuga). |
| `TN.Beach.Loot.Reroll` | Quita el botín de la ronda (rebuscables, objetos y conchas) y lo vuelve a repartir. |
| `TN.Beach.Loot 0` | Sin botín desde la ronda siguiente; con `1` vuelve. |
| `TN.Beach.Chest` | Un cofre de la playa delante de ti, con el frente hacia tu tortuga: 5,5 s manteniendo E, dos objetos de los mejores y seis conchas de puntos. `TN.Beach.Place clear` lo quita. |

### Rendimiento de la playa

| Comando | Qué hace |
|---|---|
| `TN.Beach.Perf` | Tiempos de la última ronda (reparto, asientos, decorado, actores, botín y fotogramas), decorado local (piezas, instancias, con colisión y con sombra, partes que se mueven, componentes), actores de la playa (con dormancy, siempre relevantes, relevancia media), rebuscables (puntos, usados, actores ahora), objetos, conchas y la lista de red. En la ventana donde se escribe y, en PIE, también el servidor. |
| `TN.Beach.BuildBudgetMs 6` | Milisegundos por fotograma para montar la ronda (asientos, decorado local y actores). |
| `TN.Beach.AsyncBuild 0` | Monta la ronda entera en un fotograma, como antes (para comparar); `1` vuelve a por partes. |

## Fantasma espectador y volver a la vida

| Comando | Qué hace |
|---|---|
| `TN.Ghost.Become [jugador=0]` | Ese jugador pasa a fantasma espectador en cualquier modo, también en el lobby. Controles: ←/→ o LB/RB cambian de tortuga; C o R3 alterna cámara libre y fija. |
| `TN.Ghost.Revive [jugador]` | Solo en el cooperativo y el lobby: el fantasma vuela en U, se mete en un huevo y sale. En la carrera solo avisa. |

## Menú de pausa

Escape en el juego y Tabulador en el editor (PIE); Start en el mando. No tiene comandos. La lista de pruebas está en
`Docs/Menu_Pausa.md`.

## Pantalla de carga del huevo

| Comando | Qué hace |
|---|---|
| `TN.Loading.Test` | Cierra el huevo y lo rompe a los 3 s. |
| `TN.Loading.Test.Close` | Lo cierra y lo deja cerrado. |
| `TN.Loading.Test.Break` | Rompe el huevo que esté a la vista. |
| `TN.Loading.Test.Open` | Lo abre sin romperlo. |
| `TN.Loading.Test.Go` | Cierra y rompe con «¡ADELANTE!». |
| `TN.Loading.Test.GoOnly` | Solo el «¡ADELANTE!». |

## Cooperativo (mapa procedural y lobby)

| Comando | Qué hace |
|---|---|
| `TN.Proc.StartStyle 0` / `TN.Proc.StartStyle 1` | Salida por puerta doble (`0`) o con huevos (`1`) desde la siguiente generación del mapa; `-1` = lo del lobby. |
| `TN.Proc.Egg` | Con la salida con huevos: los cierra otra vez con cada tortuga dentro y a los 1,5 s los vuelve a romper (1 s en el huevo y salen despedidas), sin regenerar el mapa. |
| `TN.Fauna.Enable 0` / `TN.Fauna.Stats 1` | Esconde la fauna ambiental / saca sus métricas en el log. |
| `TN.Lobby.Castle 0` / `TN.Lobby.Valley 0` | Esconde el castillo o el valle del lobby (al recargarlo). |
| `TN.Storm.Cough 1` / `TN.Storm.Cough 2` | Carraspeos sueltos (`1`) o tos fuerte (`2`) de la tormenta sin tormenta; `0` la apaga. |

## Tortuga: cara, voz, HUD y panzazo

| Comando | Qué hace |
|---|---|
| `tn.Face.Mood 0-3` | Fuerza la cara: 0 feliz, 1 cansada, 2 jadeando, 3 tumbada. `-1` = la real. |
| `tn.Face.Tongue 0-3` | Fuerza la lengua: 0 dentro, 1 al viento, 2 colgando, 3 la punta. `-1` = la real. |
| `tn.Face.Talk 1` | Todas mueven la boca como si hablaran. |
| `tn.HUD.Face 0-5` | Fuerza la cara del HUD: 0 feliz, 1 cansada, 2 jadeando, 3 caparazón, 4 mareada, 5 victoria. `-1` = la real. |
| `tn.HUD.Energy 0.3` | Fuerza la energía del salvavidas (0-1). `-1` = la real. |
| `tn.HUD.Talk 1` | Fuerza el bocadillo de voz; `0` lo apaga. `-1` = el real. |
| `tn.HUD.CrewPreview 3` | Rellena 3 filas de tripulación contigo para ver el diseño. |
| `TN.Voice.Steps 1` / `TN.Voice.Steps 2` | Pasos de prueba en el sitio: `1` andando, `2` corriendo; `0` los apaga. |
| `TN.Voice.Pant 1` / `TN.Voice.Pant 2` | Jadeo de prueba: `1` suave, `2` agotada; `0` lo apaga. |
| `TN.Voice.Drag 1` / `TN.Voice.Drag 2` | Arrastre de panzazo de prueba: `1` lento, `2` rápido; `0` lo apaga. |
| `TN.Dive.Debug 1` | Datos del deslizamiento del panzazo. |
| `TN.Music.Play Victoria` | Hace sonar una pista: `Victoria`, `Derrota`, `Eliminado`, `Tienda`, `Probador` o `Silencio`. |

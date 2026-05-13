<style>
@import url('https://fonts.googleapis.com/css2?family=Inter:ital,wght@0,300;0,400;0,500;0,600;0,700;1,400&family=JetBrains+Mono:wght@400;500&display=swap');

:root {
  --navy:      #0d1b2a;
  --navy-mid:  #1c3048;
  --gold:      #c9a845;
  --gold-lt:   #f0d685;
  --gold-fade: rgba(201,168,69,.10);
  --surface:   #f2f4f7;
  --border:    #d8dce6;
  --muted:     #5a6a80;
  --text:      #1a2030;
  --white:     #ffffff;
  --red:       #b83232;
  --code-bg:   #0f1720;
  --code-fg:   #93c5fd;
  --easy:      #2d6a4f;
  --medium:    #c97b1a;
  --hard:      #9b2335;
}

body, .markdown-body {
  font-family: 'Inter', 'Segoe UI', Arial, sans-serif;
  font-size:   10.5pt;
  line-height: 1.72;
  color:       var(--text);
  background:  var(--white);
  max-width:   860px;
  margin:      0 auto;
  padding:     0 56px 64px;
}

/* ── Cover ── */
.cover {
  display:       flex;
  align-items:   stretch;
  background:    var(--navy);
  margin:        0 -56px 44px -56px;
  border-bottom: 5px solid var(--gold);
  min-height:    210px;
  overflow:      hidden;
}
.cover-left {
  flex:           1;
  padding:        36px 40px 36px 56px;
  display:        flex;
  flex-direction: column;
  justify-content: center;
}
.doc-label {
  font-size:      8pt;
  font-weight:    600;
  letter-spacing: 2.5px;
  text-transform: uppercase;
  color:          var(--gold-lt);
  display:        block;
  margin-bottom:  10px;
}
.cover-left h1 {
  font-size:   28pt;
  font-weight: 700;
  color:       var(--white);
  margin:      0 0 20px 0;
  letter-spacing: -0.5px;
  line-height: 1.1;
  border:      none;
  background:  transparent;
  padding:     0;
}
.cover-right {
  width:    260px;
  flex-shrink: 0;
  overflow: hidden;
  position: relative;
}
.cover-right img.cover-art {
  width:           100%;
  height:          100%;
  object-fit:      cover;
  object-position: center top;
  display:         block;
}
.art-credit {
  position:  absolute;
  bottom:    6px;
  right:     8px;
  font-size: 7pt;
  color:     rgba(255,255,255,0.6);
  font-style: italic;
}

/* ── Meta-table en el cover (NO hereda estilos generales) ── */
table.meta-table {
  width:           auto;
  border-collapse: collapse;
  font-size:       9pt;
  margin:          0;
  background:      transparent;
}
table.meta-table td {
  padding:    5px 12px 5px 0;
  border:     none;
  color:      var(--white);
  background: transparent !important;
  line-height: 1.45;
  vertical-align: top;
}
table.meta-table td:first-child {
  font-weight:    700;
  color:          var(--gold-lt);
  width:          110px;
  font-size:      8pt;
  text-transform: uppercase;
  letter-spacing: 0.6px;
  white-space:    nowrap;
}
table.meta-table tr,
table.meta-table tbody tr,
table.meta-table tbody tr:nth-child(even),
table.meta-table tbody tr:hover,
table.meta-table tr:hover td { background: transparent !important; }

/* ── H1 (fuera del cover, no debería aparecer) ── */
h1 {
  font-size:   22pt;
  font-weight: 700;
  color:       var(--white);
  background:  var(--navy);
  margin:      0 -56px 40px -56px;
  padding:     40px 56px 32px;
  border-bottom: 5px solid var(--gold);
}

/* ── H2 — secciones ── */
h2 {
  font-size:      9pt;
  font-weight:    700;
  color:          var(--white);
  background:     var(--navy);
  padding:        8px 16px 8px 20px;
  margin:         40px 0 18px 0;
  border-left:    4px solid var(--gold);
  text-transform: uppercase;
  letter-spacing: 1.2px;
}

/* ── H3 ── */
h3 {
  font-size:    10.5pt;
  font-weight:  600;
  color:        var(--navy);
  padding-bottom: 5px;
  border-bottom: 2px solid var(--gold);
  margin:       28px 0 14px 0;
}

/* ── H4 ── */
h4 {
  font-size:   10pt;
  font-weight: 600;
  color:       var(--navy-mid);
  margin:      20px 0 8px 0;
}

/* ── Tablas generales ── */
table {
  width:           100%;
  border-collapse: collapse;
  font-size:       9.5pt;
  margin:          16px 0 22px 0;
}
thead tr { background: var(--navy); color: var(--white); }
thead th {
  padding:        8px 12px;
  text-align:     left;
  font-weight:    600;
  font-size:      8.5pt;
  text-transform: uppercase;
  letter-spacing: 0.4px;
  border:         1px solid var(--navy-mid);
}
tbody td {
  padding:        7px 12px;
  border:         1px solid var(--border);
  vertical-align: top;
  line-height:    1.55;
}
tbody tr:nth-child(even) { background: var(--surface); }
tbody tr:hover           { background: #e5e9f2; }
tbody td:first-child     { font-weight: 500; }

/* ── Tier badges ── */
.easy   { color: var(--easy);   font-weight: 700; }
.medium { color: var(--medium); font-weight: 700; }
.hard   { color: var(--hard);   font-weight: 700; }

/* ── Blockquotes = Description ── */
blockquote {
  border-left:   3px solid var(--gold);
  background:    var(--gold-fade);
  margin:        16px 0;
  padding:       10px 18px;
  color:         var(--muted);
  font-size:     9.5pt;
  font-style:    italic;
  border-radius: 0 3px 3px 0;
}
blockquote p { margin: 0; }

/* ── Código ── */
pre {
  font-family:   'JetBrains Mono', 'Consolas', monospace;
  font-size:     8pt;
  background:    var(--code-bg);
  color:         var(--code-fg);
  padding:       16px 20px;
  border-left:   3px solid var(--gold);
  border-radius: 0 4px 4px 0;
  overflow-x:    auto;
  line-height:   1.6;
  margin:        14px 0;
}
code {
  font-family:   'JetBrains Mono', 'Consolas', monospace;
  font-size:     85%;
  background:    #eef0f6;
  color:         var(--red);
  padding:       1px 5px;
  border-radius: 3px;
}
pre code { background: transparent; color: inherit; padding: 0; font-size: inherit; }

/* ── Listas ── */
ul, ol { padding-left: 22px; margin: 6px 0; }
li     { margin-bottom: 5px; line-height: 1.65; }
ul > li::marker { color: var(--gold); font-weight: 600; }

/* ── HR ── */
hr { border: none; border-top: 1px solid var(--border); margin: 36px 0; }

/* ── Negrita ── */
strong { font-weight: 600; color: var(--navy); }

/* ── Imágenes del nivel ── */
.sketch-figure { margin: 20px 0; text-align: center; }
.sketch-figure img {
  max-width:     100%;
  border:        1px solid var(--border);
  border-radius: 4px;
  border-left:   3px solid var(--gold);
}
.sketch-figure figcaption {
  font-size:  8.5pt;
  color:      var(--muted);
  font-style: italic;
  margin-top: 6px;
}
.placeholder-box {
  background:    var(--surface);
  border:        2px dashed var(--border);
  border-radius: 4px;
  padding:       28px 20px;
  text-align:    center;
  color:         var(--muted);
  font-size:     9pt;
  font-style:    italic;
  margin:        16px 0;
}

/* ── Appendix header ── */
.appendix-header {
  background:    var(--navy-mid);
  color:         var(--white);
  padding:       14px 20px;
  margin:        48px 0 24px 0;
  border-left:   4px solid var(--gold);
  font-size:     9pt;
  font-weight:   700;
  letter-spacing: 1.5px;
  text-transform: uppercase;
}

/* ── Print / PDF ── */
@media print {
  @page { size: A4; margin: 16mm 14mm 18mm 14mm; }
  body, .markdown-body { padding: 0; max-width: 100%; font-size: 9.5pt; }
  .cover { margin: 0 0 28px 0; }
  h2 { break-after: avoid; }
  h3 { break-after: avoid; }
  table { break-inside: avoid; }
  blockquote { break-inside: avoid; }
  pre { white-space: pre-wrap; word-break: break-word; }
}
</style>

<div class="cover">
  <div class="cover-left">
    <span class="doc-label">Level Design Document</span>
    <h1>Tortunabo</h1>
    <table class="meta-table">
      <tr><td>Nivel</td><td>Sistema de Carrera — Chunks Procedurales</td></tr>
      <tr><td>Versión</td><td>3.0</td></tr>
      <tr><td>Fecha</td><td>Mayo 2026</td></tr>
      <tr><td>Autores</td><td>José Antonio Mota &middot; Rodrigo Fernández Carnicer</td></tr>
      <tr><td>Motor</td><td>Unreal Engine 5.6</td></tr>
      <tr><td>Ref. GDD</td><td>DayT_GDD_Final.pdf</td></tr>
    </table>
  </div>
  <div class="cover-right">
    <img class="cover-art" src="TURTLE_IMG" alt="Tortunabo — artwork">
    <span class="art-credit">Iván Burgueño</span>
  </div>
</div>

## Project Overview

- **Título:** Tortunabo
- **Tipo:** Corredor lineal cooperativo (1–4 jugadores) estructurado en **5 rondas**, cada una compuesta por **6 módulos** generados de forma semialeatoria dentro de una curva de dificultad prediseñada. Entre rondas el grupo regresa al Lobby.
- **Condición de victoria:** Completar las 5 rondas sin que el grupo sea eliminado. La derrota reinicia la partida desde la Ronda 1 — no hay checkpoints.
- **Rejugabilidad:** Cada módulo de dificultad se elige de forma aleatoria dentro del nivel de dificultad asignado para esa posición, por lo que la secuencia exacta varía en cada partida aunque la curva general sea conocida.
- **Design Goals:**
  - Mantener presión cooperativa constante sin que el juego la explique con tutoriales
  - Escalar la dificultad mediante complejidad de coordinación, no solo cantidad de daño o velocidad de enemigos
  - Generar variedad de partida a partida a través de módulos aleatorios dentro de curvas de dificultad prediseñadas — el jugador anticipa el tipo de desafío, no el módulo concreto
  - Que cada ronda dure entre 4 y 8 minutos con cuatro jugadores, con muertes que se sienten graciosas y no punitivas
  - Que cada elemento del entorno comunique sus reglas visualmente antes de que el jugador interactúe con él
  - Que el Lobby entre rondas sea un espacio de descompresión activa, no una pantalla de carga

> Tortunabo no tiene un nivel único. Tiene un sistema que ensambla módulos de diseño validados en secuencias distintas cada partida, dentro de una progresión de 5 rondas con dificultad creciente. Este documento describe ese sistema: qué elementos existen, cómo se comportan, cómo escalan y qué experiencia producen en conjunto.

---

## Level Objectives

- **Objetivo de partida:**
  - Completar las 5 rondas sin que el grupo sea eliminado. La derrota en cualquier ronda reinicia la partida desde la Ronda 1.

- **Objetivo de ronda:**
  - Que al menos un jugador alcance el módulo de victoria (módulo 6) al final de la secuencia. Si el grupo completo es eliminado antes, la ronda se pierde y la partida reinicia.

- **Objetivos secundarios (emergen de la situación, no aparecen en pantalla):**
  - Rescatar compañeros caídos antes de que el grupo llegue al módulo de victoria
  - Gestionar ítems recogidos para usarlos en el momento de mayor necesidad, no acumularlos sin consumir
  - Mantener estamina suficiente al inicio de cada módulo para tener margen ante los primeros obstáculos
  - Coordinar la activación de mecanismos cooperativos (puzles, botones, placas) sin perder tiempo innecesario
  - Usar el tiempo de Lobby entre rondas para preparar al grupo: información del sargento, personalización, práctica

> Los objetivos secundarios nunca aparecen en pantalla. Emergen de la situación: si un compañero cayó y todavía no se abrió la puerta de un puzle, el tiempo de espera es la ventana de rescate.

---

## Narrative & Setting

- **Context:**
  - Tortunabo transcurre en una playa caótica. Un grupo de tortugas antropomórficas participa en una carrera cooperativa. No hay historia que el jugador deba seguir: el contexto lo da el entorno visual y el diseño de los enemigos. La playa es hostil por naturaleza, no porque haya un antagonista con motivación.

- **Environment:**
  - Playa tropical con luz intensa y arena dorada, con estética de circuito de entrenamiento militar de playa: obstáculos de madera, pasajes estrechos, vallas, señalización de campaña. Los chunks tienen variaciones de densidad y apertura, pero el bioma es siempre playa. El agua al fondo es la meta: llegar a ella es ganar. La presión de avance no viene del terreno sino de los bañistas que avanzan desde atrás — si el grupo se detiene demasiado tiempo, lo alcanzan y lo eliminan. Las gaviotas son fauna local que se ha vuelto agresiva. Los cangrejos defienden su territorio.

- **Themes:**
  - Caos cooperativo. La playa no es fondo decorativo; cada elemento del entorno tiene un comportamiento que el grupo tiene que leer y gestionar. El desafío no es individual: emerge de que cuatro personas intentan resolver lo mismo al mismo tiempo en el mismo espacio.

- **Tone:**
  - Frenético y ligero con estética de obstáculos militares de playa: caótico pero con ritmo. Las muertes son cómicas, no punitivas. El rescate es mecánico, no emotivo. La amenaza de los bañistas por detrás crea una tensión de fondo constante que no es agobiante — siempre hay margen, pero nunca tanto como para detenerse a descansar. Los momentos de tensión real (puzle cooperativo con separación física) se intercalan con secciones de movimiento puro donde el grupo puede respirar, siempre con los bañistas como reloj de arena visual.

- **Points of Interest:**
  - Major:
    - **El Lobby** — base de operaciones entre rondas; espacio de personalización, información, práctica y socialización. El grupo parte desde aquí al inicio de cada ronda y regresa tras completarla o perderla.
    - **La Carrera** — la secuencia de 6 módulos de cada ronda; el espacio central del juego.
    - **El Módulo de Victoria** — módulo final fijo de cada ronda; llegar a él completa la ronda. El agua al fondo es la meta visual.
  - Minor:
    - **Torres de vigilancia** — entrada del Lobby; los jugadores interactúan aquí para iniciar la misión cuando todo el grupo esté listo.
    - **Puesto de mando** — dentro del Lobby; el general da consejos y briefing de la siguiente ronda.
    - **Campo de entrenamiento** — dentro del Lobby; circuito de obstáculos para practicar mecánicas de movimiento antes de cada ronda.
    - **Zonas de ítems** — puntos fijos de consumibles dentro de cada módulo, posicionados cerca de los momentos de mayor necesidad.
    - **Transición entre módulos** — paso de un módulo al siguiente; sin pantalla de carga ni desconexión.

> Solo se lista lo que el diseñador necesita saber para construir un módulo coherente con el mundo. El setting no tiene lore adicional.

---

## Gameplay Mechanics

- **Core Mechanics:**
  - Sprint y gestión de estamina: el sprint consume estamina rápidamente. Al agotarse hay un breve bloqueo antes de que empiece a recuperarse. El sprint es el recurso más gestionado de toda la Carrera.
  - Peso e inventario: el jugador tiene dos huecos de inventario. Cada objeto tiene un peso que reduce la estamina máxima efectiva mientras se carga. Llevar una caja pesada en un puzle es una decisión con coste real.
  - Recoger, cargar y soltar objetos: el jugador puede coger ítems del suelo, guardarlos o equiparlos y lanzarlos. Las cajas de puzle no se lanzan: solo se colocan sobre las placas de presión.
  - Interactuar con el entorno: botones, placas de presión, rescate de compañeros. Todas las interacciones son de un solo botón; la complejidad está en el cuándo y el quién.
  - Derribo y rescate: un jugador derribado cae boca abajo y queda inutilizado hasta que un compañero lo recoge en su punto de caída. El tiempo en el suelo es tiempo que el grupo pierde.

- **Unique Mechanics:**
  - Progresión de 5 rondas sin checkpoints: el juego se estructura en 5 rondas de dificultad creciente. Perder una ronda reinicia desde la Ronda 1. No hay guardado entre rondas. El Lobby entre rondas es el único momento de descanso real.
  - Módulos semialeatorioss por ronda: cada ronda encadena 6 módulos. Los 4 módulos de dificultad se seleccionan aleatoriamente dentro del nivel de dificultad asignado para esa posición — el jugador sabe qué tipo de desafío le toca pero no el módulo concreto.
  - Separación espacial forzada: algunos módulos de nivel difícil dividen físicamente al grupo. Los que están en un lado no pueden llegar al otro sin resolver el mecanismo; el chat de voz por proximidad es el único canal disponible.
  - Peso como penalización de puzle: el jugador que transporta una caja llega a la placa con menos estamina que el resto. Si además hay cangrejos en la zona, decidir quién lleva la caja y quién escolta es una decisión táctica real.
  - Muerte sin agonía: la muerte es inmediata tras tres segundos en una zona de peligro. El rescate se hace sobre un objeto que aparece en el punto de caída, no sobre el cuerpo. El jugador eliminado pasa a modo espectador.
  - Bañistas como presión de avance: una horda de bañistas avanza lentamente desde el spawn inicial durante toda la ronda. No son un temporizador invisible: el jugador los ve y los escucha. Si los bañistas alcanzan a un jugador, lo eliminan. No obligan a correr sin control — obligan a no detenerse más tiempo del necesario. Son el reloj de arena visual del juego.

- **Level Design Interactives:**

| Elemento | Categoría | Tier | Descripción del efecto | Señal al jugador |
|---|---|---|---|---|
| Rampas inclinadas | Terreno | Easy | Requieren sprint sostenido para subir; sin penalización si se bordean | Visual de pendiente |
| Algas en el suelo | Hazard pasivo | Easy | Reducen velocidad de movimiento mientras el jugador las pisa | Color oscuro, textura visible |
| Erizo | Enemigo estático | Easy | Veneno al pisarlo; daño progresivo durante unos segundos | Silueta espinosa, color oscuro |
| Arenas movedizas | Hazard pasivo | Medium | Ralentizan y consumen estamina extra por segundo de contacto | Textura de arena fina, color distinto |
| Alambre de espino | Hazard pasivo | Medium | Ralentizan y aplican daño leve al atravesarlos | Textura de alambre, altura de rodilla |
| Cangrejo dinámico | Enemigo activo | Medium | Patrulla ruta fija; persigue al jugador que entra en su radio (~300 cm); vuelve cuando sale | Pinzas elevadas al activarse; color rojizo |
| Cangrejo enterrado | Enemigo oculto | Medium | Oculto bajo la arena; pinza de knockback al acercarse a menos de 100 cm | Burbujas de arena sutiles en el suelo |
| Agua (meta) | Zona de llegada | Todos | Llegar al agua es ganar; el jugador que la toca primero pasa a modo espectador | Masa de agua visible al fondo desde spawn |
| Bañistas (horda) | Presión de avance | Todos | Avanzan desde atrás a velocidad constante y lenta; eliminan al jugador al contacto; no requieren detección — son presión de zona progresiva | Masa de personas visible; ruido de grupo en aumento |
| Gaviota (estándar) | Enemigo aéreo | Hard | Agarra al jugador si está quieto más de 2 segundos; lanza un bombardeo en zona aleatoria cada ~8 s | Sombra en el suelo; graznido |
| Gaviota (hostigamiento continuo) | Enemigo aéreo | Hard | Variante sin temporizador de parada; ataca sin que el jugador tenga que estar quieto | Sombra + graznido más intenso |
| Plataformas rompibles | Hazard dinámico | Hard | Aguantan peso limitado; con varios jugadores encima caen más rápido | Textura agrietada; crujido al pisarlas |
| Sombrillas / paredes de madera | Cobertura | Todos | Obstáculos físicos que fragmentan la visibilidad y la línea de carrera directa; dan cobertura frente a gaviotas | Presencia visual clara |
| Cajas de puzle | Objeto interactuable | Hard | Se cogen y transportan (ocupan slot, penalizan estamina por peso); activan placas de presión al colocarse encima | Color contrastado; handle visual de agarre |
| Placas de presión | Interactivo de puzle | Hard | Envían señal de apertura mientras tienen peso encima; se desactivan si se retira la caja | Luz de estado (rojo / verde) |
| Quads (bloqueadores) | Bloqueador físico | Hard | Bloquean el 100% del ancho de la pista; no se pueden esquivar lateralmente; se retiran cuando se resuelve el puzle | Volumen físico grande, color contrastado |
| Botones (x3 por zona) | Interactivo de puzle | Hard | Cada botón mueve los obstáculos rotativos a un ángulo fijo; solo uno activo a la vez | Indicador de estado iluminado |
| Varas rotatorias (cruces) | Obstáculo dinámico | Hard | Bloquean o abren el paso según su ángulo; controladas remotamente por botones | Rotación continua visible; indicadores angulares en suelo |
| Botón final | Interactivo de meta | Hard | Solo accesible desde el interior de la zona dividida; abre la salida al resto del grupo | Iluminación diferenciada |

> Cada elemento del nivel comunica sus reglas antes de que el jugador lo active. Si un obstacle requiere texto explicativo para entenderse, el diseño visual ha fallado.

---

## Level Beats, Flow, and Pacing

- **Level Beats (estructura macro de partida):**

| # | Momento | Evento | Tensión |
|---|---|---|---|
| 1 | Inicio | Lobby → grupo completo en torres de vigilancia → inicio de Ronda 1 | Neutra → Anticipación |
| 2 | Ronda 1 | 6 módulos de dificultad baja. Bañistas visibles pero lejanos. El grupo aprende el lenguaje visual del juego. | Baja → Media |
| 3 | Lobby R1→R2 | El grupo regresa al Lobby. Pausa activa: sargento informa de la Ronda 2. Personalización opcional. | Descompresión |
| 4 | Ronda 2 | Módulos con primer enemigo activo (cangrejo). El grupo empieza a desincronizarse. Bañistas más perceptibles. | Media → Alta |
| 5 | Lobby R2→R3 | Pausa activa. La tensión acumulada se descarga. El sargento avisa de que la dificultad sube. | Descarga |
| 6 | Ronda 3 | Primer módulo de puzle cooperativo. Gaviota posible. Presión de bañistas ya real. | Alta, coordinación |
| 7 | Lobby R3→R4 | Pausa activa. El grupo ha perdido jugadores probablemente. La amenaza de reinicio empieza a pesar. | Tensión latente |
| 8 | Ronda 4 | Módulos Hard con separación espacial. El puzle exige comunicación verbal. Pico de exigencia. | Muy alta |
| 9 | Lobby R4→R5 | Última pausa antes del reto final. El sargento da el briefing final. | Anticipación máxima |
| 10 | Ronda 5 | Máxima dificultad. Bañistas agresivos. Módulo de puzle y módulo de victoria como cierre. | Pico absoluto |
| 11 | Victoria | Módulo de victoria superado → pantalla de resultados → Lobby de victoria | Euforia |

- **Beats dentro de cada ronda (micro):**

| # | Tiempo est. | Evento | Tensión |
|---|---|---|---|
| 1 | 0:00 | Spawn en módulo 1; bañistas aparecen al fondo y empiezan a avanzar | Neutra → Anticipación |
| 2 | 0:10 | Módulo 1 (dificultad asignada): el grupo lee el espacio; los bañistas son visibles pero lejanos | Baja, orientación |
| 3 | 1:30 | Módulos 2–4: escalada de obstáculos; primer enemigo activo probable; grupo empieza a desincronizarse | Media → Alta |
| 4 | 3:00 | Módulo 5 (puzle): el avance se detiene; el grupo coordina; bañistas acercándose perceptiblemente | Alta, coordinación |
| 5 | 4:30 | Módulo 6 (victoria): carrera final hacia el agua; los bañistas están ya cerca | Pico → Euforia |
| 6 | 5:00 | Módulo completado → regreso al Lobby | Descarga emocional |

- **Flow:**

```
[LOBBY]
     |
     v
[RONDA 1: M1 → M2 → M3 → M4 → PUZLE → VICTORIA]
     |
     v
[LOBBY]  <-- descanso activo: sargento + personalización
     |
     v
[RONDA 2: M1 → M2 → M3 → M4 → PUZLE → VICTORIA]
     |
     v
[LOBBY]
     |
     ...
     v
[RONDA 5: M1 → M2 → M3 → M4 → PUZLE → VICTORIA]
     |
     v
[PANTALLA DE VICTORIA]
     |
     v
[LOBBY]

Nota: perder cualquier ronda reinicia desde RONDA 1.
```

- **Pacing (macro — partida completa):**

```
Intensidad por ronda

  Muy alta |                                        ████
     Alta  |                          ████    ████  ████
    Media  |          ████    ████   ████    ████
     Baja  | ████
            └───────────────────────────────────────────►
             Lobby  R1  L  R2  L  R3  L  R4  L  R5  Victoria

- **Pacing (micro — dentro de una ronda):**

  Muy alta |                              ████
     Alta  |              ████   ████    ████ ████
    Media  |     ████  ████
     Baja  | ████
            └───────────────────────────────────────►
             M1    M2    M3    M4   Puzle  Victoria
```

> Los Lobbys entre rondas no son tiempo muerto: son la válvula de presión que hace sostenible una partida de 5 rondas. Sin ellos, la curva de tensión no podría escalar hasta los niveles del final sin agotar al jugador emocionalmente.

---

## Level Layout & Structure

### Sistema de Módulos

Cada ronda está compuesta por **6 módulos** en orden fijo:

```
[M1 Dificultad] -> [M2 Dificultad] -> [M3 Dificultad] -> [M4 Dificultad]
        -> [M5 Puzle] -> [M6 Victoria]
```

- **Módulos de dificultad (M1–M4):** el nivel de dificultad de cada posición está prediseñado para la ronda; el módulo concreto dentro de esa dificultad es aleatorio. Esto garantiza una curva de escalada conocida con variedad de run a run.
- **Módulo de puzle (M5):** longitud variable según el tipo de puzle. Siempre el penúltimo módulo. Fuerza una pausa de coordinación grupal antes del sprint final.
- **Módulo de victoria (M6):** módulo fijo, siempre el mismo para todas las rondas. Longitud propia. Llegar a él completa la ronda.

**Medidas estándar de módulo:**
- Longitud: ~300 m (excepto módulo de puzle y de victoria, que tienen longitud propia)
- Ancho en inicio y fin: **35 m** (regla obligatoria para todos los módulos, incluyendo puzle y victoria)
- Forma y medidas internas: variables según el diseño del módulo

### Módulos de Prototipo

Los módulos de prototipo son versiones reducidas para validar mecánicas, enemigos y layout antes de los módulos de producción. Tienen **100 m de longitud** y mantienen el ancho estándar de 35 m en inicio y fin. Solo incluyen los obstáculos y objetos más básicos: alga, gaviota, concha, medusa y pez globo.

#### Leyenda

<div class="placeholder-box">
  Leyenda de módulos — pendiente de imagen
</div>

#### Módulo Prototipo Fácil

<div class="placeholder-box">
  Sketch módulo prototipo Easy — pendiente de imagen
</div>

#### Módulo Prototipo Medio

<div class="placeholder-box">
  Sketch módulo prototipo Medium — pendiente de imagen
</div>

#### Módulo Prototipo Difícil

<div class="placeholder-box">
  Sketch módulo prototipo Hard — pendiente de imagen
</div>

### Tipos de Módulo

| Tipo | Tier | Tipo de desafío | Elementos principales | Objetivo de diseño |
|---|---|---|---|---|
| Módulos Fáciles | Easy | Navegación y lectura de terreno | Rampas, algas, erizos, sombrillas, poliqueto | El jugador aprende el lenguaje visual; sin presión real de tiempo |
| Módulos Medios | Medium | Evasión activa + penalización de terreno | Cangrejos, arenas movedizas, alambre, coco, medusa | El jugador gestiona recursos bajo presión; primera tensión de grupo |
| Módulos Difíciles | Hard | Coordinación cooperativa forzada | Gaviota, puzles de cajas/placas, varas rotatorias, separación espacial | El grupo no puede avanzar solo; la coordinación es la mecánica central |
| Módulo de Puzle | Todos | Coordinación de grupo obligatoria | Variable según tipo de puzle | Frena el ritmo y fuerza planificación grupal antes del sprint final |
| Módulo de Victoria | Fijo | Carrera final hacia el agua | Obstáculos de cierre, bañistas en máxima proximidad | Clímax de ronda: el grupo llega al agua y completa la ronda |

### Progresión de Rondas

| Ronda | Dificultad general | Módulos de dificultad (M1–M4) típicos | Bañistas |
|---|---|---|---|
| 1 | Introducción | Easy / Easy / Easy / Medium | Lentos, muy lejanos |
| 2 | Escalada inicial | Easy / Medium / Medium / Hard | Lentos, visibles |
| 3 | Presión real | Medium / Medium / Hard / Hard | Ritmo medio |
| 4 | Alta exigencia | Medium / Hard / Hard / Hard | Ritmo elevado |
| 5 | Máxima dificultad | Hard / Hard / Hard / Hard | Ritmo máximo |

### Tipología de obstáculos por tier

- **Easy:** Sin combate. Evasión de erizos estáticos, lectura de terreno. Ningún enemigo persigue.
- **Medium:** Evasión activa de cangrejos (patrulla + persecución). Cangrejo enterrado como trampa de posición. Sin puzles cooperativos.
- **Hard:** Sin combate individual relevante. El desafío es la coordinación: puzles de activación simultánea (cajas + placas), control remoto (botones + varas rotatorias), separación física forzada.

### Duraciones estimadas

| Segmento | Experimentados | Primera partida |
|---|---|---|
| Lobby → Spawn | 30–60 s | 60–120 s |
| Módulo Easy | 45–60 s | 60–90 s |
| Módulo Medium | 75–100 s | 90–150 s |
| Módulo Hard | 60–90 s | 90–180 s |
| Módulo de Puzle | 60–120 s | 90–240 s |
| Módulo de Victoria | 30–60 s | 60–90 s |
| Ronda completa (estimado) | 5–8 min | 8–14 min |
| Partida completa (5 rondas) | 25–40 min | 40–70 min |

---

## El Lobby

El Lobby es la base de operaciones entre rondas. El grupo se reúne aquí al inicio de la partida y regresa aquí después de completar o perder cada ronda. No es una pantalla de carga ni un menú: es un espacio de juego activo donde el grupo se reorganiza, se informa y se prepara.

La salida al campo está controlada por las **torres de vigilancia** en la entrada: los jugadores deben situarse en ellas para iniciar la ronda cuando el grupo esté listo.

### Sketch del Lobby

<div class="placeholder-box">
  Sketch 1 del Lobby — pendiente de imagen
</div>

<div class="placeholder-box">
  Sketch 2 del Lobby — pendiente de imagen
</div>

### Zona 1 — Personalización

**Tienda:** Los jugadores pueden canjear los puntos obtenidos en partidas para comprar accesorios cosméticos.

**Vestuarios:** Los jugadores cambian entre los cosméticos que han comprado. Separados de la tienda para evitar que la compra y el equipado se confundan como una sola acción.

### Zona 2 — Minijuegos

**Campo de tiro:** Zona de prueba donde los jugadores pueden probar cómo funcionan los objetos antes de usarlos en una ronda real. Acceso libre en cualquier momento del Lobby.

**Campo de entrenamiento:** Circuito de obstáculos donde el jugador practica las mecánicas de movimiento básicas. Mientras un jugador recorre el circuito, el resto del grupo puede acceder a una plataforma elevada desde la que lanzar objetos y dificultar el recorrido. Fomenta interacción social sin salir del espacio del Lobby.

### Zona 3 — Ambientación

**Baños:** Zona de ambientación. Función decorativa y de caracterización del entorno militar de playa.

**Puesto de mando:** El general da consejos al grupo y briefing de la siguiente ronda. Los jugadores pueden interactuar con él para obtener información sobre los módulos que les esperan: tipo de obstáculos, mecánicas nuevas que aparecen en esa ronda, etc.

**Torres de vigilancia:** Situadas en la entrada del Lobby. Desde aquí los jugadores inician la ronda cuando todo el grupo esté en posición. No funcionan como botón de menú: los jugadores físicamente se tienen que situar en ellas, como si estuvieran en una zona de salida.

| Zona | Área | Función principal | Obligatorio |
|---|---|---|---|
| Personalización | Tienda | Comprar cosméticos con puntos de partida | No |
| Personalización | Vestuarios | Equipar cosméticos comprados | No |
| Minijuegos | Campo de tiro | Probar objetos y consumibles | No |
| Minijuegos | Campo de entrenamiento | Practicar mecánicas de movimiento | No |
| Ambientación | Baños | Decoración; caracterización del entorno | — |
| Ambientación | Puesto de mando | Información del general sobre la siguiente ronda | Recomendado |
| Ambientación | Torres de vigilancia | Iniciar la ronda; punto de salida al campo | **Sí** |

> El Lobby es también el espacio donde el juego puede añadir contenido opcional sin afectar al loop central: coleccionables, álbum de logros, arena de minijuego entre jugadores (sacar al otro del círculo). Estos elementos se listan como pendientes de diseño detallado y no son obligatorios para la versión base.

---

## Enemies / NPC

- **Enemy Types:**

| Enemigo | Tier | Comportamiento | Señal visual | Señal sonora | Contramedida |
|---|---|---|---|---|---|
| Erizo | Easy | Estático en el suelo; aplica veneno al pisarlo durante unos segundos | Silueta espinosa, color oscuro contrastado | Ninguna | Esquivar lateralmente o saltar por encima |
| Cangrejo dinámico | Medium / Hard | Patrulla ruta fija; entra en modo persecución cuando el jugador cruza su radio de detección (~300 cm); vuelve a patrullar si el jugador sale del radio | Pinzas elevadas al activarse; coloración rojiza | Chasquido al detectar | Bordear la ruta de patrulla; usar a un compañero sin caja como señuelo |
| Cangrejo enterrado | Medium | Oculto bajo la arena en desvíos; saca una pinza y aplica knockback cuando el jugador se acerca a menos de 100 cm | Burbujas de arena sutiles en el suelo | Snap sorpresa | Aprender la posición por run o usar el Detector de objetos |
| Gaviota (estándar) | Hard | Agarra al jugador si permanece quieto más de 2 segundos y lo desplaza fuera de la ruta; lanza un bombardeo en área aleatoria cada ~8 s | Sombra proyectada en el suelo; graznido | Graznido + sonido de batir de alas | Movimiento constante; refugiarse en zonas de cobertura |
| Gaviota (hostigamiento continuo) | Hard | Variante de Hard Tier con `bContinuousHarassMode = true`; ataca sin necesidad de que el jugador esté quieto; hostigamiento permanente desde el spawn | Sombra + graznido constante | Graznido intenso sin pausa | Movimiento constante sin excepción; sombrillas y paredes como cobertura puntual |
| Bañistas (horda) | Todos | Avanzan lentamente desde el punto de spawn en bloque; eliminan al jugador al contacto; no persiguen ni reaccionan — son presión de zona que avanza a ritmo fijo | Masa visible desde atrás en todo momento | Voces de playa + salpicaduras en aumento de volumen | Avanzar; no detenerse más del tiempo necesario para resolver cada obstáculo |
| Quad (bloqueador) | Hard (puzle) | Estático; bloquea el 100% del ancho de la pista mientras la condición de apertura no se cumpla; se retira en secuencia animada al resolver el puzle | Volumen físico grande; color contrastado | Ninguno | No existe contramedida física; solo resolver el puzle de cajas |

- **NPC Interaction:**
  - No hay NPCs en La Carrera. Los jugadores son los únicos actores con agencia narrativa durante una ronda.
  - En el Lobby existen dos NPCs funcionales: el **general** (puesto de mando, da información sobre la siguiente ronda) y los **guardias de las torres de vigilancia** (controlan la puerta de salida al campo). Ambos son NPCs de información, no de combate.

> Los enemigos de Tier Easy son maestros del lenguaje visual: son estáticos, tienen señales claras y no persiguen. Los de Tier Medium introducen persecución activa con radio bien definido. Los de Tier Hard exigen movimiento continuo o coordinación de grupo. El escalado es de información y coordinación, no de daño bruto.

---

## Technical Requirements

- **Unique Scripting:**
  - Gestor de módulos: administra la selección aleatoria de módulos dentro de cada nivel de dificultad asignado, el encadenamiento de spawns y la transición entre módulos sin pantalla de carga. Los jugadores no se desconectan entre módulos.
  - Progresión de rondas: al completar el módulo de victoria, el sistema registra la ronda como superada y carga el Lobby. Si el grupo es eliminado, reinicia desde Ronda 1. No hay checkpoints entre rondas.
  - Bañistas: actor de horda con velocidad de avance constante configurable por ronda. Se spawnea al inicio de cada módulo y avanza independientemente de los jugadores. No tiene lógica de persecución — solo avance de zona.
  - Zonas de efecto (terreno lento, detección de placa): volúmenes simples acoplados al actor del módulo. No son actores independientes.
  - Gaviota: dos modos configurables desde el editor. Modo estándar: ataca si el jugador lleva más de 2 s quieto. Modo hostigamiento continuo: ataca permanentemente sin condición de parada.
  - Zona de peligro: cuenta 3 segundos desde que el jugador entra. Si sale antes, el contador se cancela. Al llegar a cero, aplica la muerte directamente.
  - Mecanismo de puerta cooperativa: la puerta lleva un contador de placas activas. Se abre cuando todas las placas necesarias están ocupadas simultáneamente. Si alguna se desactiva antes de que la animación termine, el contador se reinicia.
  - Torres de vigilancia (Lobby): zona de activación de ronda. La ronda arranca cuando todos los jugadores están posicionados en las torres. No hay temporizador forzado — el grupo decide cuándo salir.

---

## Design Principles

- **Design Pillars:**
  - Cooperación emergente: el juego nunca dice al jugador que coopere. Diseña situaciones donde cooperar es la solución obvia. Un puzle con dos placas simultáneas implica dos portadores. Una separación física implica comunicación. Ninguno requiere un tutorial.
  - Legibilidad sin texto: cada mechanic comunica sus reglas antes de que el jugador las active. Los erizos tienen textura espinosa visible desde lejos. El cangrejo levanta las pinzas al detectar. La gaviota proyecta sombra antes de atacar. Si hay que explicarlo con texto, el diseño visual ha fallado.
  - Presión justa: la dificultad viene de la demanda de información y coordinación, no de daño impredecible o mecánicas ocultas. El cangrejo dinámico tiene un radio de detección claro. El cangrejo enterrado tiene burbujas de arena como señal (sutil pero aprendible). La gaviota tiene un timer que el jugador puede interiorizar.
  - Variedad sin imprevisibilidad: la generación procedural crea variedad de run a run, pero dentro de reglas conocidas. El jugador sabe que después de un chunk Medium viene algo más difícil. Lo que no sabe es qué tipo de Hard Chunk vendrá. Esa es la sorpresa gestionada.

> Los cuatro pilares se aplican en orden de prioridad. Si un elemento rompe la legibilidad sin texto para ser más cooperativo, la legibilidad gana.

- **Core Gameplay Loop:**
  - Visión general: el grupo se mueve → encuentra un obstáculo (enemigo, hazard o puzle) → evalúa si se puede superar de forma individual o requiere coordinación → ejecuta (esquiva, usa ítem, coordina) → avanza al siguiente obstáculo o chunk
  - La gestión de estamina corre como capa de recurso permanente bajo este loop. No es un sistema separado: es la consecuencia acumulada de cada decisión de movimiento desde el spawn.
  - El loop escala en complejidad entre tiers: en Easy el loop es individual (cada uno esquiva por su cuenta), en Medium el loop tiene interferencia de enemigos activos que descoordinan al grupo, en Hard el loop solo se completa si el grupo coordina activamente.

> El loop nunca cambia de estructura. Lo que cambia es el tipo de obstáculo que lo activa y el grado de coordinación que requiere para resolverlo.

---

<div class="appendix-header">Apéndice — Chunk DH-01: Referencia de Diseño Hard Tier</div>

El Chunk DH-01 es el chunk de Hard Tier construido a mano que sirve como referencia de implementación para el sistema procedural. Contiene los tres sub-sistemas de Hard Tier en secuencia: hostigamiento continuo de gaviota (Zona 1), puzle de cajas con puerta (Zona 2) y puzle de separación espacial con varas rotatorias (Zona 3).

### Estructura del Chunk DH-01

| Zona | Nombre | Duración | Tipo de desafío | Elementos |
|---|---|---|---|---|
| 1 | El Emboscadero | 60–90 s | Evasión bajo hostigamiento aéreo continuo | Gaviota (continua), sombrillas, paredes, paso estrecho × 2 franjas ralentizantes |
| 2 | La Compuerta | 75–100 s | Puzle de activación simultánea + interferencia de enemigos | Cajas × 2, placas × 2, cangrejos × 2, Quads × 2, puerta |
| 3 | El Mecanismo | 45–75 s | Puzle de separación espacial + coordinación verbal | Botones × 3, varas rotatorias, zona de salto, botón final |

### Layout del Chunk DH-01

<figure class="sketch-figure">
  <img src="SKETCH_IMG" alt="Sketch inicial del Chunk DH-01 — pizarra de diseño">
  <figcaption>Sketch inicial del Chunk DH-01 — pizarra de diseño, Mayo 2026</figcaption>
</figure>

```
  [SPAWN x2]
       |
       v
  [ZONA 1 - El Emboscadero]  <-- gaviota continua + obstaculos fisicos
       |
       v
  [PASO ESTRECHO]  <-- x2 franjas ralentizantes a los lados
       |
       v
  [ZONA 2 - La Compuerta]  <-- cajas + placas + cangrejos
       |
  [PUERTA / QUADS]  <-- se abre solo con ambas placas activas
       |
       v
  [ZONA 3 - exterior]  <-- x3 botones controlan las varas
       |     [ZONA 3 - interior]  <-- varas giratorias + boton final
       +-- salto cuando el angulo es correcto -->|
                                                  v
                                            [META]
```

### Flujo esperado del jugador (DH-01)

El grupo sale del spawn junto. Las gaviotas empiezan a hostigar desde el primer segundo, sin tiempo de adaptación. El paso estrecho desincroniza al grupo: los más lentos quedan más expuestos. El grupo llega a la Zona 2 desincronizado, descubre las cajas y la puerta bloqueada, y se divide de forma implícita en portadores y escoltas. Cuando las dos placas se activan simultáneamente y los Quads se retiran, hay un momento de alivio colectivo breve. La Zona 3 confunde al principio: el primer intento de salto falla porque las varas están en posición incorrecta. Ese fallo funciona como tutorial emergente. La coordinación de botones + salto + botón final es el pico emocional del chunk.

### Notas de diseño del DH-01

- La gaviota de la Zona 1 usa el modo de hostigamiento continuo: ataca sin necesidad de que el jugador esté quieto
- La puerta de la Zona 2 requiere ambas placas activas al mismo tiempo; si alguna se desactiva antes de completar la apertura, el proceso se reinicia
- Los tres botones de la Zona 3 mueven las varas a posiciones angulares fijas; solo uno puede estar activo a la vez
- Los Quads se retiran lateralmente en una animación cuando la puerta se abre, para que el momento sea visualmente claro
- El indicador del ángulo correcto para el salto está marcado en el suelo de la zona interior

---

<p style="text-align:center; color:#5a6a80; font-size:8.5pt; border-top:1px solid #d8dce6; padding-top:16px; margin-top:48px;">
Tortunabo &middot; LDD v3.0 &middot; Mayo 2026 &middot; José Antonio Mota &middot; Rodrigo Fernández Carnicer
</p>

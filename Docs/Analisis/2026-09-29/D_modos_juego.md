# D. Modos de juego: Todos contra todos (P01), Carrera de buggies, ideas y política de contenido

Base leída: `Biblia_Tortunavy.md` §2, §5, §19, §22, §29; `Modo_Carrera.md`; `2026-09-29-Terreno-Plataformas.md`; `TN_RaceItems.h`. Los valores marcados **[1.ª pasada]** son hipótesis para playtest, no medidas. Los marcados **[calc]** salen de fórmulas con cifras de la Biblia.

## 0. Contradicciones y huecos a resolver antes de implementar

| # | Hallazgo | Propuesta |
|---|---|---|
| C1 | La Biblia dice que en la carrera de la playa "no se muere" (aturde). P01 dice que caer al río es morir (`kill_boxes_uu`). | La muerte es una regla **del modo**, no del mapa. En TcT (Todos contra Todos) caer al río es una *baja con reaparición*, no la "muerte real con cuerpo y rescate" del Clásico (§22.3). |
| C2 | Los puentes de P01 son malla de terreno (marching cubes) fusionada con las mesetas. No se pueden romper, cortar ni balancear sin más. | Cualquier interacción con puentes exige puentes como **actores separados** (ver §1.9). Sin eso, el MVP solo tiene "ráfagas de viento" (empuje) y barricadas en la entrada. |
| C3 | Una tortuga solo se puede coger si está en caparazón o derribada (§19.1), y una bola lanzada empuja por física pero **no derriba** (§19.6, sin confirmar en juego). | Todo el diseño de empujar se basa en esas dos reglas. Hay que verificar el empujón en PIE antes de fijar números. |
| C4 | El director pide que ningún modo principal sea 100 % procedural en assets, pero el Cooperativo (principal) usa `LVL_ProcMap`. | Ver §4: el Cooperativo pasa a "híbrido" con módulos de diseñador. **Decisión del dueño.** |
| C5 | No hay datos del buggy (plazas, velocidad, daño). | Se asume conductor + hasta 3 pasajeros, sin daño ni arma propia. Ver preguntas abiertas. |

## 1. Todos contra todos en el mapa de puentes P01

Nombre de trabajo: **«Batalla de los Puentes»** (`ETNProcGameMode::Bridges`). Estado: MVP. Jugadores: 2-8.

**Experiencia buscada.** Caos de fiesta: cruzas un puente de 70 m con el corazón en la boca, ves a alguien venir de frente y decides entre esconderte en el caparazón (no te tiran, pero te pueden coger) o embestir. Cada caída se ve, se ríe y se venga.

### 1.1 Objetivo, rondas y victoria

- Partida a **3 conchas**, igual que la carrera (reutiliza `ETNBeachRacePhase`: recuento, sprint final, campeón, podio de basura).
- Ronda de **120 s** [1.ª pasada]; cuenta atrás de 5 s con las tortugas saliendo de sus huevos como en la carrera.
- Gana la ronda quien tiene más puntos: se lleva **1 concha**. Con 4 o más jugadores, la segunda se lleva **media concha** (reutiliza `RaceShellHalves`).
- Empate en lo más alto: **muerte súbita** de 45 s solo entre empatadas (sin reaparición, cae una bola de 1 punto por baja); la última en pie gana. Es el equivalente del sprint final.
- Duración esperada de partida: 4-5 rondas, unos **10-12 min**. Si se quiere corta: 2 conchas.

### 1.2 Puntuación

| Evento | Puntos | Razón |
|---|---|---|
| Expulsar a una rival al río | +1 | El objetivo del modo. |
| Rival cae con tu último contacto hace menos de **6 s** | +1 (a quien la tocó, empujó, lanzó o le pegó un objeto) | Atribución generosa: la mayoría de caídas son por accidente tras un golpe. |
| Caída sin agresor | 0 y reaparición lenta (8 s en vez de 4) | Penaliza sin restar; restar frustra a quien va perdiendo. |
| Sobrevivir sin caer toda la ronda | +1 | Premia jugar a esconderse, pero poco. |
| Gaviota o cangrejo te mandan al río | +1 al **líder actual** | Anti "dejar que el entorno mate por mí"; evita que el líder se relaje. |

### 1.3 Reaparición

- Huevo de tu **meseta de origen**; si hay una rival a menos de 15 m, en el nido libre más alejado de rivales. Ocho nidos, uno por meseta, salvo la +30 del cuello (premio central).
- 4 s de espera con cámara de repetición de la caída (1,5 s) [1.ª pasada]; **2 s de inmunidad** (ya existe `ReviveImmunitySeconds`).
- Suelta lo que carga y pierde el objeto en mano (no el de la ranura de reserva).

### 1.4 Puentes

| Opción | Efecto | Estado | Coste |
|---|---|---|---|
| Ráfaga de viento | Cada 25-40 s, un puente al azar recibe 4 s de viento lateral con aviso (silbido y polvo). Empuje horizontal de 250 cm/s² [1.ª pasada], vence al caminar y no al ir en bola pesada. | **MVP** | S |
| Barricada de entrada | Puente cerrado con sacos terreros o puerta de conchas (assets existentes) según jugadores. | **MVP** | S |
| Balanceo | Cuerdas que se mecen al correr en grupo. | Cortar (mareo y red) | — |
| Corte por tiempo | A los 60 y 90 s caen dos puentes (los de fuera primero); las mesetas quedan aisladas y solo se cruza con catapulta. | Después | M (puentes-actores) |
| Rotura por peso | Se rompe con 3 o más encima. | Cortar | — |

Regla de oro: el puente **nunca** se rompe con jugadoras encima sin aviso de 3 s.

### 1.5 Ítems

Reutiliza `TNRaceItems::RollLoot` con `Norm` calculada por puntos (líder = 1, colista = 0), no por posición en la carrera.

| Ítem | En TcT | Nota |
|---|---|---|
| Coco turbo / triple | Sí | Embestida y huida. |
| Coco dorado | No | Rompe el modo. |
| Protector solar | Sí, peso alto para colistas | Invulnerable y derriba: 5 s en vez de la duración de carrera [1.ª pasada]. |
| Cangrejo teledirigido | Sí | Persigue a la rival **más cercana** en vez de la de delante. |
| Gaviota justiciera | Sí | Va a por la líder en puntos: es el catch-up del modo. |
| Mina de arena | Sí | Excelente en puentes estrechos. |
| Nube de tormenta | Sí, peso bajo | Marea a todas menos a ti; con caída es cruel. |
| Disco volador | Sí | Barre puentes. |
| Silbato | Sí, peso bajo | Aturde cangrejos y gaviotas alrededor. |
| Pelícano taxi | No en el MVP | Te deja "por delante"; en P01 podría teletransportar a la +30. Después. |

Cajas: una por meseta (8), reaparecen a los 20 s. Cofre único en la +30 del cuello: el mejor botín, y el sitio más difícil de mantener.

### 1.6 Peligros

- **Gaviotas** (2-3): patrullan sobre el río entre mesetas; cogen y suelen soltar sobre el río. Aquí eso es un asesinato, así que ~70 % de las presas caen sobre un puente o meseta (el punto de suelta se ajusta a "tablero o meseta más cercano"). Sombra creciente de aviso, la plancha esquiva.
- **Cangrejos** (1-2 por meseta grande): aturden en bola y te dejan cogible. Patrullas cortas.
- **Catapultas y trampolines**: atajos entre cotas (ver §1.8). Aterrizaje siempre sobre meseta.
- Sin quads ni minas fijas en mesetas de aparición (evita muerte injusta al reaparecer).

### 1.7 Cámara y HUD

- Cámara normal de tercera persona. Al borde de un puente el terreno se aleja: subir la altura base 15 % [1.ª pasada] para ver el vacío.
- HUD de carrera con caras y puntos, aviso de "quién te empujó" en la pantalla de caída, indicador del temporizador de ráfaga y **flechas de borde** hacia la rival más cercana solo tras 10 s sin ver a nadie (evita esconderse).
- Espectadoras: fantasma actual, con cámara aérea fija de todo el mapa.

### 1.8 Balance para 2-8 jugadores

| Jugadoras | Mesetas activas | Cierre | Cajas | Ronda |
|---|---|---|---|---|
| 2-3 | 5 (cuello y las 4 mesetas centrales) | 4 puentes barricados | 5 | 90 s |
| 4-5 | 7 | 2 barricados | 7 | 120 s |
| 6-8 | 9 | ninguno | 9 | 120 s |

Regla: encuentro esperado cada 15-20 s. Catch-up: pesos de ítems por puntos, gaviota al líder, reaparición lenta solo para el que cae solo, y el premio de segunda con media concha.

### 1.9 Cambios en el mapa P01

1. **Puentes como actores** (`ATN_BridgeSpan`): el generador debe dejar de fundir el tablero en el terreno y emitirlo como malla propia con colisión, para poder cerrar, cortar y animar. Coste M, y es el único cambio con riesgo. Sin él, solo el MVP de §1.4.
2. **Colocaciones**: ocho nidos de huevos (uno por meseta, menos la +30 del cuello), una caja por meseta a 8-10 m del borde, cofre en el arco del cuello (73 m de puente en túnel: zona de emboscada).
3. **Cobertura**: 15 % de cada meseta con parapetos de sacos, erizos antitanque y castillos pequeños (assets existentes). Sin cobertura, todo es persecución en llano.
4. **Rampas y ascensos**: las mesetas de +20/+25/+30 tienen paredes de 5 m que la tortuga no sube (salta 1,2 m). Añadir una rampa de tierra de 20° en 2 mesetas, 2 catapultas (+20 → +30) y 3 trampolines a mesetas contiguas. Las palas dan atajos cooperativos.
5. **Zonas de muerte**: se mantiene la caja de agua. Añadir un volumen de "fuera de mapa" de 50 m alrededor del marco (ya es río). Guardar quién tocó por última vez a cada tortuga (`LastInstigator`, 6 s).
6. **Barandillas**: los tableros no llevan ninguna. Es intencionado, pero los diseñadores pueden añadir cuerdas visuales sin colisión.
7. **Aviso de la grieta**: la +30 del borde sur toca la +25 con una rampa de 27° (Salvedades del doc de terreno). Decidir si es rampa (atajo) o muro. Propuesta: dejarla como rampa.

### 1.10 Variantes

| Variante | Regla | Coste |
|---|---|---|
| **2v2 / 4v4** | Parejas que rotan como en 2v2 (`AB|CD`, `AC|BD`...). Se puntúa por pareja. Pueden lanzarse mutuamente a otra meseta: **17,6 m** de alcance [calc: v = 15 m/s, 25°, g = 9,8]. | S |
| **Rey de la colina** | La +30 del cuello tiene un círculo de 6 m; +1 punto cada 5 s dentro con una sola tortuga. Sin bajas por puntos. | S |
| **Último en pie** | Una vida, sin reaparición; cada 30 s cae un puente; gana la que queda. Necesita puentes-actores. | M |
| **Puentes de fuego** (más tarde) | Las ráfagas se convierten en tormenta. | M |

## 2. Carrera de buggies en mapas geográficos (España, Europa, Mundo)

Estado: **Later** (depende del buggy). Nombre de trabajo: **«Gran Premio Tortunavy»**. Escala de España: 1 m de juego = 2,1 km reales; en Europa y Mundo la escala se ajusta para que el tiempo de tramo sea constante.

**Experiencia buscada.** Un rally de coche-equipo: el conductor lleva, la navegante elige el atajo, el artillero castiga al de delante y el mecánico apaga el humo. Se pierde por mala comunicación, no por mala puntería.

### 2.1 Formato

- **Punto a punto con checkpoints obligatorios en orden** ("etapas": ciudades o hitos), ruta libre entre ellos. Cada checkpoint es un arco (el de neumático de la meta) de 40 m de luz. Esto crea los atajos solo.
- Variante circuito para Europa: **3 vueltas** de un anillo cerrado (solo cuando el mapa tiene un lazo natural).
- 6-10 checkpoints; el tramo medio debe durar **60-75 s** [1.ª pasada]. Fórmula: distancia entre arcos = velocidad media del buggy × 65 s. Con la velocidad aún por medir, esta es la regla de trazado.
- Carrera de **8-10 min** (una sola ronda; el "Gran Premio" son 3 mapas seguidos con puntos por puesto: los de §30.2).
- Equipos: 1 buggy por equipo de 1-4. Con 8 jugadoras: 4 buggies de 2 o 2 de 4.

### 2.2 Reglas base

- Sin paso por el checkpoint anterior no cuenta el siguiente.
- Vuelco, mar o precipicio: reaparición en el último arco con 3 s de espera y 5 s de inmunidad (no muerte real).
- Fuera de ruta: aviso a los 200 m del último arco y reaparición automática a los 400 m [1.ª pasada].
- Quien lleva 30 s parado en un mismo sitio, reaparece.

### 2.3 Papel de las pasajeras

| Puesto | Función | Reutiliza |
|---|---|---|
| Conductora | Conduce y gasta el turbo. Su ranura es solo de movimiento (coco, protector). | Cocos, protector |
| Artillera | Lanza objetos y tinta hacia atrás o los lados. Ángulo fijo de 25° + cámara (§19.3). | Lanzar, `TN_InkProjectile`, minas, disco |
| Mecánica | Mantener E sobre el buggy durante 3 s repara neumático o motor dañado (humo, -35 % de velocidad [1.ª pasada]). Un impacto de cangrejo teledirigido daña. | Interactuar |
| Navegante | Único puesto con el mapa grande y las flechas de atajos (los conductores ven solo el siguiente arco). | HUD |
| Lastre | Cualquier pasajera puede inclinarse a un lado para no volcar en rampas y curvas. | Movimiento |
| Proyectil | En caparazón puede ser lanzada de un buggy a otro: sale como caja con 17,6 m de alcance y aturde. Vuelve andando o esperando a su equipo. | Coger y lanzar |

Cada puesto se reasigna con un botón; hay huecos vacíos (1 jugadora = todos los puestos con ayudas automáticas: reparación automática lenta, sin artillería).

### 2.4 Ítems, atajos y catch-up

- **Ítems**: las cajas en los arcos y a mitad de tramo. Sirven todos menos silbato y disco corto. Mapeo: cangrejo teledirigido y gaviota justiciera persiguen al buggy de delante; la mina se suelta detrás; la nube marea al conductor; el coco turbo es el turbo del buggy; el pelícano taxi puede coger el buggy entero (más tarde, L).
- **Atajos**: puertas de conchas (cuestan las conchas de puntos del equipo), rampas colocadas por diseñadores sobre cordilleras o ríos, y el peligro natural de cada ruta corta (agua, acantilado, cangrejos). El mapa se prepara con **dos rutas por tramo**: una segura y otra corta y arriesgada.
- **Catch-up**: pesos por puesto de `RollLoot`; turbo de "rebufo" tras el buggy de delante (+15 % de velocidad a menos de 30 m, 4 s); y **tormenta que avanza** (la ya existente) como presión trasera: quien va último ya no queda eliminado, solo puntúa menos.
- **Duración total**: 8-10 min por carrera.

## 3. Lluvia de ideas: 13 modos, sobre todo código

Coste en código: S (menos de 3 días), M (1-2 semanas), L (más). Ordenados por diversión/coste; ★ = top 4.

| # | Modo | Concepto | Mecánicas reutilizadas | Mapa | Coste | Por qué funciona online |
|---|---|---|---|---|---|---|
| 1 ★ | **Caza de conchas** | 3 min: gana quien tenga más conchas. Un golpe o una caída suelta la mitad que llevas. | Conchas, rebuscables, cofres, empujones, caparazón | Playa procedural (100 % procedural, secundario) | S | Robo constante, lectura inmediata, cualquier grupo. |
| 2 ★ | **Sumo de bolas** | Todas en caparazón sobre una plataforma que se encoge; se echa a las demás. | Bola física, mina, plataformas que se tambalean | Arena plana o meseta de P01 | S | Fácil de entender, partidas de 60 s, revancha instantánea. |
| 3 ★ | **Escalada en equipo** | Coop: todas deben subir a la +30 de P01; los muros de 5 m se superan lanzándose entre sí. | Coger/lanzar, catapulta, pala, huevos | P01 (retoque) o dunas | S-M | Comunicación pura ("¡lánzame!"), cero tiempo de espera. |
| 4 ★ | **Patata caliente** | Una tortuga lleva una mina con mecha; se la lanza a otra; explota y elimina. | Mina, coger/lanzar, aturdimiento | Cualquier arena | S | Risa garantizada; se explica en 10 s. |
| 5 | **Pilla-pilla de tinta** | La "manchada" mancha a las demás con tinta; la última sin manchar gana. | `TN_InkProjectile`, aturdimiento | Playa o P01 | S | Reglas conocidas por todos. |
| 6 | **Rey de la colina móvil** | Un círculo salta de sitio cada 30 s. | Zonas, plataformas | Cualquier mapa | S | Empuja al movimiento y a las peleas. |
| 7 | **Cocobol** | 2v2 o 4v4: mete el coco gigante en la portería; se coge, se lanza, se rueda. | Coco, coger/lanzar, bola | Playa llana | M | Deporte fácil de seguir, muchos goles. |
| 8 | **Robar huevos** | Cada equipo defiende sus 3 huevos y roba los de los demás (llevar huevo = velocidad de carga 330). | Huevos, nidos, carga | Fortaleza de arena o P01 | M | Ataque/defensa, roles, rescates. |
| 9 | **Oleadas** | Coop: defender los huevos de olas de cangrejos, gaviotas y quads. | Enemigos, castillo, huevos | Fortaleza de arena (con retoque) | M | Cooperativo, dificultad escalable a 1-8. |
| 10 | **Zona de tormenta** | La tormenta cierra el círculo; sobrevive la última, con objetos en el suelo. | Tormenta, objetos, conchas | Playa o P01 | M | Tensión creciente, poca espera si hay fantasma. |
| 11 | **Lanzamiento largo** | Por parejas: una lanza a la otra en bola a la diana en el mar; puntos por anillo. | Lanzar, bola, agua | Playa o meseta | S | Fiesta con espectadores, todos opinan. |
| 12 | **Tesoro escondido** | Coop: rebuscar 5 cofres antes de que la tormenta llegue; el tesoro se transporta con carga. | Rebuscables, cofres, carga, tormenta | Playa procedural | M | Objetivo compartido, ritmo de exploración. |
| 13 | **El Sargento** | Asimétrico: una jugadora usa el tanque de juguete contra el resto, que debe llegar a la meta. | Tanque, silbato, objetos | Playa | L | Papel distinto y memorable; necesita control de tanque. |

Por qué el top-4: los cuatro son S/S/S-M/S, cubren versus, coop y fiesta, y ninguno requiere arte. **Siguiente paso** si sobra semana: Cocobol (M, mucho margen de diversión).

## 4. Política de contenido: qué se prepara y qué puede ser 100 % procedural

Definición: "100 % procedural" = terreno, puntos de juego y decorado salen de una semilla sin intervención humana. "Preparado" = un diseñador dibuja un boceto, el generador produce el mapa y el diseñador lo retoca a mano (el importador conserva lo diseñado y exige `TN_REGENERATE=1` para pisar, según la regla ya vigente).

| Modo | Rango | Mapa | Categoría |
|---|---|---|---|
| Batalla de los Puentes (P01) | Principal | Boceto → P01 → retoque de colocaciones, cobertura y rampas | **Preparado** |
| Carrera de buggies | Principal | Mapas geográficos (España, Europa, Mundo) + arcos y atajos de diseño | **Preparado** (terreno generado, rutas de diseñador) |
| Carrera en la playa | Principal | Terreno fijo + reparto por semilla | **Híbrido** (ya cumple) |
| Cooperativo | Principal | Hoy `LVL_ProcMap` procedural | **Híbrido propuesto**: mantener el camino procedural, pero insertar "módulos de diseñador" (salidas, puestos de nido, sets colosales) y firmar un tramo por bioma. **Decisión del dueño.** |
| Escalada en equipo | Secundario | P01 o boceto propio | Preparado |
| Robar huevos, Oleadas | Secundario | Fortaleza de arena de diseño, retoque | Preparado (reparto procedural permitido) |
| Cocobol, Sumo de bolas, Patata | Secundario | Arena plana | Procedural permitido |
| Caza de conchas, Pilla-pilla, Zona de tormenta, Tesoro escondido | Secundario | Playa procedural | **100 % procedural permitido** |
| Rey de la colina móvil, Lanzamiento largo | Secundario | Cualquiera | Procedural permitido |

Reglas del pipeline:

1. Todo mapa principal parte de un boceto (JPEG o PNG), no de una semilla.
2. Las piezas críticas (nidos, arcos, cofres, puertas, puentes) las coloca una persona; el generador solo rellena decorado y enemigos ambiente.
3. Un mapa preparado nunca se regenera encima del retoque sin la variable de confirmación.
4. Cada modo declara en su `GameMode` si acepta mapas procedurales (`bAllowsProceduralMap`); los principales lo tienen a `false`.

## 5. Preguntas abiertas

1. ¿Cuántas plazas tiene el buggy y qué velocidad tiene? De eso dependen §2.1-2.3.
2. ¿Caer al río en TcT es siempre baja con reaparición (recomendado) o quieres muerte real con rescate?
3. ¿Se paga el coste M de puentes como actores separados (§1.9)? Sin él no hay corte de puentes ni "último en pie".
4. ¿Cooperativo como modo híbrido con módulos de diseñador (C4)?
5. ¿El empujón de la bola derriba? (§19.6, sin confirmar): condiciona todos los modos de empuje.

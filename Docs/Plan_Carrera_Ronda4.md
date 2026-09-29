# Plan de la ronda 4 del modo carrera (y ajustes comunes)

Apuntado el 29-09-2026 con las notas del usuario tras probar las salas, los objetos y la red de 8 (commits 186a7547 a
6d46da0a). **Sin empezar**: se ejecuta cuando el usuario diga que se empiece. Rama `claude/modo-carrera`; lo marcado como
*común* se lleva después al cooperativo, como las salas, el menú de pausa y el fantasma.

Todos los textos nuevos, en el idioma del juego (español de España por defecto) y con su versión inglesa adaptada, no
literal (como los nombres de sala).

## Fallos

1. **Segunda gaviota + caparazón = torbellino.** La primera vez que te coge una gaviota todo va bien; si te coge otra y
   te metes en el caparazón, la física se rompe y la tortuga queda atrapada en un bucle de bola y suelo (como el
   torbellino de la tormenta ya arreglado). Sospecha: algo del primer agarre no se restaura del todo (la reserva de
   `TNBeach::ClaimTurtle` en `Held`, el ignorar colisiones con el enemigo, el modo de movimiento o la base) y el segundo
   agarre suelta a la tortuga en bola con ese estado sucio. Reproducir con dos gaviotas seguidas (`TN.Beach.Enemy…`),
   mirar el registro del árbitro y del `GuardUnderSand`, y cubrir el caso con una prueba si se puede.
2. **Nombre de la sala en inglés con el juego en español.** `TNRoomNames::IsSpanish()` mira la cultura del motor, que en
   el editor (y en un juego sin datos de localización «es») es «en» aunque Windows esté en español. Arreglo: que el
   idioma salga del ajuste de idioma del juego (español por defecto), no de la cultura del motor; revisar también los
   avisos de rechazo de las salas, que se traducen igual.
3. **Ruedas del quad**: la cara interior se ve rota desde fuera (normales o sentido de los triángulos del lado de dentro,
   o material de una cara). Revisar la malla creada en ejecución del quad (enemigo `ToyTank`/quad de la carrera).
4. **Catapultas**: al vibrar se mueven de verdad (física) y expulsan a la tortuga metida en su caparazón antes del
   disparo. La vibración pasa a ser solo visual (mover la malla visible, no la colisión ni el punto de carga).

## Mejoras de juego

5. **Nerf de la gaviota y de su caca.** Ahora es casi imposible esquivarlas. Debe poder esquivarse corriendo, cambiando
   de dirección y tirándose en plancha en el momento justo:
   - Menos radio de actuación (detección y picado) y menos seguimiento en el último tramo del picado.
   - La caca: menos radio de impacto, caída que se pueda leer (la sombra y el «!» ya lo anuncian) y que la plancha en el
     momento justo libre.
   - Ajustar también la gaviota justiciera del objeto de carrera (que ya se libra corriendo a 800 cm/s).
6. **Entre ronda y ronda** la espera queda pobre. Transición del huevo (la del huevo negro del cooperativo o parecida),
   salir directamente saltando de los huevos como al empezar y un «RONDA 2» (o la que toque) en grande, con su
   animación. Libertad de diseño.
7. **Llegada al agua (lo más importante).** Ahora, al terminar de zambullirse, la tortuga se pone de pie antes de volverse
   fantasma o de acabar. Nuevo flujo:
   - Según se zambulle, el **huevo negro** (el de revivir del cooperativo) se cierra desde arriba y desde abajo de la
     pantalla, rápido.
   - En la pantalla negra: **«Has quedado X.º»** con su premio y un mensaje gracioso según el puesto; después, a
     espectador fantasma (o al final si ya no queda nadie).
   - Premios: 1.º corona enorme; 2.º corona más pequeña; 3.º corona enana; del 4.º al 8.º, cada vez más humillante.
     Mensajes alentadores arriba del podio y más burlones abajo. Propuesta inicial (se afina al empezar):
     - 1.º corona de oro enorme: «¡Reina de la playa! Hasta las gaviotas te hacen reverencias.»
     - 2.º corona de plata: «¡Casi! La arena aún quema de tus pasos.»
     - 3.º corona de bronce enana: «Podio. La corona es pequeña; el orgullo, no.»
     - 4.º un cubo de playa por corona: «Cuarta. El cubo también es un trono… de arena.»
     - 5.º media concha rota: «Quinta. Ni frío ni calor: templadita.»
     - 6.º un flotador pinchado: «Sexta. Hasta el flotador se rindió antes que tú.»
     - 7.º un calcetín mojado lleno de arena: «Séptima. El gusano ya había reservado mesa.»
     - 8.º un alga en la cabeza: «Octava. Las gaviotas ya te llaman por tu nombre.»
   - Sin ponerse de pie: la cámara y la tortuga se quedan en el agua hasta que el huevo tapa la pantalla.

## Sonido y efectos

8. **Música de fondo de la carrera** con identidad propia, de fondo: por debajo de efectos, voces y avisos (categoría
   Música del menú de pausa, volumen bajo y que se aparte un poco en momentos fuertes: cuenta atrás, sprint, podio).
   Opciones: sintetizarla como la del tráiler (`Tools/Trailer/musica.py`, en bucle sin cortes) e importarla, o
   componerla por capas en el motor (base tranquila + capa de tensión en el último minuto).
9. **Golpes del caparazón**: con la tortuga en bola, sonido y mini efecto de golpe según contra qué choca (arena, roca,
   madera, agua, otra tortuga, enemigo, objetos de los bañistas), con fuerza según la velocidad y un mínimo entre golpes
   para no hacer ruido continuo (lección del «pum pum pum» de los bañistas).

## Ajustes y controles

10. **Ojo de pez leve** en la cámara, nueva opción del menú de pausa (pestaña Juego), **activada por defecto**: da la
    sensación de que todo es aún más inmenso. Opción barata: la proyección Panini del motor (`r.Upscale.Panini.D` y
    `r.Upscale.Panini.S`) con un valor suave; si no basta, material de posproceso con distorsión de barril. Comprobar que
    no deforma el HUD ni marea con el FOV al correr. *Común*.
11. **Mando: Círculo / B para meterse en el caparazón.** Revisar choques con lo que haga ahora B en juego (en los menús es
    «volver») y actualizar la lista de controles del menú de pausa y la copia de `IMC_Player`. *Común*.

## Documento maestro del juego

12. Un documento con **toda** la información del juego, para tenerlo como criterio y capacidades completas: modos
    (cooperativo, carrera, 2vs2, tutorial, lobby), mapas y cómo se generan, flujo de partida y de sala, la tortuga y todas
    sus mecánicas (movimiento, estamina, caparazón, plancha, cargar compañeros, derribo, inventario, emotes, voz),
    decorado pieza a pieza, trampas, enemigos uno a uno con su comportamiento y cómo se evitan, todos los objetos (los de
    siempre y los de carrera) con sus pesos, puntuación, cosméticos y tienda, interfaz y menús, sonido, red y límites (8
    jugadores), accesibilidad (cada opción y por qué está) y comandos de prueba. Sale del código y de los documentos de
    `Docs/`, no de memoria; con referencias a los archivos. Nombre propuesto: `Docs/Biblia_Tortunavy.md`.

## Pendiente de antes

- Voz con Opus (ahora 16 kHz y 4 oyentes como arreglo provisional) y medir con `stat net` con 4 y 8.
- Llevar al cooperativo lo común: salas, menú de pausa, fantasma, pausa del huevo, brillo de objetos.
- Con Steam, una sala llena no sale en la lista ni por código (Steam oculta los lobbies llenos).
- Planos opcionales del tráiler (salto al mar, grupo en cooperativo, catapulta, gaviota, podio).

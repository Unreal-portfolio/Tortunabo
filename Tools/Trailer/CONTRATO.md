# Tráiler de Tortunavy: contrato entre captura, música y montaje

Tráiler de feria «frenético, adictivo y dopamínico» con lo que ofrece el juego. Dos versiones de idioma (rótulos en
español y en inglés) y dos formatos:

- **Principal:** 60 s, 1920×1080, 30 fps, H.264 + AAC (pantalla del stand).
- **Vertical:** 30 s, 1080×1920, 30 fps, H.264 + AAC (redes y móviles).

Salidas en `Saved/Trailer/out/`: `Tortunavy_Trailer_60s_ES.mp4`, `…_60s_EN.mp4`, `…_30s_vertical_ES.mp4`,
`…_30s_vertical_EN.mp4`. Herramientas en `Tools/Trailer/` (se versionan); material pesado en `Saved/Trailer/` (no se
versiona). Codificador: `imageio_ffmpeg.get_ffmpeg_exe()` (ffmpeg 7.1 con libx264, ya instalado). Python 3.14 con
numpy, OpenCV y Pillow. Fuentes de Windows disponibles: `impact.ttf`, `ariblk.ttf`, `bahnschrift.ttf`, `seguibl.ttf`,
`BAUHS93.TTF`, `comicbd.ttf`.

## El juego (lo que hay que vender)

Tortunavy: eres una cría de tortuga de 5 cm en un mundo gigante (todo a escala 28: un coco mide 4 m). Low-poly cartoon,
colores vivos, humor. Hasta 4 jugadores en línea, chat de voz por proximidad.

- **Lobby:** castillo de arena, tienda de disfraces y vestidor, general de Tortunavy (tono militar cómico), ponerse
  listos saltando a los huevos.
- **Cooperativo (aventura):** mapa procedural por biomas (selva, volcán con cámara de magma, cuevas, cascadas y pozas,
  puentes colosales, murallas con torres), tormenta que persigue, revivir a los compañeros, conchas de puntos, rebuscar en
  el decorado, objetos (bola, tinta de pulpo, concha trampa…), llevar y lanzar a un compañero, panzazo.
- **Carrera en la playa:** hasta la meta saltando de cabeza desde un acantilado al mar; ~5000 objetos por ronda; salida
  de huevos que te lanzan; fortalezas de arena gigantes con premio en la cima; catapultas de un solo uso y trampolines;
  cangrejos gigantes, erizos, lagartos que muerden, gaviotas y pelícanos que te cogen, quads que pasan, pulpos, pulgas,
  tanques de juguete, minas; tormenta de bañistas; gusano de arena gigante que se come a las rezagadas; al mejor de 3
  conchas, medias conchas en la cuenta de 10 s, sprint final de desempate y podio del campeón.

## Estructura del principal (60 s, 150 BPM: 0,4 s por pulso, 1,6 s por compás)

| Tramo | Tiempo (s) | Música | Imagen |
|---|---|---|---|
| S1 Gancho | 0,0–6,4 | Escaso y misterioso, golpes en 1,6 / 3,2 / 4,8 y subida | Rótulos del gancho sobre planos «macro» de la tortuga entre objetos gigantes |
| S2 Cooperativo | 6,4–19,2 | Ritmo tropical-electrónico juguetón (marimba, steel drum y sintes), energía media-alta | Cortes cada 2 pulsos: biomas, puentes, cuevas, grupo de tortugas, tormenta |
| S3 Corte | 19,2–22,4 | Parón con rayado de disco en 19,2, silencio y tensión, subida | «¿O PREFIERES…?» y cuenta 3-2-1 |
| S4 Carrera | 22,4–41,6 | Caída a tope: bombo en cada pulso, hi-hats dobles, golpe en cada compás | Cortes en cada pulso: huevos, catapultas, trampolines, cangrejo, gaviota, quads, minas, tormenta, gusano, zambullida |
| S5 Características | 41,6–51,2 | Patrón entrecortado con cortes secos; un golpe por rótulo | Rótulos a golpe de pulso o medio pulso con planos de apoyo |
| S6 Final | 51,2–60,0 | Último estribillo 51,2–56,0; golpe del logo en 56,0 con cola hasta 60,0 | Podio, logo TORTUNAVY y «PRÓXIMAMENTE» / «COMING SOON» |

## Estructura del vertical (30 s, 150 BPM)

| Tramo | Tiempo (s) | Música |
|---|---|---|
| V1 Gancho | 0,0–3,2 | Golpe en 0,0 y en 1,6 y subida |
| V2 Carrera | 3,2–19,2 | La caída de S4 |
| V3 Características | 19,2–25,6 | Patrón de S5 |
| V4 Logo | 25,6–30,0 | Golpe del logo en 25,6 y cola |

## Ficheros

- **Música** (`Saved/Trailer/music/`): `trailer_60.wav` y `trailer_30.wav` (48 kHz, estéreo, 16 bits, sin clip, en torno
  a −14 LUFS integrados y pico −1 dBTP), más `beatmap_60.json` y `beatmap_30.json`:
  `{"bpm":150,"duration":60.0,"beats":[t…],"downbeats":[t…],"sections":[{"name":"S1","start":0.0,"end":6.4}…],
  "hits":[{"t":1.6,"kind":"impact"}…]}` (todo en segundos).
- **Efectos** (`Saved/Trailer/sfx/`): WAV 48 kHz sueltos que el montaje mezcla sobre la música:
  - golpes: `whoosh_short`, `whoosh_long`, `impact_big`, `impact_small`, `riser_2s`, `record_scratch`;
  - juego: `boing`, `splash`, `explosion_toy`, `gull_screech`, `crab_clack`, `worm_chomp`, `egg_crack`, `coin_pop`,
    `crowd_cheer_short`;
  - interfaz: `ui_tick`, `logo_sting`.
- **Material** (`Saved/Trailer/footage/<plano>/`): `frame_00000.jpg`… a 30 fps, 1920×1080 (calidad 95), más
  `shot.json` con `{"name","frames","fps":30,"desc","tags":[…]}`. La lista de planos está en `Saved/Trailer/footage/index.json`
  (la escribe la captura) y crece a medida que se captura.
- **Montaje** (`Tools/Trailer/`): `montaje.py` lee `edl_60.json` y `edl_30.json` (listas de cortes, efectos y rótulos
  por idioma) y genera los cuatro MP4. Si falta un plano, usa un sustituto (otro plano con etiquetas parecidas o una
  tarjeta animada) y lo avisa, para poder montar antes de tener todo el material.

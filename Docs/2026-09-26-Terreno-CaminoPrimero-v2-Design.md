# Terreno «camino primero» v2 — cruces, puentes, barranco y paredes naturales

Fecha: 2026-09-26. Estado: aprobado en conversación por Rodrigo; pendiente de revisión del texto.
Parte de `Docs/Diseno_Terreno_CaminoPrimero.md` y del mapa C01 (`Scripts/terrain_path/`). Todo lo no mencionado aquí se conserva.

## 1. Motivo

Feedback de Rodrigo sobre C01:

- el agua gusta y se queda;
- los puentes generados casi nunca aparecen en el mapa;
- las paredes suben y se vuelven irregulares, pero todas acaban a la misma distancia del camino; parecen mesetas;
- el suelo y las paredes tienen la misma textura.

## 2. Qué no cambia

- El camino principal, los lazos, las bifurcaciones y las islas interiores que rodean los lazos.
- Las dunas normales fuera del trayecto (fondo de vistas), los lagos y el mar.
- El río del camino principal y el agua cartoon.
- El pie de la pared: 3–3,5 m casi verticales que cierran el paso.
- No se sale a las vistas.

## 3. Cruces y puentes

- De 1 a 4 cruces por mapa (`PathStyle.crossings` pasa a ser un rango).
- Cada cruce elige tipo al azar con una proporción del estilo (`bridge_share`):
  - **túnel bajo cerro**: el camino de arriba pasa sobre un montículo; el de abajo lo atraviesa con ≥ 8 m de roca sobre su suelo;
  - **puente fino**: el de arriba se estrecha a un tablero de 3–4 m de ancho sobre la luz del de abajo más 3 m por lado; el de abajo pasa al aire libre con ≥ 5 m de hueco libre.
- El tablero es una **pieza propia en la densidad 3D**: losa de 1,5–2 m con cara inferior en arco. Se suma después de excavar, así que la excavación ya no se lo come.
- El tablero se **funde con el terreno**: unión suave (soft max) con las paredes, que hacen de estribos; no debe leerse como una pieza pegada.
- Los arcos sueltos actuales sobre el camino pasan a ser esta misma pieza, sin camino encima.

## 4. Barranco

- 0 o 1 por mapa, según el estilo (`canyon`: `none`, `deadly`, `walkable`).
- Atraviesa el mapa de este a oeste serpenteando como un cauce natural: 20–35 m de ancho y 8–14 m por debajo del camino que lo cruza, con paredes de roca y el remate natural de la sección 5.
- El camino principal lo cruza por un puente fino (sección 3) con más luz.
- Fondo: río ancho con orillas de arena y el agua cartoon.
- **`deadly`**: ningún camino baja. El manifest trae `kill_boxes_uu` (cajas que cubren el fondo) y `ATN_MapVariantLoader` pone un `ATN_DeathZoneVolume` en cada una.
- **`walkable`**: un lazo baja al fondo por rampas, lo recorre por la orilla y vuelve a subir.

## 5. Paredes naturales

- Sobre el pie vertical, la parte alta es natural: sin norma fija (pico, corte, rugosidad, voladizo hacia dentro) y **nunca meseta**.
- La altura de la cresta y la distancia a la que acaba la pared varían a lo largo del camino. Donde la pared es baja, se ven las dunas de alrededor.
- Implementación: la banda de bloqueo (`block_band_m`) y la bajada a las vistas pasan a depender de un ruido de escala grande; la cima usa ruido multiescala (no un lomo senoidal de perfil fijo).
- El estilo fija el carácter por mapa: rango de altura extra, rango de profundidad y rugosidad.

## 6. Camino

- Un poco más ancho: moda del semiancho de 5,5 a 6,5 m (`width_m` = (4.0, 6.5, 10.5)).

## 7. Texturas

- Suelo: arena clara de grano fino.
- Pared: arena más oscura y compacta con vetas horizontales.
- Mezcla completa según la inclinación (a partir de ~35°), no el 60 % actual. El material usa un grano distinto en pared y suelo.

## 8. Catálogo de 30 mapas

- El agua cartoon vive en `LVL_MapVariants`: los 30 la tienen.
- Por mapa cambian la forma del camino, los cruces (1–4) y su tipo, las bifurcaciones y lazos, el barranco, el carácter de las paredes, el ancho y el río.
- Se generan después de que Rodrigo valide C01.

## 9. Pruebas

Automáticas, por mapa (tests de `Scripts/tests/test_terrain_path.py` y `check()` de `gen_terrain_path.py`):

- hueco libre bajo cada puente en toda la sección del camino de abajo;
- tablero andable y continuo;
- cada túnel tiene techo;
- recorrido de inicio a fin con todos los lazos, sin pisar las vistas;
- la altura de la cresta varía a lo largo del camino (sin mesetas);
- barranco `walkable`: el fondo se recorre; `deadly`: las cajas cubren todo el fondo.

Visual: Rodrigo valida C01 en `LVL_MapVariants`.

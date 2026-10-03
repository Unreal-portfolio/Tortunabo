> **Obsoleto** (2026-10-02): consolidado en `Docs/superpowers/specs/modos/03-Rally.md` (referencia del Rally).

# Rally Tortuga: sistemas de carrera, efectos e interfaz

Fecha: 2026-09-29 · Rama: `macro-update` · Complementa `Docs/Rally_E01B_y_Biplaza.md`, `Docs/Modos-UI-FX-2026-09-29.md` y el plan maestro §3.3.

Principio: todo lo que decide la carrera (posición, vuelta, dirección, reaparición, penalizaciones) lo calcula el **servidor**; los clientes solo lo muestran. Los efectos se disparan en cada máquina a partir de estado replicado, sin RPC por frame.

## 1. Sistemas de carrera (backend)

| Sistema | Regla | Autoridad y red | Pieza |
|---|---|---|---|
| Trazado | Spline del circuito generado con el mapa (`checkpoints_uu` + eje de la calzada) | Dato del mapa, igual en todas las máquinas | `ATN_RallyTrack` |
| Checkpoints y vueltas | Puertas en orden; saltarse una no cuenta; vuelta = pasar la meta con todas las puertas | Servidor; `CurrentLap` y `NextCheckpoint` replicados en el PlayerState | `UTN_RallyProgressComponent` |
| Posición en carrera | Orden por (vuelta, puerta, distancia al eje de la spline) | Servidor, 5 Hz; array ordenado replicado en el GameState | `ATN_RallyGameState` |
| Dirección contraria | Producto escalar entre la velocidad y la tangente de la spline < −0,5 durante 1,5 s a más de 20 km/h | Servidor decide; cliente muestra aviso | `TNRally::IsWrongWay` (puro, testeado) |
| Enderezar el coche | Volcado (vector arriba < 0,3) o parado > 2 s: botón de enderezar; automático a los 4 s | Servidor aplica par y elevación de 1 m con barrido; `ForceNetUpdate` | `UTN_BuggyFlipComponent` |
| Reaparición | Fuera de pista, en agua, bajo el terreno o tras 8 s atascado: vuelve a la última puerta, orientado a la spline, 2 s de fantasma sin colisión con buggies | Servidor | `UTN_BuggyRespawnComponent` |
| Atajos ilegales | Si entre dos puertas recorre menos del 60 % de la distancia de la spline, la puerta no cuenta | Servidor | Parte del progreso |
| Salida | Parrilla por posición de la carrera anterior (copa) o aleatoria; semáforo de 3 s; salida anticipada = 1 s de penalización | Servidor fija la hora de salida replicada | `ATN_RallyGameMode` |
| Fin | El primero cierra la carrera; los demás tienen 20 s; resultados y puntos de copa (10-8-6-5-4-3-2-1) | Servidor | GameMode |
| Rebufo y ayuda al último | Rebufo +15 % a < 30 m detrás; ítems ponderados por posición (`RollLoot`) | Servidor | Existente, adaptado |
| Anti-vuelco | Centro de masas bajo, par estabilizador en el aire limitado; la artillera con «Contrapeso» lo refuerza | Física del servidor | `ATN_Buggy` |
| Suelo | Fricción por material: calzada, arena, hierba, barro, agua somera (frena y salpica) | Físico, igual en todas las máquinas | Tabla de materiales |

## 2. Efectos (VFX y SFX), reutilizando lo existente

| Efecto | Disparo | Recurso |
|---|---|---|
| Derrape | Deslizamiento lateral de rueda > umbral | Nube de arena de la carrera recoloreada por material + marcas de rueda con decal en pool (máx. 64) |
| Boost (coco) | `bBoosting` replicado | Estela y chispas de la carrera; FOV +8° en la cámara; sonido del *Synth* con tono subido |
| Motor | Rpm y carga | `UTN_BuggyEngineSynth` (procedural, C++) |
| Salto y aterrizaje | Ruedas en el aire > 0,3 s | Polvo al aterrizar, sacudida de cámara leve |
| Agua | Rueda en agua | Salpicadura existente |
| Choque | Impulso > umbral | Destello y golpe del *ShellImpactSynth* |
| Dirección contraria | Estado del servidor | Aviso en pantalla + pitido |
| Enderezar | Evento replicado | Nube de arena y «¡hop!» |
| Túnel | Entrar en volumen de túnel | Reverberación en el bus de audio |

Sin Niagara nuevo salvo necesidad; todo con los sistemas de la carrera recoloreados por parámetro.

## 3. Interfaz

- HUD: posición (1.º/8), vuelta (2/3), velocímetro, tiempo de vuelta y mejor vuelta, ítem actual de la artillera y su cuenta atrás, aviso de dirección contraria, aviso «pulsa para enderezar».
- Minimapa con la spline del circuito y puntos de cada buggy (colores del equipo o de la skin).
- Semáforo de salida, pantalla de vuelta rápida, pantalla de llegada y tabla de copa.
- Lobby: selector de país/circuito y de copa; plazas conductora/artillera por buggy.

## 4. Assets

Borradores IA en `Art/Library/IA/` (en curso): caja de ítems del Rally, caracola de la artillera, rampa, pórtico de salida y meta, checkpoint. Imprescindibles del equipo de arte: `M_BuggyPaint` y la carrocería biplaza final (hay borrador en `Art/Source/Vehicles/Buggy/`).

## 5. Pruebas

- Puras: dirección contraria, orden de posiciones, atajo ilegal, reaparición (punto y orientación), puntos de copa.
- Piloto IA headless: 10 vueltas por mapa sin atascos ni vuelcos sin enderezar.
- PIE 4P con 150 ms y 2 % de pérdida: posiciones iguales en todas las máquinas, enderezar sin saltos de más de 50 cm.

## 6. Coste estimado

Backend 45–60 h · efectos 15–20 h · interfaz 25–35 h · pruebas 10 h. Entra en la fase F6 del plan maestro.

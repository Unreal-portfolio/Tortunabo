# Rally Tortuga: MVP jugable (2026-10-01)

Primera versión completa del Rally (objeto #40): buggy biplaza, carrera con puertas, puestos, meta, reaparición, torreta de la artillera con munición variada, piloto IA e interfaz. Va **separado** del lobby y del menú: se juega abriendo `LVL_Rally` directamente. La integración en el lobby será otra issue.

Fuentes: plan maestro §3.3 y §7.2, `Docs/Rally_Sistemas.md`, `Docs/Rally_E01B_y_Biplaza.md`, `Docs/superpowers/specs/modos/03-Rally.md`. Donde este documento difiere, manda la **Decisión** registrada en #40 (torreta en vez de los 4 ítems de caja).

## Cómo se juega

- `open LVL_Rally?Variant=I03R_tortuga_magna` (también `E01B_espana_rally`, `C01_camino` y las demás variantes con `checkpoints_uu`). Opciones: `?Seats=1` (un buggy por jugador), `?Bots=N` (buggies con piloto IA), `?Laps=N`.
- Biplaza (por defecto): los jugadores se emparejan por orden de llegada; la 1.ª de cada pareja conduce y la 2.ª es la artillera. Si una tortuga va sola, conduce y dispara ella con apuntado automático.
- Semáforo de 3 s; salir antes corta el motor 1 s. Cuando llega el primer buggy quedan 20 s; después, resultados y, a los 15 s, carrera nueva en el mismo mapa.

## Controles (Enhanced Input creado en C++, sin assets)

| Acción | Conductora | Artillera |
|---|---|---|
| Acelerar / frenar / girar | W / S / A-D · RT / LT / stick izq. | — |
| Freno de mano | Shift izq. · X | — |
| Turbo (mientras haya carga; se recarga derrapando con freno de mano y en el aire) | Espacio · A | — |
| Enderezar (pulsar) / reaparecer (mantener 1,5 s) | R · Y | R · Y |
| Apuntar | automático si va sola | ratón · stick der. |
| Coco (básica) | clic izq. · RB (sola) | clic izq. · RT |
| Munición especial | clic der. · LB (sola) | clic der. · LT |
| Disparar hacia atrás | Q · B (sola) | — (apunta detrás) |
| Cambiar de munición | rueda del ratón · cruceta izq./der. (sola) | rueda del ratón · cruceta izq./der. |

## Torreta y munición

El retroceso es la mecánica central: cada disparo empuja al buggy propio en sentido contrario. Disparar hacia delante frena un poco; disparar hacia atrás acelera y castiga al que viene detrás. Todo lo decide el servidor (`Server` RPC validada, impulsos con `AddImpulse` en el servidor y `ForceNetUpdate`).

| Munición | Origen | Efecto [1.ª pasada] | Retroceso |
|---|---|---|---|
| Coco | infinita; 6 disparos seguidos sobrecalientan 2,5 s | impacto: impulso lateral 350 cm/s y bamboleo de dirección 0,4 s | 120 cm/s |
| Alga | caja, 2 cargas | al tocar buggy o suelo deja un charco de 6 m durante 5 s: agarre ×0,5 y velocidad máx. ×0,6 a **cualquier** buggy dentro, incluido el propio | 60 cm/s |
| Burbuja | caja, 1 carga | burbuja lenta que flota 6 s; el primer buggy que la toca (propio o rival) gana un escudo de 4 s que anula un impacto o un charco | 0 |
| Mortero | caja, 1 carga | explosión de 5 m: impulso vertical 450 cm/s sin vuelco forzado a todos los buggies dentro, también al propio | 700 cm/s (hacia atrás = turbo) |
| Tinta | caja, 2 cargas | mancha la pantalla de las dos ocupantes del buggy alcanzado 3 s | 60 cm/s |
| Ancla | caja (rara), 2 cargas | se engancha al buggy alcanzado y lo frena 2 s | 200 cm/s |

Cajas de munición: filas en las puertas pares y a mitad de tramo; reaparecen a los 3 s; el reparto pondera por puesto (los últimos, más Mortero y Burbuja; los primeros, más Alga y Tinta; el Ancla es rara en todos los puestos, algo menos para los primeros).

## Arquitectura

| Pieza | Carpeta | Qué hace |
|---|---|---|
| `ITN_RallyVehicle` | `Rally/TN_RallyVehicle.h` | Contrato buggy ↔ carrera (plazas, reaparición, motor, munición, mando IA) |
| `ATN_Buggy` | `Vehicles/` | Port de `AHYBuggy` sin `Cargo` ni fichas: Chaos Vehicles, derrape, enderezado, cámara, tinte por equipo, asientos y tortugas visuales |
| `ATN_BuggyGunnerPawn` | `Vehicles/` | Peón de la artillera, sujeto a `Seat_Gunner`, con cámara y apuntado replicado |
| `UTN_BuggyTurretComponent` | `Vehicles/` | Apuntado, calentamiento, munición especial, disparo con retroceso |
| `ATN_RallyProjectile` y efectos | `Vehicles/` | Proyectiles replicados, charco de alga, burbuja, explosión, tinta |
| `ATN_RallyTrack` | `Rally/` | Lee `checkpoints_uu` del manifest de la variante; spline, puertas, bordes con mallas existentes, parrilla y cajas |
| `ATN_RallyGameMode` / `GameState` / `PlayerState` | `Rally/` | Emparejado, fases, puertas en orden, vueltas, puestos a 5 Hz, contramano, reaparición, meta y resultados |
| `TNRally::` (lógica pura) | `Rally/TN_RallyLogic.h` | Orden de puestos, contramano, validación de puerta (regla del 60 %), puntos; con tests `Tortunabo.Rally.*` |
| `ATN_RallyAIController` | `Rally/` | Piloto IA: sigue la spline, frena en curva, dispara al de delante |
| `UTN_RallyHUDWidget` | `Rally/` | Velocímetro, puesto, vuelta y puerta, semáforo, contramano, torreta, tinta, resultados (C++ con `TN_RaceUIKit`) |

Red: el servidor tiene la autoridad del buggy (movimiento replicado de Chaos, `PredictiveInterpolation`), de las puertas, los puestos y los impactos. Los clientes solo mandan entradas (conducción por el movimiento de Chaos, apuntado y disparo por RPC validada).

## Pruebas sin editor (fuera de Shipping)

- Nivel: `Scripts/build_rally_level.py` crea `LVL_Rally` (editor headless **sin** `-nullrhi`: con `-nullrhi` el editor revienta al colocar actores del proyecto). No está en `MapsToCook`.
- Carreras solo de IA: `LVL_Rally?Variant=V?Bots=4?AutoStart?Races=10?RaceTimeout=300 -server -nullrhi`; cada carrera deja una línea `[RallyStats]` (terminados, atascos, vuelcos, caídas, fuera de pista, ganador).
- Comandos: `TN.Rally.Measure [s] [cerrar]` (#100), `TN.Rally.StatusLater`, `TN.Rally.LocalFire`, `TN.Rally.DebugCosmetics`, `TN.Rally.DebugTeleport`.
- Medida en I03R: punta 113,5 km/h, 0-100 km/h 6,6 s, frenada desde 60 km/h 12,9 m. En E01B: punta 99,8 km/h (no llega a 100 en 25 s), frenada 13,2 m.
- Piloto IA (2026-10-01, 7 carreras por variante): I03R 23/28 terminados, 3 atascos, 1 vuelco, 3 caídas al agua, ganador 119-123 s; E01B 23/28, 2 atascos, 1 vuelco, 1 caída, ganador 111-120 s. Los fallos se concentran tras choques entre buggies: en I03R, en la curva de la cabeza (arco 200-285 m, con agua por fuera); en E01B, fuera de la calzada en un talud.

## Fuera de este MVP

Física síncrona con subpasos (#101), `GeoRegion` y validador completo del corredor (#107–#109), decimado (#111), atribuciones (#112), skins y tienda (#114, #115), sonda de red con 8 buggies (#118), copa de 3 carreras y la integración en lobby y menú.

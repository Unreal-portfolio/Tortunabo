# Pruebas de red local e informe de bug

Herramientas para probar la red sin Steam y para reportar fallos: hasta 8 instancias del juego en un PC con emulación de red, el monkey en los clientes remotos para medir correcciones y el informe de bug con F8. Solo compilaciones que no son Shipping. Issues #65, #80 y #67 (lote #386). El monkey y el estrés están en [`Estres-Monkey-2026-09-29.md`](Estres-Monkey-2026-09-29.md).

## Monkey en un cliente remoto

Un cliente lanzado con la dirección del servidor (`127.0.0.1:7777 -game -TNMonkey=60:9`) que no llega a conectar (por ejemplo, porque el servidor aún no escucha: a los 20 s da `ConnectionTimeout`) vuelve al mapa por defecto (`LVL_Menu`). Antes, el monkey arrancaba en ese mundo, sin jugadores, y el informe salía vacío. Ahora:

- `-TNMonkeyNet=client|server|any` elige en qué mundo arranca: `client` solo en el mundo conectado (`NM_Client`), `server` solo en el que hace de servidor y `any` en cualquiera.
- Sin `-TNMonkeyNet`, si el primer argumento de la línea de órdenes es una dirección (IPv4 con o sin puerto, `localhost` o `steam.<id>`), espera al mundo de cliente; con un mapa, arranca donde siempre.
- A mano, dentro de la partida: `TN.Monkey 60 9` en la consola del cliente.

El informe (`-TNMonkeyOut` o `Saved/Monkey/<fecha>.json`) lleva `net_mode: Client`, `players` y `net_corrections` (las correcciones del servidor que registra `p.NetShowCorrections`, que el monkey enciende mientras dura). Lógica en `Testing/TN_MonkeyNetStart.h`, tests `Tortunabo.Monkey.NetStart`.

## Informe de bug con F8

F8 en la ventana del juego (también `TN.BugReport` en la consola, o `-TNBugReportAfter=<s>` en la línea de órdenes) crea `Saved/BugReports/<aaaa-mm-dd_hh-mm-ss>/` con:

| Fichero | Contenido |
| --- | --- |
| `informe.md` | Listo para pegar en una issue nueva con la plantilla «Fallo»: pasos, esperado y obtenido y criterios por rellenar; tabla de contexto (fecha, commit, compilación, mapa, modo, red, posición, semillas) y los últimos 15 errores y avisos del registro. |
| `captura.png` | Captura con la interfaz, del siguiente fotograma. No hay en `-nullrhi` (el Markdown lo dice). |
| `log.txt` | Las últimas 2000 líneas del registro. |
| `partida.json` | Commit, mapa, modo, semillas, red (modo, conexiones, dirección del servidor, ping, `PktLag`/`PktLoss` activos), estado de la partida y propiedades del GameState declaradas en el juego. |
| `jugador.json` | Cada jugador local: controlador, pawn, posición y velocidad en metros, rotación, rol de red, modo de movimiento y las propiedades del juego del pawn y de su PlayerState. |

- El commit se lee de `.git` (también en worktrees: `abc1234ef (rama)`); en una build empaquetada sale «desconocido».
- Las semillas son las propiedades enteras con «Seed» en el nombre del GameState y del GameMode (este solo en el servidor), más la del monkey si está en marcha.
- F8 lo lee un `IInputProcessor` de Slate desde `UTN_BugReportSubsystem` (subsistema del GameInstance), antes que el PlayerController y el HUD. En PIE solo responde la ventana con el foco y la tecla no sigue: F8 ya no expulsa del pawn en PIE (el botón «Eject» de la barra sigue).
- No hay aviso en pantalla: la ruta sale en el registro (`[Informe] F8: informe de bug en …`).
- Solo fuera de Shipping (el subsistema no se crea en Shipping). Lógica pura en `Testing/TN_BugReport.h`, tests `Tortunabo.BugReport`.

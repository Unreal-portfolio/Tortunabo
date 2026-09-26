# Pantalla de carga del huevo

En cada cambio de mapa, Tortunavy enseña un huevo que se cierra, se balancea mientras carga y, cuando el mapa está
listo, tiembla, se agrieta y revienta con un «¡PUM!» para dejar ver la partida. Todo es código: arte pintado en
ejecución, Slate y sonido sintetizado.

## Piezas

| Pieza | Archivo | Qué hace |
|---|---|---|
| `UTN_LoadingScreenSubsystem` | `UI/Loading/TN_LoadingScreenSubsystem.*` | `UGameInstanceSubsystem` + `FTickableGameObject`: decide cuándo se enseña y cuándo se rompe. |
| `STN_EggLoadingScreen` | `Private/UI/Loading/STN_EggLoadingScreen.*` | La escena (cielo con estrellas, arena, cuatro tortugas andando, el huevo), el título, el estado con puntos animados y los consejos que rotan. |
| `UTN_EggSynthComponent` | `UI/Loading/TN_LoadingScreenSubsystem.*` | Crujidos (ruido filtrado con chasquidos) y el «¡pum!» (golpe grave que cae de tono), en 2D. |

## Cuándo sale y cuándo se rompe

- **Carga bloqueante** (`FCoreUObjectDelegates::PreLoadMapWithContext`): el huevo sale ya cerrado. Fuera del editor,
  MoviePlayer pinta la misma escena en su hilo mientras carga; en PIE el viewport se queda quieto durante la carga y
  la escena sigue al terminar (`PostLoadMapWithWorld` la vuelve a poner en el viewport).
- **Viaje sin cortes** (`World->IsInSeamlessTravel()`): el hilo de juego sigue vivo y la escena se anima todo el rato.
- **Listo** cuando la partida ha empezado (`HasBegunPlay`), hay tortuga propia (o han pasado 6 s) y, en el mapa
  procedural, el terreno está generado (`ATN_ProcMapGenerator::IsMapReady`). Como mínimo se ve 1,2 s y como mucho
  espera 30 s tras la carga.
- **Rotura** (1,5 s): temblor creciente, grietas, a los 0,75 s el «¡PUM!» con fogonazo y trozos de cáscara, y a los
  0,95 s empieza el fundido.
- `UMP_GameInstance::ShowLoadingScreen` le pasa sus mensajes cuando el huevo está a la vista (no se apilan dos
  pantallas) y `FriendlyStatusForMap` da textos amables por mapa.

## Pruebas por consola

- `TN.Loading.Test`: enseña el huevo y lo rompe a los 3 s.
- `TN.Loading.Test.Hold`: lo deja cargando.
- `TN.Loading.Test.Break`: rompe el que esté a la vista.

Necesita el módulo `MoviePlayer` en `Tortunabo.Build.cs`.

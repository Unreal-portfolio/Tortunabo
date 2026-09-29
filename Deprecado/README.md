# Deprecado

Código y scripts retirados del proyecto sin borrarlos, para no perder trabajo de diseño.
Nada de esta carpeta se compila ni se ejecuta: `Deprecado/Source` está fuera del módulo
`Source/Tortunabo`, y los assets de Content retirados viven en `/Game/_Deprecado`, que
`Config/DefaultGame.ini` excluye del cocinado (`DirectoriesToNeverCook`).

Para recuperar algo: `git mv Deprecado/<ruta> <ruta>` (conserva el historial) y compilar.
El inventario completo, con la evidencia de cada entrada, está en
`Docs/Limpieza-2026-09-29.md`.

## Entradas

| Ruta original | Origen | Motivo |
|---|---|---|
| `Source/Tortunabo/{Public,Private}/Player/TortugaFirstPersonCharacter.*` | Personaje en primera persona de la plantilla inicial | 0 referencias en C++, Config y Content (limpieza 1.3) |
| `Source/Tortunabo/{Public,Private}/UI/HUD/TN_CosmeticsMenuWidget.*` y `World/TN_CosmeticsStationInteractable.*` | Primer menú de cosméticos (Rodrigo y Mokius) | Ningún WBP hereda de él; la tienda vigente es `UI/Shop/TN_ShopWidgets` (limpieza 1.4) |
| `Source/Tortunabo/{Public,Private}/Player/TN_LegAnimComponent.*` | Animación procedural de patas (Rodrigo y Mokius) | Ninguna cabecera lo incluye y 0 referencias en Content (limpieza 1.5) |
| `build_check.bat` | Script de compilación de Mokius | Ruta fija a su equipo y configuración `Development`; el editor usa `DebugGame` (limpieza 1.6) |

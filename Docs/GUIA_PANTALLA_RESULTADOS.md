# Guía: Pantalla de Resultados — solo diseño visual

## Qué hace el C++ automáticamente

Toda la lógica está en `TN_CoopFlowHUDWidget`. No necesitas nada en el Event Graph.
Al entrar en estado **Results** el código:

1. Muestra `ResultsOverlay`
2. Escribe el título según el puesto (¡PRIMER LUGAR! / ¡ELIMINADO! / etc.)
3. Escribe el puesto y el tiempo del jugador local
4. Muestra la pista de espectador si el jugador no terminó
5. Actualiza el countdown cada 0.1 s hasta el travel de vuelta a lobby

---

## Lo único que debes hacer en Blueprint: añadir widgets con estos nombres exactos

Abre tu `WBP_CoopFlowHUD` en el **Widget Designer** y construye esta jerarquía:

```
Canvas Panel
  │
  ├── VerticalBox  ──────────────── nombre: RootContainer   (ya existe)
  │     ├── TextBlock ────────────── nombre: PrimaryText    (ya existe)
  │     └── TextBlock ────────────── nombre: SecondaryText  (ya existe)
  │
  └── Overlay  ─────────────────── nombre: ResultsOverlay   ← NUEVO
        ├── Image                  (fondo oscuro, Fill, Opacity 0.75 — sin nombre especial)
        └── VerticalBox            (sin nombre especial, centrado)
              ├── TextBlock ──────── nombre: ResultsTitle
              ├── TextBlock ──────── nombre: ResultsRankText
              ├── TextBlock ──────── nombre: ResultsTimeText
              ├── TextBlock ──────── nombre: ResultsCountdown
              └── TextBlock ──────── nombre: SpectatorHint
```

### Configuración obligatoria de `ResultsOverlay`
- **Visibility** en Details → **Collapsed**  (el C++ lo muestra/oculta)
- Canvas Slot: ancla a los 4 bordes (Full Screen) o usa Size To Content = false

### El resto (fuente, colores, padding, animaciones) es libre — ponlo como quieras.

---

## Nombres exactos requeridos

| Widget | Tipo sugerido | Texto por defecto |
|---|---|---|
| `ResultsOverlay` | Overlay (o cualquier contenedor) | — |
| `ResultsTitle` | TextBlock | (vacío) |
| `ResultsRankText` | TextBlock | (vacío) |
| `ResultsTimeText` | TextBlock | (vacío) |
| `ResultsCountdown` | TextBlock | (vacío) |
| `SpectatorHint` | TextBlock | (vacío) |

> El nombre debe coincidir **exactamente** (mayúsculas incluidas) con los de esta tabla.
> Si un widget no existe en tu BP, el C++ simplemente lo ignora — no hay crash.

---

## Qué escribe el C++ en cada widget

| Widget | Estado Results (terminó) | Estado Results (eliminado) |
|---|---|---|
| `ResultsTitle` | ¡PRIMER LUGAR! / ¡SEGUNDO LUGAR! … | ¡ELIMINADO! |
| `ResultsRankText` | Puesto: #1 | Eliminado |
| `ResultsTimeText` | Tiempo: 12.3s | (vacío) |
| `ResultsCountdown` | Volviendo al lobby en: 8 | Volviendo al lobby en: 8 |
| `SpectatorHint` | (vacío — ya terminó) | Scroll para cambiar de jugador |

---

## Hook Blueprint opcional

Si quieres añadir efectos visuales (animaciones, sonidos, etc.) al cambio de estado,
implementa el evento `On Flow State Changed` en el Event Graph del widget.
El C++ lo llama en cada transición. No es obligatorio.

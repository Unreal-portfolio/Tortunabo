# Bug Fix Plan: Replicación de Chunks, Física y Widget de Resultados

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Corregir 6 bugs de replicación multijugador detectados en testing: widget de resultados invisible en clientes (#B6/#B7), pawn muerto destruye chunk y rompe replicación al revivir (#B3), pelota de física parada en cliente (#B5), y puerta atravesable con estado incorrecto tras muerte (#B4/#B8).

**Architecture:** Cada bug tiene causa raíz distinta y se arregla de forma quirúrgica. No se cambia arquitectura, solo se parchean los puntos concretos donde la replicación falla. Orden: widget (sin riesgo) → física (bajo riesgo) → chunks (complejidad media) → puerta (BP puro, fuera de C++).

**Tech Stack:** Unreal Engine 5.6, C++ (sin test suite automatizada), validación con smoke tests en PIE/Standalone multijugador.

> **Nota de compilación:** Siempre compilar con el comando de build en la sección al final. Nunca usar Live Coding en tests multijugador.

---

## MAPA DE ARCHIVOS

| Tarea | Archivos modificados |
|-------|---------------------|
| Task 1 — Widget resultados | `Source/Tortunabo/Private/UI/HUD/TN_CoopFlowHUDWidget.cpp` |
| Task 2 — Widget: bind delegate | `Source/Tortunabo/Private/UI/HUD/TN_CoopFlowHUDWidget.cpp`, `Source/Tortunabo/Public/UI/HUD/TN_CoopFlowHUDWidget.h` |
| Task 3 — ChunkManager safe position | `Source/Tortunabo/Public/World/TN_ChunkManager.h`, `Source/Tortunabo/Private/World/TN_ChunkManager.cpp` |
| Task 4 — RevivePlayer teleport | `Source/Tortunabo/Private/Game/TN_RunGameMode.cpp` |
| Task 5 — Pelota física (BP) | `Content/Blueprints/...` (sin C++, solo editor) |
| Task 6 — Puerta (BP) | `Content/Blueprints/...` (sin C++, solo editor) |

---

## Task 1: Diagnóstico — Widget de resultados (`#B6/#B7`)

**Contexto:** `ResultsOverlay` usa `BindWidgetOptional`. Si el BP Widget Designer no tiene un widget con el nombre exacto `ResultsOverlay`, el puntero es `nullptr` y `ShowResultsPanel()` es un no-op silencioso. Primero diagnosticamos antes de tocar nada.

**Files:**
- Modify: `Source/Tortunabo/Private/UI/HUD/TN_CoopFlowHUDWidget.cpp:274`

- [ ] **Step 1: Añadir logs de diagnóstico en ShowResultsPanel**

En `TN_CoopFlowHUDWidget.cpp`, localizar `void UTN_CoopFlowHUDWidget::ShowResultsPanel(...)` (línea ~274). Reemplazar el bloque completo con la versión con logs:

```cpp
void UTN_CoopFlowHUDWidget::ShowResultsPanel(const ATN_CoopGameState* GameState)
{
	bResultsVisible = true;

	// ── Diagnóstico de BindWidget ──────────────────────────────────────────
	UE_LOG(LogTemp, Warning, TEXT("[HUD] ShowResultsPanel called. ResultsOverlay=%s ResultsTitle=%s ResultsRankText=%s ResultsCountdown=%s"),
		ResultsOverlay   ? TEXT("OK") : TEXT("NULL — check BP Designer widget name"),
		ResultsTitle     ? TEXT("OK") : TEXT("NULL"),
		ResultsRankText  ? TEXT("OK") : TEXT("NULL"),
		ResultsCountdown ? TEXT("OK") : TEXT("NULL"));

	if (ResultsOverlay)
	{
		ResultsOverlay->SetVisibility(ESlateVisibility::Visible);
	}

	// ── Gather local player stats ────────────────────────────────────────────
	const ATN_CoopPlayerState* TNPS = nullptr;
	if (const APlayerController* PC = GetOwningPlayer())
	{
		TNPS = PC->GetPlayerState<ATN_CoopPlayerState>();
	}

	if (!TNPS)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HUD] ShowResultsPanel: TNPS is null — PlayerState not yet replicated. Results content will be empty."));
	}

	const int32  Rank          = TNPS ? TNPS->FinishRank       : 0;
	const float  Time          = TNPS ? TNPS->FinishTimeSeconds : -1.f;
	const bool   bEliminated   = TNPS && TNPS->bIsEliminated;
	const bool   bFinishedNorm = TNPS && TNPS->bHasFinishedRun && Rank > 0 && !bEliminated;

	if (ResultsTitle)
	{
		ResultsTitle->SetText(BuildRankTitle(Rank, bEliminated));
	}

	if (ResultsRankText)
	{
		if (bEliminated)
		{
			ResultsRankText->SetText(FText::FromString(TEXT("Eliminado")));
		}
		else if (bFinishedNorm)
		{
			ResultsRankText->SetText(FText::FromString(FString::Printf(TEXT("Puesto: #%d"), Rank)));
		}
		else
		{
			ResultsRankText->SetText(FText::GetEmpty());
		}
	}

	if (ResultsTimeText)
	{
		ResultsTimeText->SetText((bFinishedNorm && Time > 0.f)
			? FText::FromString(FString::Printf(TEXT("Tiempo: %.1fs"), Time))
			: FText::GetEmpty());
	}

	if (SpectatorHint)
	{
		SpectatorHint->SetText(bEliminated
			? FText::FromString(TEXT("Scroll para cambiar de jugador"))
			: FText::GetEmpty());
	}

	RefreshResultsCountdown(GameState);
}
```

- [ ] **Step 2: Compilar y verificar que compila**

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" TortunaboEditor Win64 Development "C:\Users\mokiu\Documents\Unreal Projects\Tortunabo\Tortunabo.uproject" -WaitMutex -NoHotReload
```

Resultado esperado: `BUILD SUCCESSFUL` sin errores. Si hay errores de compilación, revisar que el include de `TN_CoopPlayerState` está en el .cpp (ya lo estaba).

- [ ] **Step 3: Abrir editor y lanzar PIE con 2 jugadores**

En editor: Play → Number of Players: 2 → Standalone Game. Completar la carrera (o morir). Al llegar a resultados, revisar el Output Log:
- Si `ResultsOverlay=NULL` → proceder a Task 2 (fix en BP)
- Si `ResultsOverlay=OK` pero el widget no se ve → problema distinto (Task 2 backup)
- Si `ShowResultsPanel called` no aparece → el estado Results no llega al cliente (problema de replicación)

- [ ] **Step 4: Commit del log de diagnóstico**

```bash
git add Source/Tortunabo/Private/UI/HUD/TN_CoopFlowHUDWidget.cpp
git commit -m "debug(HUD): add diagnostic logs for ResultsOverlay BindWidget null check"
```

---

## Task 2: Fix Widget — BindWidget + Delegate backup (`#B6/#B7`)

**Contexto:** Dos sub-fixes paralelos:
1. **BP fix**: Asegurar que el BP Widget Designer tiene los widgets con los nombres correctos.
2. **C++ fix**: Añadir binding al delegate `OnMatchFlowStateChanged` como canal de respaldo (más inmediato que el polling de 0.1s, y más robusto si el widget se recrea al revivir).

**Files:**
- Modify: `Source/Tortunabo/Public/UI/HUD/TN_CoopFlowHUDWidget.h`
- Modify: `Source/Tortunabo/Private/UI/HUD/TN_CoopFlowHUDWidget.cpp`

### Sub-fix 2A: BP Designer (en el editor de UE5)

- [ ] **Step 1: Abrir BP_CoopFlowHUDWidget en UE5 Editor**

En Content Browser → `Game/Blueprints/UI/HUD/` → doble click en `BP_CoopFlowHUDWidget` (o el nombre que tenga). Abrir la pestaña Designer.

- [ ] **Step 2: Verificar y añadir widgets con nombres exactos**

En la jerarquía del Designer, verificar que existen los siguientes widgets con EXACTAMENTE estos nombres (case-sensitive, sin espacios extra):

| Nombre requerido | Tipo UMG sugerido | Si no existe |
|-----------------|-------------------|--------------|
| `ResultsOverlay` | Overlay o Border | Añadir un Overlay encima del canvas |
| `ResultsTitle` | TextBlock | Añadir dentro de ResultsOverlay |
| `ResultsRankText` | TextBlock | Añadir dentro de ResultsOverlay |
| `ResultsTimeText` | TextBlock | Añadir dentro de ResultsOverlay |
| `ResultsCountdown` | TextBlock | Añadir dentro de ResultsOverlay |
| `SpectatorHint` | TextBlock | Añadir dentro de ResultsOverlay |

Para añadir: en la paleta de widgets, arrastrar el tipo al Designer. En el panel de Details → Name → escribir exactamente el nombre de la tabla.

- [ ] **Step 3: ResultsOverlay empieza oculto**

Seleccionar `ResultsOverlay` → en Details → Visibility → **Collapsed** (el C++ lo mostrará cuando corresponda).

- [ ] **Step 4: Compilar y guardar el BP**

En el BP Editor: File → Compile → Save. Verificar que no hay errores de compilación en el BP.

### Sub-fix 2B: Añadir binding a OnMatchFlowStateChanged (C++)

- [ ] **Step 5: Añadir handler en el .h**

En `TN_CoopFlowHUDWidget.h`, al final de la sección `private:`, añadir:

```cpp
private:
	// [líneas existentes...]
	
	// ── OnMatchFlowStateChanged binding — backup channel besides polling ──────
	// Binding allows immediate response when state changes, covering the case
	// where the widget is recreated (e.g. after ClientRestart on revive) and 
	// the polling misses the transition because the timer hasn't fired yet.
	UFUNCTION()
	void OnMatchFlowStateChangedHandler(ETNMatchFlowState NewState);

	UPROPERTY(Transient)
	TObjectPtr<ATN_CoopGameState> BoundFlowGameState;
```

- [ ] **Step 6: Implementar binding en el .cpp**

En `TN_CoopFlowHUDWidget.cpp`, añadir al final del archivo:

```cpp
void UTN_CoopFlowHUDWidget::OnMatchFlowStateChangedHandler(ETNMatchFlowState NewState)
{
	ATN_CoopGameState* GameState = GetOwningPlayer()
		? GetOwningPlayer()->GetWorld()->GetGameState<ATN_CoopGameState>()
		: nullptr;

	if (!GameState)
	{
		return;
	}

	// Sync polling state to avoid double-firing when RefreshTexts next runs
	LastKnownFlowState    = NewState;
	bFlowStateInitialized = true;

	HandleFlowStateChange(NewState, GameState);
	OnFlowStateChanged(NewState);

	const bool bShow = ShouldBeVisible(NewState);
	SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
```

- [ ] **Step 7: Activar binding en BindQuickChat y desactivar en UnbindQuickChat**

Localizar `void UTN_CoopFlowHUDWidget::BindQuickChat(ATN_CoopGameState* GameState)` (línea ~173). Añadir el binding del delegate de flow state al final del bloque que ya hace bind:

```cpp
void UTN_CoopFlowHUDWidget::BindQuickChat(ATN_CoopGameState* GameState)
{
	// [código existente de BoundQuickChatGameState — no modificar]
	if (!GameState)
	{
		UnbindQuickChat();
		return;
	}

	if (BoundQuickChatGameState == GameState)
	{
		if (!bQuickChatHistoryReplayed)
		{
			ReplayQuickChatHistory(GameState);
		}
		// ── Bind flow state delegate si no está ya bound ──────────────────
		if (BoundFlowGameState != GameState)
		{
			if (BoundFlowGameState)
			{
				BoundFlowGameState->OnMatchFlowStateChanged.RemoveDynamic(
					this, &UTN_CoopFlowHUDWidget::OnMatchFlowStateChangedHandler);
			}
			GameState->OnMatchFlowStateChanged.AddUniqueDynamic(
				this, &UTN_CoopFlowHUDWidget::OnMatchFlowStateChangedHandler);
			BoundFlowGameState = GameState;
		}
		return;
	}

	UnbindQuickChat();
	BoundQuickChatGameState = GameState;
	GameState->OnQuickChatReceived.AddUniqueDynamic(this, &UTN_CoopFlowHUDWidget::HandleQuickChatReceived);
	ReplayQuickChatHistory(GameState);

	// ── Bind flow state delegate ─────────────────────────────────────────
	if (BoundFlowGameState != GameState)
	{
		if (BoundFlowGameState)
		{
			BoundFlowGameState->OnMatchFlowStateChanged.RemoveDynamic(
				this, &UTN_CoopFlowHUDWidget::OnMatchFlowStateChangedHandler);
		}
		GameState->OnMatchFlowStateChanged.AddUniqueDynamic(
			this, &UTN_CoopFlowHUDWidget::OnMatchFlowStateChangedHandler);
		BoundFlowGameState = GameState;
	}
}
```

Localizar `void UTN_CoopFlowHUDWidget::UnbindQuickChat()` (línea ~196). Añadir unbind del delegate al final:

```cpp
void UTN_CoopFlowHUDWidget::UnbindQuickChat()
{
	if (BoundQuickChatGameState)
	{
		BoundQuickChatGameState->OnQuickChatReceived.RemoveDynamic(this, &UTN_CoopFlowHUDWidget::HandleQuickChatReceived);
		BoundQuickChatGameState = nullptr;
	}

	bQuickChatHistoryReplayed = false;
	LastQuickChatSequenceSeen = 0;

	// ── Unbind flow state delegate ──────────────────────────────────────────
	if (BoundFlowGameState)
	{
		BoundFlowGameState->OnMatchFlowStateChanged.RemoveDynamic(
			this, &UTN_CoopFlowHUDWidget::OnMatchFlowStateChangedHandler);
		BoundFlowGameState = nullptr;
	}
}
```

- [ ] **Step 8: Compilar**

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" TortunaboEditor Win64 Development "C:\Users\mokiu\Documents\Unreal Projects\Tortunabo\Tortunabo.uproject" -WaitMutex -NoHotReload
```

Resultado esperado: `BUILD SUCCESSFUL`.

- [ ] **Step 9: Smoke test — pantalla de resultados**

En editor: Play → 2 Players → Standalone. Escenario 1: ambos jugadores terminan la carrera normalmente. Verificar en el cliente (ventana secundaria) que aparece la pantalla de resultados. Escenario 2: un jugador muere y es revivido, luego ambos terminan. Verificar pantalla en ambas ventanas.

Output Log esperado: `[HUD] ShowResultsPanel called. ResultsOverlay=OK ...`

- [ ] **Step 10: Commit**

```bash
git add Source/Tortunabo/Public/UI/HUD/TN_CoopFlowHUDWidget.h
git add Source/Tortunabo/Private/UI/HUD/TN_CoopFlowHUDWidget.cpp
git commit -m "fix(HUD): bind OnMatchFlowStateChanged delegate + diagnostic logs for ResultsOverlay (#B6 #B7)"
```

---

## Task 3: ChunkManager — exponer posición de revive segura (`#B3` parte 1/2)

**Contexto:** `CleanupChunks()` destruye chunks sin saber que un pawn muerto está posicionado dentro. Necesitamos que `RevivePlayer` pueda consultar al ChunkManager cuál es la posición de respawn segura actual (el `NextSpawnTransform`), para mover al pawn muerto a un chunk activo antes de revivir.

**Files:**
- Modify: `Source/Tortunabo/Public/World/TN_ChunkManager.h`
- Modify: `Source/Tortunabo/Private/World/TN_ChunkManager.cpp`

- [ ] **Step 1: Exponer getter público en TN_ChunkManager.h**

En `TN_ChunkManager.h`, al final de la sección `public:` (después de `bDebugDrawSockets`), añadir:

```cpp
public:
	// [declaraciones existentes...]

	/**
	 * Devuelve la posición del OutSocket del último chunk spawneado,
	 * que es donde el próximo chunk empezará y está garantizado que es
	 * un área "activa" (nunca será destruida por CleanupChunks).
	 * Usado por TN_RunGameMode::RevivePlayer para teleportar pawns muertos
	 * a un área con chunks válidos antes de revivir.
	 * Retorna FVector::ZeroVector si aún no se ha spawneado ningún chunk.
	 */
	UFUNCTION(BlueprintPure, Category = "Chunks")
	FVector GetSafeReviveLocation() const;
```

- [ ] **Step 2: Implementar el getter en TN_ChunkManager.cpp**

Al final de `TN_ChunkManager.cpp`, añadir:

```cpp
FVector ATN_ChunkManager::GetSafeReviveLocation() const
{
	// NextSpawnTransform es el OutSocket del último chunk spawneado.
	// Siempre apunta a un área activa (el próximo chunk se spawneará aquí).
	// Elevamos 100 cm para evitar que el pawn aparezca dentro del suelo.
	return NextSpawnTransform.GetLocation() + FVector(0.f, 0.f, 100.f);
}
```

- [ ] **Step 3: Compilar**

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" TortunaboEditor Win64 Development "C:\Users\mokiu\Documents\Unreal Projects\Tortunabo\Tortunabo.uproject" -WaitMutex -NoHotReload
```

Resultado esperado: `BUILD SUCCESSFUL`.

- [ ] **Step 4: Commit parcial**

```bash
git add Source/Tortunabo/Public/World/TN_ChunkManager.h
git add Source/Tortunabo/Private/World/TN_ChunkManager.cpp
git commit -m "feat(ChunkManager): expose GetSafeReviveLocation for dead pawn teleport (#B3 prep)"
```

---

## Task 4: RevivePlayer — teleportar pawn muerto a zona segura (`#B3` parte 2/2)

**Contexto:** Usar `GetSafeReviveLocation()` en `RevivePlayer()` para mover al pawn hidden a un área con chunks activos ANTES de re-poseerse. Si no hay ChunkManager (mapa de lobby), el comportamiento es el mismo que antes.

**Files:**
- Modify: `Source/Tortunabo/Private/Game/TN_RunGameMode.cpp`

Añadir include al principio del .cpp si no está ya:
```cpp
#include "World/TN_ChunkManager.h"
#include "EngineUtils.h" // Para TActorIterator (ya estaba incluido)
```

- [ ] **Step 1: Localizar en TN_RunGameMode.cpp el bloque de RevivePlayer donde se restaura el pawn**

Buscar el comentario `// ── Restaurar visibilidad (el pawn fue ocultado en MarkPlayerDead)` (aproximadamente línea 666). El bloque es:

```cpp
if (Pawn)
{
    // Restaurar visibilidad (el pawn fue ocultado en MarkPlayerDead)
    Pawn->SetActorHiddenInGame(false);
    Pawn->SetActorEnableCollision(true);
    ...
```

- [ ] **Step 2: Añadir teleport a zona segura ANTES de restaurar visibilidad**

Reemplazar el inicio del bloque `if (Pawn)` (solo la parte del teleport, no tocar nada más):

```cpp
if (Pawn)
{
	// ── Teleportar a zona segura si hay chunks activos ────────────────────
	// Si el pawn muerto está en un chunk que ya fue destruido por CleanupChunks,
	// re-poseer en esa posición causa errores de replicación en el cliente.
	// Buscamos el ChunkManager y usamos su OutSocket actual como zona segura.
	ATN_ChunkManager* ChunkManager = nullptr;
	for (TActorIterator<ATN_ChunkManager> It(GetWorld()); It; ++It)
	{
		ChunkManager = *It;
		break;
	}

	if (ChunkManager)
	{
		const FVector SafeLocation = ChunkManager->GetSafeReviveLocation();
		// Solo teleportar si el ChunkManager tiene chunks spawneados
		// (GetSafeReviveLocation retorna ZeroVector antes del primer spawn)
		if (!SafeLocation.IsNearlyZero())
		{
			Pawn->SetActorLocation(SafeLocation, false, nullptr, ETeleportType::TeleportPhysics);
			UE_LOG(LogTemp, Log, TEXT("[Revive] Teleported dead pawn to safe chunk location (%.0f,%.0f,%.0f)"),
				SafeLocation.X, SafeLocation.Y, SafeLocation.Z);
		}
	}

	// ── Restaurar visibilidad (el pawn fue ocultado en MarkPlayerDead) ─────
	Pawn->SetActorHiddenInGame(false);
	Pawn->SetActorEnableCollision(true);
    // [resto del código existente sin tocar...]
```

- [ ] **Step 3: Añadir include de ChunkManager si falta**

Al inicio de `TN_RunGameMode.cpp`, en el bloque de includes, verificar que existe:
```cpp
#include "World/TN_ChunkManager.h"
```
Si no existe, añadirlo junto a los demás includes de World.

- [ ] **Step 4: Compilar**

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" TortunaboEditor Win64 Development "C:\Users\mokiu\Documents\Unreal Projects\Tortunabo\Tortunabo.uproject" -WaitMutex -NoHotReload
```

Resultado esperado: `BUILD SUCCESSFUL`.

- [ ] **Step 5: Smoke test — die in chunk, revive, verify no replication errors**

En editor: Play → 2 Players → Standalone. Escenario:
1. Jugador A muere en un chunk medio (no el primero)
2. Jugador B avanza 3-4 chunks adelante (para que el chunk de A sea destruido por CleanupChunks)
3. Jugador B interactúa con el RescuePickup para revivir a A
4. Verificar: A aparece en una zona con chunks activos (no en el vacío), sin errores de replicación en Output Log
5. Buscar en Output Log: `[Revive] Teleported dead pawn to safe chunk location`
6. Buscar ausencia de: `Actor replication error`, `cannot find net actor`

- [ ] **Step 6: Commit**

```bash
git add Source/Tortunabo/Private/Game/TN_RunGameMode.cpp
git add Source/Tortunabo/Public/World/TN_ChunkManager.h  # si se modificó el include
git commit -m "fix(RevivePlayer): teleport dead pawn to active chunk area before revive (#B3)"
```

---

## Task 5: Pelota física — replicación en cliente (`#B5`)

**Contexto:** El actor de la pelota (o cualquier objeto físico en los chunks) tiene su física simulando localmente en el cliente pero sin recibir actualizaciones del servidor. La pelota se queda inmóvil en el cliente hasta que la posición final se replica y hace snap. El fix es puro Blueprint: cambiar la configuración de replicación del actor.

**Files:**
- Modify: `Content/Blueprints/World/[BP del actor pelota]` en editor

> **Nota:** Si el actor de la pelota tiene clase C++ dedicada (no visible en el análisis de código), también habría que cambiar `bReplicates` y `bReplicateMovement` en el constructor C++. Esta task asume que es un BP actor sin clase C++ propia.

- [ ] **Step 1: Encontrar el BP actor de la pelota**

En Content Browser, buscar el actor de la pelota. Probablemente está en `Game/Blueprints/World/` o en alguno de los chunks. Abrir el BP.

- [ ] **Step 2: En Class Defaults — activar replicación**

En el BP Editor → Class Defaults (icono de engranaje o pestaña Class Defaults):
- `Replicates` → **checked (true)**
- `Replicate Movement` → **checked (true)**

- [ ] **Step 3: En BeginPlay — desactivar física en clientes**

En el Event Graph del BP, en `Event BeginPlay`, añadir:

```
Event BeginPlay
  → Switch Has Authority
      → [Remote] → StaticMesh (o la malla del actor) → Set Simulate Physics → False
      → [Authority] → (no hacer nada — el servidor simula)
```

Esto hace que solo el servidor simule la física. Los clientes reciben la posición replicada vía `Replicate Movement`.

- [ ] **Step 4: Compilar y guardar el BP**

En BP Editor: Compile → Save.

- [ ] **Step 5: Smoke test — física de la pelota**

En editor: Play → 2 Players → Standalone. Empujar la pelota como jugador en el servidor. Verificar en la ventana del cliente que la pelota se mueve sincrónicamente (sin quedarse parada ni hacer snap).

- [ ] **Step 6: Commit del resultado (después de confirmar que funciona)**

```bash
git commit -m "fix(Physics): ball actor replicate movement, client no-simulate (#B5)"
```

> **Nota:** Si la pelota tiene múltiples instancias en diferentes chunks BPs, repetir Steps 1-4 para cada una. Si hay un actor base compartido (parent BP), el fix en el parent se propaga a todos los hijos.

---

## Task 6: Puerta — replicación de movimiento y colisión (`#B4/#B8`)

**Contexto:** Dos bugs relacionados:
- **#B4**: El personaje puede atravesar la puerta (colisión no coincide con posición visual en clientes)
- **#B8**: Tras morir y revivir, la puerta aparece en posición incorrecta en el cliente

Ambos tienen la misma raíz: el movimiento de la puerta no se replica a clientes. El fix: hacer el movimiento server-authoritative y replicar el ángulo/posición vía `Replicate Movement` o un `float` replicado.

**Files:**
- Modify: `Content/Blueprints/World/[BP del actor puerta]` en editor

- [ ] **Step 1: Encontrar el BP actor de la puerta**

En Content Browser, buscar el actor puerta. Probablemente en `Game/Blueprints/World/` o dentro de un chunk BP. Abrir el BP.

- [ ] **Step 2: Activar replicación en Class Defaults**

En Class Defaults:
- `Replicates` → **checked (true)**
- `Replicate Movement` → **checked (true)**  *(si la puerta usa SetActorLocation/SetActorRotation)*
- `Net Update Frequency` → **30** (por defecto es 100, pero para una puerta 30hz es suficiente)

- [ ] **Step 3: Asegurar que el Timeline corre solo en servidor**

En el Event Graph, localizar el nodo que arranca el Timeline de la puerta (probablemente disparado por un overlap o un botón). Antes del nodo de Play del Timeline, añadir:

```
[Trigger Event / Overlap]
  → Switch Has Authority
      → [Authority] → [Timeline Play Forward / Reverse]
      → [Remote] → (no hacer nada)
```

- [ ] **Step 4: Añadir bAlwaysRelevant para evitar pérdida de estado tras muerte**

En Class Defaults:
- `Always Relevant` → **checked (true)**

Esto garantiza que el cliente siempre recibe actualizaciones de la puerta, incluso cuando está en modo espectador.

- [ ] **Step 5: Compilar y guardar el BP**

En BP Editor: Compile → Save.

- [ ] **Step 6: Smoke test — colisión de puerta**

En editor: Play → 2 Players → Standalone.
- Probar que el personaje no puede atravesar la puerta en movimiento (en cliente y servidor)
- Morir como jugador B, que el jugador A haga mover la puerta, revivir a B → verificar que la puerta está en la posición correcta en la ventana de B

- [ ] **Step 7: Commit**

```bash
git commit -m "fix(Door): replicate movement + always relevant + server-only timeline (#B4 #B8)"
```

---

## Task 7: Limpieza — eliminar log de diagnóstico temporal

Una vez confirmado que el widget de resultados funciona correctamente (Task 2 smoke test pasado), eliminar los logs de diagnóstico verbose para no contaminar el Output Log en producción.

**Files:**
- Modify: `Source/Tortunabo/Private/UI/HUD/TN_CoopFlowHUDWidget.cpp`

- [ ] **Step 1: Eliminar los UE_LOG de diagnóstico de ShowResultsPanel**

En `ShowResultsPanel`, eliminar el bloque completo:
```cpp
UE_LOG(LogTemp, Warning, TEXT("[HUD] ShowResultsPanel called. ResultsOverlay=%s ...
```
Y el log de TNPS null:
```cpp
UE_LOG(LogTemp, Warning, TEXT("[HUD] ShowResultsPanel: TNPS is null ...
```

Mantener la lógica de null-check sobre `ResultsOverlay` (los `if (ResultsOverlay)`) — solo borrar los logs.

- [ ] **Step 2: Compilar**

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" TortunaboEditor Win64 Development "C:\Users\mokiu\Documents\Unreal Projects\Tortunabo\Tortunabo.uproject" -WaitMutex -NoHotReload
```

- [ ] **Step 3: Commit final**

```bash
git add Source/Tortunabo/Private/UI/HUD/TN_CoopFlowHUDWidget.cpp
git commit -m "chore(HUD): remove diagnostic logs from ShowResultsPanel"
```

---

## Comando de compilación (referencia rápida)

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" TortunaboEditor Win64 Development "C:\Users\mokiu\Documents\Unreal Projects\Tortunabo\Tortunabo.uproject" -WaitMutex -NoHotReload
```

**CRÍTICO**: Nunca usar Live Coding ni Hot Reload para tests multijugador → `NetChecksumMismatch`.

---

## Self-Review

### Spec coverage
- [x] #B3 — chunks destruidos al morir → Tasks 3 + 4
- [x] #B4 — puerta atravesable → Task 6
- [x] #B5 — pelota inmóvil en cliente → Task 5
- [x] #B6 — widget resultados no aparece → Tasks 1 + 2
- [x] #B7 — victoria no aparece tras muerte+revive → Tasks 1 + 2 (mismo root cause + delegate binding)
- [x] #B8 — puerta incorrecta tras muerte → Task 6 (bAlwaysRelevant)

### Gaps identificados
- **#B4/#B5**: Si los BPs de puerta y pelota tienen clase C++ propia no encontrada en el análisis, puede que `bReplicates` ya esté en el constructor. En ese caso el fix de Task 5/6 sigue siendo válido (los valores de BP override al constructor en `bReplicateMovement`).
- **#B3 edge case**: Si hay múltiples ChunkManagers (no debería haberlos), el TActorIterator devuelve el primero. OK para el diseño actual.
- **#B7 específico**: Si `ClientRestart` recrea el HUD (destruye y crea el widget), el nuevo widget empieza con `bFlowStateInitialized = false` y detectará el estado `Results` en el primer tick. El delegate binding de Task 2 cubre esto si hay un race condition con el tick.

### Placeholder scan
Ningún placeholder encontrado. Todos los steps tienen código concreto o instrucciones de editor específicas.

### Type consistency
- `GetSafeReviveLocation()` declarado en Task 3 Step 1, implementado en Step 2, usado en Task 4 Step 2. Consistente.
- `OnMatchFlowStateChangedHandler` declarado en Task 2 Step 5, implementado en Step 6, bindeado en Step 7. Consistente.
- `BoundFlowGameState` declarado en Task 2 Step 5, usado en Steps 6+7. Consistente.

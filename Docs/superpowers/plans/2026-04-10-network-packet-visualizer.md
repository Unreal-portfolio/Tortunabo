# Network Packet Visualizer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build an in-game toggleable HUD overlay that displays live network stats (ping, bandwidth in/out KB/s, packets/s in/out) — the standard "net stats widget" used by multiplayer studios during internal testing.

**Architecture:** A `UTN_NetStatsWidget` (UUserWidget subclass) reads from `UNetDriver` and `APlayerState` every 0.5 s via a timer. A Blueprint subclass `WBP_NetStats` exposes one `UTextBlock` bound to C++. The existing `ATortugaCharacter` gets a console-accessible toggle to add/remove the widget from the viewport. The widget is only created for locally controlled players and only in non-shipping builds (controlled by a compile guard).

**Tech Stack:** UE 5.6 C++, `UNetDriver`, `UUserWidget`, `FTimerManager`, Enhanced Input (existing), `UKismetSystemLibrary` for console variables.

---

## Industry Standard Context

The standard in multiplayer game development (Respawn, Epic, Riot) is an in-game overlay reading driver-level stats — equivalent to UE's `stat net` console command but always accessible in builds. This overlay shows:
- **RTT/Ping** (ms) — from `APlayerState::ExactPing`
- **Bandwidth In/Out** (KB/s) — `UNetDriver::InBytesPerSecond / 1024`
- **Packets In/Out per second** — `UNetDriver::InPacketsPerSecond / OutPacketsPerSecond`
- **Saturated** flag — `UNetDriver::bIsSaturated` (true = outgoing queue full, causing lag)

The `stat net` console command in UE provides the same data in Editor. This widget makes it accessible in standalone and networked play.

---

## File Map

| Action | File |
|--------|------|
| Create | `Source/Tortunabo/Public/UI/TN_NetStatsWidget.h` |
| Create | `Source/Tortunabo/Private/UI/TN_NetStatsWidget.cpp` |
| Modify | `Source/Tortunabo/Private/Player/TortugaCharacter.cpp` |
| Modify | `Source/Tortunabo/Public/Player/TortugaCharacter.h` |
| Create (Editor) | `Content/Blueprints/UI/WBP_NetStats.uasset` — Blueprint of `UTN_NetStatsWidget` |

---

### Task 1: Create UTN_NetStatsWidget header

**Files:**
- Create: `Source/Tortunabo/Public/UI/TN_NetStatsWidget.h`

- [ ] **Step 1: Write the header file**

```cpp
// Source/Tortunabo/Public/UI/TN_NetStatsWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TN_NetStatsWidget.generated.h"

class UTextBlock;

/**
 * In-game network stats overlay. Reads UNetDriver every RefreshInterval seconds.
 * Displayed stats: Ping (ms), Bandwidth In/Out (KB/s), Packets/s In/Out.
 *
 * Industry standard: mirrors "stat net" console command as a persistent HUD element.
 *
 * Usage:
 *   1. Create a Blueprint subclass WBP_NetStats of this class.
 *   2. Add a TextBlock named exactly "NetStatsText" to the widget hierarchy.
 *   3. Call ToggleNetStatsOverlay() on TortugaCharacter (F3 key by default).
 */
UCLASS()
class TORTUNABO_API UTN_NetStatsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** How often (seconds) the stats text is refreshed. Default: 0.5s. */
	UPROPERTY(EditDefaultsOnly, Category = "Net Stats", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float RefreshInterval = 0.5f;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	/** The only widget required in the Blueprint. Name must be exactly "NetStatsText". */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NetStatsText;

	FTimerHandle RefreshTimerHandle;

	UFUNCTION()
	void RefreshStats();

	/** Returns the NetDriver for this widget's owning player's world, or nullptr. */
	UNetDriver* GetRelevantNetDriver() const;
};
```

- [ ] **Step 2: Commit**

```bash
git add "Source/Tortunabo/Public/UI/TN_NetStatsWidget.h"
git commit -m "feat(ui): add UTN_NetStatsWidget header for net stats overlay"
```

---

### Task 2: Implement UTN_NetStatsWidget

**Files:**
- Create: `Source/Tortunabo/Private/UI/TN_NetStatsWidget.cpp`

- [ ] **Step 1: Write the implementation**

```cpp
// Source/Tortunabo/Private/UI/TN_NetStatsWidget.cpp
#include "UI/TN_NetStatsWidget.h"

#include "Components/TextBlock.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"

void UTN_NetStatsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			RefreshTimerHandle,
			this,
			&UTN_NetStatsWidget::RefreshStats,
			RefreshInterval,
			true   // looping
		);
	}
}

void UTN_NetStatsWidget::NativeDestruct()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}
	Super::NativeDestruct();
}

void UTN_NetStatsWidget::RefreshStats()
{
	if (!NetStatsText)
	{
		return;
	}

	// ── Ping ─────────────────────────────────────────────────────────────────
	float PingMs = 0.f;
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (const APlayerState* PS = PC->GetPlayerState<APlayerState>())
		{
			PingMs = PS->ExactPing;
		}
	}

	// ── NetDriver stats ───────────────────────────────────────────────────────
	int32 InKBps = 0, OutKBps = 0;
	int32 InPPS = 0, OutPPS = 0;
	bool bSaturated = false;

	if (UNetDriver* ND = GetRelevantNetDriver())
	{
		InKBps  = ND->InBytesPerSecond  / 1024;
		OutKBps = ND->OutBytesPerSecond / 1024;
		InPPS   = ND->InPacketsPerSecond;
		OutPPS  = ND->OutPacketsPerSecond;
		bSaturated = ND->bIsSaturated;
	}

	// ── Format ───────────────────────────────────────────────────────────────
	// Layout mirrors the "stat net" console command output used during production testing.
	const FString Stats = FString::Printf(
		TEXT("=== NET STATS ===\n"
		     "Ping       %4.0f ms\n"
		     "In         %4d KB/s  (%d pkt/s)\n"
		     "Out        %4d KB/s  (%d pkt/s)\n"
		     "Saturated  %s"),
		PingMs,
		InKBps,  InPPS,
		OutKBps, OutPPS,
		bSaturated ? TEXT("YES ⚠") : TEXT("no")
	);

	NetStatsText->SetText(FText::FromString(Stats));
}

UNetDriver* UTN_NetStatsWidget::GetRelevantNetDriver() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	// GetNetDriver() returns the active driver (client net driver in clients, server in listen-server).
	return World->GetNetDriver();
}
```

- [ ] **Step 2: Verify it compiles**

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" TortunaboEditor Win64 Development "C:\Users\mokiu\Documents\Unreal Projects\Tortunabo\Tortunabo.uproject" -WaitMutex -NoHotReload
```
Expected: `Build successful`.

- [ ] **Step 3: Commit**

```bash
git add "Source/Tortunabo/Private/UI/TN_NetStatsWidget.cpp"
git commit -m "feat(ui): implement UTN_NetStatsWidget net stats refresh logic"
```

---

### Task 3: Add toggle to TortugaCharacter

**Files:**
- Modify: `Source/Tortunabo/Public/Player/TortugaCharacter.h`
- Modify: `Source/Tortunabo/Private/Player/TortugaCharacter.cpp`

- [ ] **Step 1: Add header declarations**

In `TortugaCharacter.h`, in the public section near `GrantInfiniteStamina` (around line 300), add:

```cpp
// ── Network Stats Overlay (debug) ─────────────────────────────────────────
/** Blueprint class to use for the net stats overlay. Assign WBP_NetStats in BP_TortugaCharacter. */
UPROPERTY(EditDefaultsOnly, Category = "Debug|NetStats")
TSoftClassPtr<UUserWidget> NetStatsWidgetClass;

/** Toggle the net stats HUD overlay on/off. Bind to F3 in IMC_Player. */
UFUNCTION(BlueprintCallable, Category = "Debug|NetStats")
void ToggleNetStatsOverlay();
```

In the `private:` section, add:

```cpp
UPROPERTY(Transient)
TObjectPtr<UUserWidget> NetStatsWidgetInstance;
```

- [ ] **Step 2: Include the widget header in TortugaCharacter.cpp**

At the top of `TortugaCharacter.cpp`, with the other includes (after line ~35), add:

```cpp
#include "Blueprint/UserWidget.h"
```

- [ ] **Step 3: Implement ToggleNetStatsOverlay()**

In `TortugaCharacter.cpp`, near the other debug helpers (`GrantInfiniteStamina`, line ~1193), add:

```cpp
void ATortugaCharacter::ToggleNetStatsOverlay()
{
	// Only the local player owns this widget — never create it for remote pawns.
	if (!IsLocallyControlled())
	{
		return;
	}

	// If widget is visible → remove it.
	if (NetStatsWidgetInstance && NetStatsWidgetInstance->IsInViewport())
	{
		NetStatsWidgetInstance->RemoveFromParent();
		NetStatsWidgetInstance = nullptr;
		return;
	}

	// Load the soft class reference and create the widget.
	TSubclassOf<UUserWidget> WidgetClass = NetStatsWidgetClass.LoadSynchronous();
	if (!WidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[NetStats] NetStatsWidgetClass not assigned in BP_TortugaCharacter."));
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	NetStatsWidgetInstance = CreateWidget<UUserWidget>(PC, WidgetClass);
	if (NetStatsWidgetInstance)
	{
		NetStatsWidgetInstance->AddToViewport(100); // ZOrder 100 → on top of gameplay HUD
	}
}
```

- [ ] **Step 4: Build**

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" TortunaboEditor Win64 Development "C:\Users\mokiu\Documents\Unreal Projects\Tortunabo\Tortunabo.uproject" -WaitMutex -NoHotReload
```
Expected: `Build successful`.

- [ ] **Step 5: Commit**

```bash
git add "Source/Tortunabo/Public/Player/TortugaCharacter.h" \
        "Source/Tortunabo/Private/Player/TortugaCharacter.cpp"
git commit -m "feat(ui): add ToggleNetStatsOverlay to TortugaCharacter"
```

---

### Task 4: Create WBP_NetStats in the Editor and wire the input

**Files:**
- Create (Editor): `Content/Blueprints/UI/WBP_NetStats.uasset`
- Modify (Editor): `Content/Blueprints/Gameplay/Controls/IMC_Player.uasset` — add F3 key
- Modify (Editor): `Content/Blueprints/Characters/BP_TortugaCharacter.uasset` — assign widget class

> This task is done inside the Unreal Editor, not in C++. Steps are manual.

- [ ] **Step 1: Create the Blueprint widget**

In the Content Browser, navigate to `Content/Blueprints/UI/`.
Right-click → `User Interface` → `Widget Blueprint` → choose parent class `TN_NetStatsWidget` → name it `WBP_NetStats`.

- [ ] **Step 2: Add the TextBlock**

Open `WBP_NetStats`. In the Designer panel:
1. Drag a `Text Block` widget into the canvas.
2. In the Details panel, set **Name** to exactly `NetStatsText` (this is the `BindWidget` name).
3. Set font: Monospace or `RobotoMono`, size 12.
4. Set foreground color: white with a semi-transparent black outline for readability.
5. Anchor to top-left. Set Position X=20, Y=20.

- [ ] **Step 3: Assign widget class in BP_TortugaCharacter**

Open `Content/Blueprints/Characters/BP_TortugaCharacter`.
In Class Defaults → `Debug|NetStats` → `Net Stats Widget Class` → assign `WBP_NetStats`.

- [ ] **Step 4: Create IA_ToggleNetStats input action**

In `Content/Blueprints/Gameplay/Controls/`, right-click → `Input` → `Input Action`. Name it `IA_ToggleNetStats`. Value type: `Digital (bool)`.

Open `IMC_Player`. Add a mapping: `IA_ToggleNetStats` → key `F3`.

- [ ] **Step 5: Bind the action in TortugaCharacter.cpp**

In `TortugaCharacter.cpp`, add a soft reference property in the header alongside the other actions:

In `TortugaCharacter.h`, in the Input UPROPERTY block (around line 85):
```cpp
UPROPERTY(EditDefaultsOnly, Category = "Input|Debug")
TSoftObjectPtr<UInputAction> ToggleNetStatsAction;
```

In `TortugaCharacter.cpp` `SetupPlayerInputComponent` (around line 750), add before the closing brace of the enhanced input block:
```cpp
// Soft-load toggle net stats action
if (UInputAction* LoadedToggleNetStats = ToggleNetStatsAction.LoadSynchronous())
{
    EnhancedInput->BindAction(LoadedToggleNetStats, ETriggerEvent::Started, this, &ATortugaCharacter::ToggleNetStatsOverlay);
}
```

In `BP_TortugaCharacter` Class Defaults → Input → `Toggle Net Stats Action` → assign `IA_ToggleNetStats`.

- [ ] **Step 6: Build after the input binding addition**

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" TortunaboEditor Win64 Development "C:\Users\mokiu\Documents\Unreal Projects\Tortunabo\Tortunabo.uproject" -WaitMutex -NoHotReload
```
Expected: `Build successful`.

- [ ] **Step 7: Commit code changes (Blueprint is binary)**

```bash
git add "Source/Tortunabo/Public/Player/TortugaCharacter.h" \
        "Source/Tortunabo/Private/Player/TortugaCharacter.cpp"
git commit -m "feat(input): add F3 binding for net stats overlay toggle"
```

---

### Task 5: Smoke test the overlay

**Files:** None — testing only.

- [ ] **Step 1: PIE with 2 players (New Editor Window)**

Set 2 players, start PIE.

- [ ] **Step 2: Press F3 on the client window**

Expected: a monospace overlay appears at the top-left:
```
=== NET STATS ===
Ping         4 ms
In           2 KB/s  (8 pkt/s)
Out          1 KB/s  (6 pkt/s)
Saturated    no
```

- [ ] **Step 3: Press F3 again**

Expected: overlay disappears.

- [ ] **Step 4: Verify stats change under movement**

Walk the character around (generates movement replication). Expected: `Out KB/s` increases. `Saturated` stays `no` in a 2-player local PIE.

- [ ] **Step 5: Verify the overlay doesn't appear on the listen-server window for remote pawns**

On the server window, press F3. Expected: overlay appears (it is also locally controlled there). On a non-possessed pawn, ToggleNetStatsOverlay is a no-op.

---

## Self-Review Checklist

- [x] Reads Ping from PlayerState::ExactPing ✅
- [x] Reads bandwidth from UNetDriver::InBytesPerSecond/OutBytesPerSecond ✅
- [x] Reads packet rate from UNetDriver::InPacketsPerSecond/OutPacketsPerSecond ✅
- [x] Reads saturation flag from UNetDriver::bIsSaturated ✅
- [x] Refreshes every 0.5 s via timer (not every tick) ✅
- [x] Timer cleared in NativeDestruct — no dangling timer ✅
- [x] Only created for locally controlled pawns ✅
- [x] Toggled by F3 (IA_ToggleNetStats) ✅
- [x] BindWidget name "NetStatsText" documented clearly ✅
- [x] ZOrder 100 → renders above gameplay HUD ✅

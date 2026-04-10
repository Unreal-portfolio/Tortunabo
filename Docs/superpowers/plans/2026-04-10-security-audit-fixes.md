# Security Audit & RPC Hardening Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix three Server RPC vulnerabilities in `TortugaCharacter` that allow knocked-down or dead clients to perform game-changing actions (use/throw items, drop items, spam revive searches).

**Architecture:** Add `bIsKnockedDown || bIsDead` early-return guards at the top of the vulnerable `_Implementation` functions. The guards are identical in structure to the existing guard in `ServerSetEmote_Implementation` (which already checks these flags correctly — use it as the reference pattern).

**Tech Stack:** Unreal Engine 5.6 C++. Changes are surgical — no new classes, no new files.

---

## Audit Summary

### Findings

| # | Severity | RPC | Issue | Line |
|---|----------|-----|-------|------|
| 1 | **High** | `ServerUseEquippedItem_Implementation` | No `bIsKnockedDown`/`bIsDead` guard. Knocked-down client can spam throwables. | 1046 |
| 2 | **High** | `ServerTryInteract` fallback path | When `CanInteract` fails on a pickup with full inventory, falls through to `ServerUseEquippedItem_Implementation()` directly — bypasses guard #1. | 1008 |
| 3 | **Medium** | `ServerDropEquippedItem_Implementation` | No state guard. Dead/knocked player can still drop items. | 1152 |
| 4 | **Low** | `ServerTryReviveNearby_Implementation` | `HasAuthority()` guard is redundant (Server RPCs always run on authority). No knockdown guard — knocked player can spam server-side world iteration. | 1680 |

### Non-Issues (already protected)

- `ServerSetEmote_Implementation` (line 2033): checks `bIsAlive`, `bIsDBNO`, `bIsKnockedDown`, has per-emote cooldown. **No action needed.**
- `ServerTryInteract_Implementation` (line 994): checks `CanInteract`, distance, ping compensation. **No action needed** (the fallback path is fixed in this plan as Finding #2).

---

## File Map

| Action | File |
|--------|------|
| Modify | `Source/Tortunabo/Private/Player/TortugaCharacter.cpp` |

---

### Task 1: Fix Finding #1 — ServerUseEquippedItem guard

**Files:**
- Modify: `Source/Tortunabo/Private/Player/TortugaCharacter.cpp:1046`

- [ ] **Step 1: Read the current implementation to confirm the exact text**

Open `TortugaCharacter.cpp` line 1046–1052. Confirm it reads:
```cpp
void ATortugaCharacter::ServerUseEquippedItem_Implementation()
{
	if (!InventoryComponent || !StaminaComponent)
	{
		return;
	}
```

- [ ] **Step 2: Add the state guard**

Replace the opening of `ServerUseEquippedItem_Implementation` with:
```cpp
void ATortugaCharacter::ServerUseEquippedItem_Implementation()
{
	if (bIsKnockedDown || bIsDead)
	{
		return;
	}
	if (!InventoryComponent || !StaminaComponent)
	{
		return;
	}
```

- [ ] **Step 3: Build**

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" TortunaboEditor Win64 Development "C:\Users\mokiu\Documents\Unreal Projects\Tortunabo\Tortunabo.uproject" -WaitMutex -NoHotReload
```
Expected: `Build successful`.

- [ ] **Step 4: Commit**

```bash
git add "Source/Tortunabo/Private/Player/TortugaCharacter.cpp"
git commit -m "fix(security): guard ServerUseEquippedItem against knocked-down/dead clients"
```

---

### Task 2: Fix Finding #2 — ServerTryInteract fallback path

**Files:**
- Modify: `Source/Tortunabo/Private/Player/TortugaCharacter.cpp:1004–1022`

- [ ] **Step 1: Read the fallback path to confirm exact text**

Open `TortugaCharacter.cpp` lines 1004–1022. Confirm the `CanInteract` failure block reads:
```cpp
	if (!Interactable->CanInteract(this))
	{
		// Si falla en un pickup Y tenemos ítem equipado → asumir "inventario lleno"
		// y usar/lanzar el ítem directamente, sin desperdiciar el input del jugador.
		if (Cast<ATN_PickupInteractableBase>(Interactable)
			&& InventoryComponent && InventoryComponent->HasEquippedItem())
		{
			if (bDebug)
			{
				UE_LOG(LogTemp, Log, TEXT("[Interact:SERVER] Pickup '%s' no recogible + inventario lleno → usando ítem equipado."),
					*Interactable->GetName());
			}
			ServerUseEquippedItem_Implementation();
		}
```

- [ ] **Step 2: Add the guard inside the fallback**

Replace only the inner block (the part that calls `ServerUseEquippedItem_Implementation()`):
```cpp
	if (!Interactable->CanInteract(this))
	{
		// Si falla en un pickup Y tenemos ítem equipado → asumir "inventario lleno"
		// y usar/lanzar el ítem directamente, sin desperdiciar el input del jugador.
		if (Cast<ATN_PickupInteractableBase>(Interactable)
			&& InventoryComponent && InventoryComponent->HasEquippedItem())
		{
			if (bIsKnockedDown || bIsDead)
			{
				return;
			}
			if (bDebug)
			{
				UE_LOG(LogTemp, Log, TEXT("[Interact:SERVER] Pickup '%s' no recogible + inventario lleno → usando ítem equipado."),
					*Interactable->GetName());
			}
			ServerUseEquippedItem_Implementation();
		}
```

- [ ] **Step 3: Build**

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" TortunaboEditor Win64 Development "C:\Users\mokiu\Documents\Unreal Projects\Tortunabo\Tortunabo.uproject" -WaitMutex -NoHotReload
```
Expected: `Build successful`.

- [ ] **Step 4: Commit**

```bash
git add "Source/Tortunabo/Private/Player/TortugaCharacter.cpp"
git commit -m "fix(security): guard interact fallback path against knocked-down/dead clients"
```

---

### Task 3: Fix Finding #3 — ServerDropEquippedItem guard

**Files:**
- Modify: `Source/Tortunabo/Private/Player/TortugaCharacter.cpp:1152`

- [ ] **Step 1: Read the current implementation**

Open line 1152–1155. Confirm it reads:
```cpp
void ATortugaCharacter::ServerDropEquippedItem_Implementation()
{
	if (!InventoryComponent) { return; }
```

- [ ] **Step 2: Add the state guard**

Replace:
```cpp
void ATortugaCharacter::ServerDropEquippedItem_Implementation()
{
	if (!InventoryComponent) { return; }
```
With:
```cpp
void ATortugaCharacter::ServerDropEquippedItem_Implementation()
{
	if (bIsKnockedDown || bIsDead) { return; }
	if (!InventoryComponent) { return; }
```

- [ ] **Step 3: Build**

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" TortunaboEditor Win64 Development "C:\Users\mokiu\Documents\Unreal Projects\Tortunabo\Tortunabo.uproject" -WaitMutex -NoHotReload
```
Expected: `Build successful`.

- [ ] **Step 4: Commit**

```bash
git add "Source/Tortunabo/Private/Player/TortugaCharacter.cpp"
git commit -m "fix(security): guard ServerDropEquippedItem against knocked-down/dead clients"
```

---

### Task 4: Fix Finding #4 — ServerTryReviveNearby cleanup

**Files:**
- Modify: `Source/Tortunabo/Private/Player/TortugaCharacter.cpp:1680`

- [ ] **Step 1: Read the current implementation**

Open lines 1680–1685. Confirm it reads:
```cpp
void ATortugaCharacter::ServerTryReviveNearby_Implementation()
{
	if (!HasAuthority()) { return; }

	// Solo busca jugadores en DBNO (knockdown). Los muertos se reviven vía TN_RescuePickup.
	const float ReviveSearchRadius = ReviveRadiusCm > 0.f ? ReviveRadiusCm : 300.f;
```

- [ ] **Step 2: Replace the redundant HasAuthority guard + add state guard**

`HasAuthority()` is always `true` in a `Server` RPC — remove the redundant check and replace with the meaningful guard:

```cpp
void ATortugaCharacter::ServerTryReviveNearby_Implementation()
{
	if (bIsKnockedDown || bIsDead) { return; }

	// Solo busca jugadores en DBNO (knockdown). Los muertos se reviven vía TN_RescuePickup.
	const float ReviveSearchRadius = ReviveRadiusCm > 0.f ? ReviveRadiusCm : 300.f;
```

- [ ] **Step 3: Build**

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" TortunaboEditor Win64 Development "C:\Users\mokiu\Documents\Unreal Projects\Tortunabo\Tortunabo.uproject" -WaitMutex -NoHotReload
```
Expected: `Build successful`.

- [ ] **Step 4: Commit**

```bash
git add "Source/Tortunabo/Private/Player/TortugaCharacter.cpp"
git commit -m "fix(security): replace redundant HasAuthority with state guard in ServerTryReviveNearby"
```

---

### Task 5: Smoke test all four fixes in PIE

**Files:** None — testing only.

> All tests require 2-player PIE (New Editor Window).

- [ ] **Step 1: Test — knocked-down player cannot throw items**

1. Start PIE, 2 players.
2. Trigger knockdown on Player 1 (walk into a puffer-fish, or use debug: open console on server window → `DebugKnockdown 1`). Player 1 falls over.
3. With Player 1 knocked down, press Interact (E). Expected: **nothing happens** — no throwable spawned, no item used.

- [ ] **Step 2: Test — knocked-down player cannot use interact fallback**

1. Player 1 (knocked down) has a throwable item equipped. Walk near a pickup.
2. Press E while knocked down. Expected: **nothing** — neither the pickup is taken nor the throwable is thrown.

- [ ] **Step 3: Test — dead player cannot drop items**

1. Mark Player 1 as dead (walk into a death zone with bIsDead set, or enter console: `MarkPlayerDead`).
2. Press Drop Item (Q). Expected: **no item drop**, item stays in world.

- [ ] **Step 4: Test — knocked-down player cannot revive others**

1. Player 1 is knocked down. Player 2 is also knocked down.
2. Player 1 presses Interact near Player 2. Expected: **no revive triggered** — server returns early.

- [ ] **Step 5: Verify normal players are unaffected**

1. Player 1 in normal state (alive, standing).
2. Interact, use item, drop item, revive — all work as before.
Expected: all normal gameplay functions correctly.

---

## Self-Review Checklist

- [x] Finding #1 (ServerUseEquippedItem) — guard added at function entry ✅
- [x] Finding #2 (ServerTryInteract fallback) — guard added before calling UseEquippedItem ✅
- [x] Finding #3 (ServerDropEquippedItem) — guard added at function entry ✅
- [x] Finding #4 (ServerTryReviveNearby) — redundant HasAuthority removed, real guard added ✅
- [x] Guard pattern matches existing ServerSetEmote pattern (consistent style) ✅
- [x] No other Server RPCs found without appropriate guards ✅
- [x] Each fix has its own commit for easy revert if needed ✅

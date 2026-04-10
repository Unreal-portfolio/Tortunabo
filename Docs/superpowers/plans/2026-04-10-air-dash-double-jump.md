# Air Dash Double Jump Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** After the first jump (while airborne), pressing Jump again performs a Fall Guys-style forward dash — a horizontal impulse in the actor's facing direction plus a small vertical boost — instead of a standard second jump.

**Architecture:** Override `ATortugaCharacter::Jump()` to detect the airborne state and redirect to an air-dash path. Use a boolean flag `bCanAirDash` (reset on `Landed()`) to allow exactly one dash per jump. The client predicts immediately via `LaunchCharacter`; the server validates and replicates the authoritative position via a `Server, Reliable` RPC. CMC's built-in prediction reconciliation handles client correction.

**Tech Stack:** Unreal Engine 5.6 C++, `UCharacterMovementComponent::LaunchCharacter`, Enhanced Input (existing binding), Server RPCs.

---

## File Map

| Action | File |
|--------|------|
| Modify | `Source/Tortunabo/Public/Player/TortugaCharacter.h` |
| Modify | `Source/Tortunabo/Private/Player/TortugaCharacter.cpp` |

---

### Task 1: Add properties and declarations to the header

**Files:**
- Modify: `Source/Tortunabo/Public/Player/TortugaCharacter.h`

- [ ] **Step 1: Add EditDefaultsOnly tuning properties**

In `TortugaCharacter.h`, locate the block with stamina/movement tuning properties (around line 140–200). Add after the leg animation block:

```cpp
// ── Air Dash (double-jump) ────────────────────────────────────────────────
/** Horizontal velocity applied on air dash (cm/s). Overrides current XY velocity. */
UPROPERTY(EditDefaultsOnly, Category = "Movement|AirDash", meta = (ClampMin = "0.0"))
float AirDashHorizontalForce = 1400.f;

/** Vertical velocity applied on air dash (cm/s). Overrides current Z velocity. */
UPROPERTY(EditDefaultsOnly, Category = "Movement|AirDash", meta = (ClampMin = "0.0"))
float AirDashVerticalBoost = 300.f;
```

- [ ] **Step 2: Add private flag and method declarations**

In `TortugaCharacter.h`, inside the `private:` section near the other movement helpers (`Move`, `Look`, `StartSprint` — around line 384), add:

```cpp
// ── Air Dash internals ────────────────────────────────────────────────────
/** True after landing; false after the first air dash of a jump. Not replicated — tracked per-machine. */
bool bCanAirDash = true;

virtual void Jump() override;
virtual void Landed(const FHitResult& Hit) override;
void PerformAirDashLocally();

UFUNCTION(Server, Reliable)
void ServerPerformAirDash(FVector ClientForwardDirection);
```

- [ ] **Step 3: Verify the header compiles**

Open a terminal at the project root. Run:
```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" TortunaboEditor Win64 Development "C:\Users\mokiu\Documents\Unreal Projects\Tortunabo\Tortunabo.uproject" -WaitMutex -NoHotReload
```
Expected: `Build successful` (or only pre-existing warnings — no new errors).

- [ ] **Step 4: Commit**

```bash
git add "Source/Tortunabo/Public/Player/TortugaCharacter.h"
git commit -m "feat(jump): declare air-dash properties and RPC in TortugaCharacter"
```

---

### Task 2: Implement the air dash logic in TortugaCharacter.cpp

**Files:**
- Modify: `Source/Tortunabo/Private/Player/TortugaCharacter.cpp`

- [ ] **Step 1: Override Jump() — detect in-air and redirect to dash**

In `TortugaCharacter.cpp`, find the `Move` function (line ~780). Add **before** it (after the input binding block):

```cpp
void ATortugaCharacter::Jump()
{
	// While airborne and dash is available → perform the air dash instead of a second jump.
	if (GetCharacterMovement()->IsFalling() && bCanAirDash && !bIsKnockedDown && !bIsDead)
	{
		PerformAirDashLocally();
		// Tell the server to validate and apply the dash authoritatively.
		if (!HasAuthority())
		{
			ServerPerformAirDash(GetActorForwardVector());
		}
		return;
	}
	Super::Jump();
}
```

- [ ] **Step 2: Implement PerformAirDashLocally()**

Add immediately after `Jump()`:

```cpp
void ATortugaCharacter::PerformAirDashLocally()
{
	bCanAirDash = false;
	const FVector DashVelocity = GetActorForwardVector() * AirDashHorizontalForce
	                           + FVector::UpVector * AirDashVerticalBoost;
	// true, true: override current XY and Z velocity entirely (Fall Guys feel).
	LaunchCharacter(DashVelocity, true, true);
}
```

- [ ] **Step 3: Implement ServerPerformAirDash_Implementation()**

Add after `PerformAirDashLocally()`:

```cpp
void ATortugaCharacter::ServerPerformAirDash_Implementation(FVector ClientForwardDirection)
{
	// Guard: only apply if the server state agrees the dash is valid.
	if (!GetCharacterMovement()->IsFalling() || !bCanAirDash || bIsKnockedDown || bIsDead)
	{
		return;
	}
	// Sanitise the direction the client sent (prevent magnitude exploits).
	const FVector SafeDir = ClientForwardDirection.GetSafeNormal();
	if (SafeDir.IsNearlyZero())
	{
		return;
	}
	bCanAirDash = false;
	const FVector DashVelocity = SafeDir * AirDashHorizontalForce
	                           + FVector::UpVector * AirDashVerticalBoost;
	LaunchCharacter(DashVelocity, true, true);
}
```

- [ ] **Step 4: Override Landed() to reset the dash flag**

Locate `ATortugaCharacter::GetLifetimeReplicatedProps` (line ~1357). Add before it:

```cpp
void ATortugaCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	bCanAirDash = true;
}
```

- [ ] **Step 5: Build and verify no errors**

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" TortunaboEditor Win64 Development "C:\Users\mokiu\Documents\Unreal Projects\Tortunabo\Tortunabo.uproject" -WaitMutex -NoHotReload
```
Expected: `Build successful`.

- [ ] **Step 6: Commit**

```bash
git add "Source/Tortunabo/Private/Player/TortugaCharacter.cpp"
git commit -m "feat(jump): implement air-dash double jump (Fall Guys style)"
```

---

### Task 3: Smoke test the mechanic in PIE

**Files:** None — testing only.

- [ ] **Step 1: Open the editor, set 2 players (New Editor Window)**

In `Play` dropdown → `Number of Players: 2` → `New Editor Window`.

- [ ] **Step 2: Verify single jump still works**

Press Space (or controller A). Character should jump normally. Expected: one jump, lands.

- [ ] **Step 3: Verify air dash works**

Jump, then press Space again while airborne. Expected: character dashes forward (horizontal burst + small upward pop, similar to Fall Guys bean dash). `bCanAirDash` becomes false.

- [ ] **Step 4: Verify one dash per jump**

While in air after the dash, pressing Space again should do nothing (no third jump or second dash). Expected: no additional movement change.

- [ ] **Step 5: Verify reset on landing**

After landing from a dash, jump again and press Space in air. Expected: dash is available again.

- [ ] **Step 6: Verify no dash while knocked down**

Use the debug console: `ServerTravelCheat bIsKnockedDown 1` (or trigger knockdown in game). While knocked down, pressing Space in air should not dash. Expected: no dash, no console errors.

- [ ] **Step 7: Tune values in BP_TortugaCharacter if needed**

In `Content/Blueprints/Characters/BP_TortugaCharacter`, Class Defaults → Movement|AirDash:
- `AirDashHorizontalForce`: start at `1400` (cm/s). Lower if too far, raise if too weak.
- `AirDashVerticalBoost`: start at `300` (cm/s). Increase for more hang time, decrease to keep it flat.

- [ ] **Step 8: Commit tuned values (if changed)**

```bash
# Note: Blueprint changes are binary — just record the values used in the commit message.
git add "Content/Blueprints/Characters/BP_TortugaCharacter.uasset"
git commit -m "feat(jump): tune air-dash force values (H=1400, V=300)"
```

---

## Self-Review Checklist

- [x] Air dash triggers on second jump press while airborne — Task 2 Step 1 ✅
- [x] Dash uses actor forward direction (not camera) — `GetActorForwardVector()` in Jump() ✅
- [x] Server validates before applying — ServerPerformAirDash_Implementation ✅
- [x] Only one dash per airborne phase — `bCanAirDash` flag reset in Landed() ✅
- [x] Knocked-down and dead guards — checked in both Jump() and Server RPC ✅
- [x] Tunable in Blueprint without recompile — EditDefaultsOnly UPROPERTY ✅
- [x] Build step included in every task ✅

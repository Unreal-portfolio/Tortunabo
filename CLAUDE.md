# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Tortunabo** is a cooperative multiplayer party game (1–4 players) built on **Unreal Engine 5.6** using C++. Players control anthropomorphic turtles racing through levels. Network transport is **Steam Sockets** (SteamDevAppId=480 for testing). Full architecture details are in `AGENTS.md`; level/setup checklists are in `README.md` and `GUIA_MONTAJE_INICIAL.md`.

## Build Commands

**Compile the editor target** (run from project root after closing the editor):
```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" TortunaboEditor Win64 Development "C:\Users\mokiu\Documents\Unreal Projects\Tortunabo\Tortunabo.uproject" -WaitMutex -NoHotReload
```

**Regenerate project files**: right-click `Tortunabo.uproject` → *Generate Visual Studio project files*.

**Critical**: Never use Live Coding or Hot Reload during multiplayer tests — it causes `NetChecksumMismatch`. Always compile with `-NoHotReload`.

## Source Structure

```
Source/Tortunabo/
├── Core/       — replicated match state & shared types (TN_CoopGameState, TN_CoopPlayerState, TN_MatchFlowTypes)
├── Game/       — run/race rules (TN_RunGameMode)
├── Lobby/      — HQ lobby logic (TN_HQGameMode, TN_LobbyReadyZone)
├── Menu/       — main menu (MP_MenuGameMode, MP_MenuPlayerController)
├── Multiplayer/— Steam sessions, save game, cosmetics persistence (MP_GameInstance, TN_CosmeticSaveGame)
├── Player/     — character, controller, components (TortugaCharacter, TN_StaminaComponent, TN_InventoryComponent)
├── Voice/      — proximity VOIP (ProximityVoiceComponent)
├── World/      — interactables, volumes, throwables
└── UI/         — HUD widgets, menus, radial wheels
```

All C++ classes are in a single module (`Tortunabo`). Blueprints live under `/Game/Blueprints/` in the Content directory.

## Game Flow

`Menu → HQ Lobby → Countdown → Cinematic → Run → Finish/Spectate → Results → HQ`

Map transitions use **Seamless Travel** (`bUseSeamlessTravel = true`) — the Steam NetDriver stays alive, clients never disconnect. Pawns are destroyed before `ServerTravel` (WASAPI cleanup); PlayerControllers and PlayerStates persist. `PostLogin` is NOT called for traveling players — use `HandleSeamlessTravelPlayer` / `PostSeamlessTravel` instead.

Each map **must** have its BP GameMode set in WorldSettings:
- `LVL_Menu` → `BP_MenuGameMode`
- `LVL_HQ` → `BP_HQGameMode`
- `LVL_Run` → `BP_RunGameMode`

## Critical Networking Patterns

- **OnRep does not fire on the listen-server** (the machine owning the variable). Game modes call `BroadcastFlowStateChange()` after changing `MatchFlowState` so the listen-server receives the notification via the `OnMatchFlowStateChanged` delegate.
- Use `ReplicatedUsing = OnRep_*` for property change callbacks; `UFUNCTION(Server, Reliable)` for server-validated actions; `NetMulticast, Reliable` for cosmetic-but-critical visuals (knockdown).
- `TN_CoopGameState` is the single source of truth for all replicated match state.
- `MP_GameInstance::PendingTravelPlayerCount` persists between maps — `TN_HQGameMode` writes it, `TN_RunGameMode` reads it in `BeginPlay` to know how many players to wait for.

## Key Systems

### Input
Enhanced Input assets live in `/Game/Blueprints/Gameplay/Controls/` (soft references in `TortugaCharacter`). Required assets: `IMC_Player`, `IA_Move`, `IA_Look`, `IA_Jump`, `IA_Interact`, `IA_RotateInventory`, `IA_Sprint`, `IA_DropItem`, `IA_Emote`–`IA_Emote9`. Missing assets log a warning and silently disable input.

### Stamina (`TN_StaminaComponent`)
MaxStamina=200, SprintDrain=45/s, RechargeDelay=0.8s. Exhaustion penalty=1s lockout before recharge. Weight reduces effective max stamina (`StaminaPerWeightUnit=20` per weight unit). `CurrentStamina` replicates owner-only; `bIsSprinting` replicates to all.

### Inventory (`TN_InventoryComponent`)
Two-slot system (equipped + stored). Item weight affects effective max stamina. `StaminaComponent->SetInventoryComponent(...)` must be called in `TortugaCharacter::BeginPlay`.

### Knockdown
`bIsKnockedDown` replicates with `OnRep_IsKnockedDown`. `ApplyKnockdownVisual` uses `SetRelativeRotation` (not world) and disables `NetworkSmoothingMode` while knocked down to prevent CMC overwriting the 180° tilt. `MulticastApplyKnockdownVisual` (Reliable) ensures all clients receive it immediately. `KnockdownComponentName` (default `"Cuerpo"`) must match the BP component name.

### Cosmetics
Saved in `TN_CosmeticSaveGame` via `MP_GameInstance`. `TN_CoopPlayerState` replicates `EquippedHelmetId`; `ATortugaCharacter::OnRep_PlayerState()` re-applies the helmet to cover the race condition where PlayerState arrives before pawn possession.

### Camera
Over-the-shoulder spring arm with position/rotation lag, dynamic zoom on sprint, interpolated FOV. All settings are `EditDefaultsOnly + BlueprintReadWrite` — adjust in `BP_TortugaCharacter` without recompiling.

### Lobby Countdown
Countdown starts when **all currently connected players** are in the ready zone (not when `LobbyExpectedPlayers` is reached). `TickCountdown` resets if anyone leaves the zone.

## Auxiliary Documentation

- `AGENTS.md` — detailed architecture reference (networking, systems, replication quirks)
- `Docs/GDD_Tortunabo.md` — full game design document
- `Docs/GUIA_PANTALLA_RESULTADOS.md` — results screen widget hierarchy guide
- `Docs/AgentSync/SESSION_LOG.md` — session history; record significant changes here

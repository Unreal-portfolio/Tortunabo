# SESSION LOG

## 2026-03-17 - Sprint 1 Kickoff

### Contexto
- Inicio de implementacion de base modular para interaccion + inventario 1+1.

### Cambios clave
- Base de interactuables C++:
  - `Source/Tortunabo/Public/World/TN_InteractableBase.h`
  - `Source/Tortunabo/Private/World/TN_InteractableBase.cpp`
- Hijas de interactuables:
  - `Source/Tortunabo/Public/World/TN_DirectInteractableBase.h`
  - `Source/Tortunabo/Private/World/TN_DirectInteractableBase.cpp`
  - `Source/Tortunabo/Public/World/TN_PickupInteractableBase.h`
  - `Source/Tortunabo/Private/World/TN_PickupInteractableBase.cpp`
- Inventario 1 equipado + 1 almacenado:
  - `Source/Tortunabo/Public/Core/TN_InventoryTypes.h`
  - `Source/Tortunabo/Public/Player/TN_InventoryComponent.h`
  - `Source/Tortunabo/Private/Player/TN_InventoryComponent.cpp`
- Integracion de input de interaccion y rotacion en personaje:
  - `Source/Tortunabo/Public/Player/TortugaCharacter.h`
  - `Source/Tortunabo/Private/Player/TortugaCharacter.cpp`
- Nuevo personaje base primera persona:
  - `Source/Tortunabo/Public/Player/TortugaFirstPersonCharacter.h`
  - `Source/Tortunabo/Private/Player/TortugaFirstPersonCharacter.cpp`
- Prompt 3D reusable:
  - `Source/Tortunabo/Public/UI/HUD/TN_InteractPromptWidget.h`
  - `Source/Tortunabo/Private/UI/HUD/TN_InteractPromptWidget.cpp`

### Networking y rendimiento
- Interaccion server-authoritative (`ServerTryInteract`) para evitar desincronizacion.
- Pickups replican estado compacto (`bTaken`) en vez de fisica replicada continua.
- Escaneo de interactuable en timer local (0.1s), no tick por frame.

### Pendientes inmediatos
1. Crear assets Enhanced Input: `IA_Interact`, `IA_RotateInventory`.
2. Crear BP widgets para prompt 3D y validar UX en mapa.
3. Probar end-to-end con 2 clientes (host + join).


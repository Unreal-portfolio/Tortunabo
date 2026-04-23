# Sistema de Skins — Tortunabo

> **Estado:** Diseño listo para implementar. Pendiente confirmar slot indices del nuevo SKM en editor.
> Creado: 2026-04-24 | Contexto: migración de multi-mesh components a Skeletal Mesh único.

---

## Contexto — Por qué cambió todo

### Sistema anterior (deprecado)
La tortuga usaba múltiples `UStaticMeshComponent` / `USkeletalMeshComponent` individuales:
- `Cuerpo` / `Body` → caparazón
- `Body1` → panza/vientre
- `Mesh1`–`Mesh5`, `Mesh13` → extremidades y detalles de piel
- `Pata1`, `Pata2`, `Cola`, `Cabeza` → partes separadas

`UpdateSkinVisual` iteraba sobre estos componentes y llamaba `SetMaterial()` en cada uno.
`DefaultBodyMaterials` era un `TMap<TWeakObjectPtr<UStaticMeshComponent>, TArray<UMaterialInterface*>>` que cacheaba los materiales por defecto de cada componente para restaurarlos.

### Sistema nuevo (a implementar)
Un único `USkeletalMeshComponent` (el SM de la tortuga) con varios **material slots** internos.
`UpdateSkinVisual` debe aplicar materiales por **índice de slot** en lugar de por nombre de componente.

**Cascos:** funcionan bien. Se adjuntan a un socket separado (`Sombrero`). No requieren cambios.

---

## Infraestructura existente (no tocar)

### `FTN_SkinData` — struct en `TN_CosmeticsTypes.h`
Ya tiene exactamente 3 materiales que mapean 1:1 a las 3 "zonas" del personaje:

| Campo | Zona original | Slot SKM propuesto |
|---|---|---|
| `BodyMaterial` | `Body` / caparazón | Slot 0 |
| `BellyMaterial` | `Body1` / panza | Slot 1 |
| `SkinMaterial` | `Mesh1-5`, `Mesh13` / extremidades | Slot 2 |

> **Pendiente verificar en editor:** abrir el nuevo SKM en UE5 → Details → Material Slots → confirmar que los índices 0/1/2 corresponden a caparazón/panza/piel. Si el orden es diferente, ajustar la tabla de arriba.

### `DT_Skins` — DataTable en Content
No requiere ningún cambio. Las filas existentes ya tienen `BodyMaterial`, `BellyMaterial`, `SkinMaterial` rellenos.

### Replicación
`EquippedSkinId` (tipo `FName`) ya replica en `TN_CoopPlayerState`. La cadena de apply es:
1. `OnRep_EquippedSkinId` en `TN_CoopPlayerState` → llama `ATortugaCharacter::UpdateSkinVisual`
2. `PostSeamlessTravel` en GameModes → retry con timer (patrón existente de helmets)
3. `OnRep_PlayerState` en `TortugaCharacter` → re-aplica skin (igual que helmet)

---

## Cambios necesarios en C++

### 1. `TortugaCharacter.h` — cambiar DefaultBodyMaterials

**Antes:**
```cpp
TMap<TWeakObjectPtr<UStaticMeshComponent>, TArray<TObjectPtr<UMaterialInterface>>> DefaultBodyMaterials;
```

**Después:**
```cpp
// Materiales por defecto del SKM por slot (cacheados en BeginPlay).
TArray<TObjectPtr<UMaterialInterface>> DefaultSkelMeshMaterials;
```

### 2. `TortugaCharacter.cpp::BeginPlay` — cachear materiales por defecto del SKM

**Antes:** iteraba sobre StaticMeshComponents del body y guardaba en el TMap.

**Después:**
```cpp
// En BeginPlay, donde antes se cacheaban los SMC materials:
USkeletalMeshComponent* SKM = GetMesh();
if (SKM)
{
    DefaultSkelMeshMaterials.Reset();
    for (int32 i = 0; i < SKM->GetNumMaterials(); i++)
    {
        DefaultSkelMeshMaterials.Add(SKM->GetMaterial(i));
    }
}
```

### 3. `TortugaCharacter.cpp::UpdateSkinVisual` — nueva implementación

```cpp
void ATortugaCharacter::UpdateSkinVisual(FName SkinId)
{
    USkeletalMeshComponent* SKM = GetMesh();
    if (!SKM) { return; }

    if (SkinId == NAME_None)
    {
        // Restaurar materiales por defecto
        for (int32 i = 0; i < DefaultSkelMeshMaterials.Num(); i++)
        {
            SKM->SetMaterial(i, DefaultSkelMeshMaterials[i]);
        }
        return;
    }

    // Buscar skin en DataTable vía GameInstance
    UMP_GameInstance* GI = Cast<UMP_GameInstance>(GetGameInstance());
    if (!GI) { return; }
    UDataTable* DT = GI->GetSkinDataTable();
    if (!DT) { return; }

    const FTN_SkinData* Data = DT->FindRow<FTN_SkinData>(SkinId, TEXT("UpdateSkinVisual"));
    if (!Data || !Data->IsValid()) { return; }

    // Aplicar por slot. Fallback: si BellyMaterial es null, usa BodyMaterial.
    if (Data->BodyMaterial)  { SKM->SetMaterial(0, Data->BodyMaterial); }
    if (Data->BellyMaterial) { SKM->SetMaterial(1, Data->BellyMaterial); }
    else if (Data->BodyMaterial) { SKM->SetMaterial(1, Data->BodyMaterial); }
    if (Data->SkinMaterial)  { SKM->SetMaterial(2, Data->SkinMaterial); }
}
```

> Si el SKM tiene más de 3 slots (ojos, detalles extra), los slots 3+ no se tocan → mantienen el material por defecto. Añadir campos a `FTN_SkinData` en el futuro si se necesitan.

---

## Checklist de implementación

- [ ] Verificar slot indices del nuevo SKM en editor (0=caparazón, 1=panza, 2=piel)
- [ ] Reemplazar `DefaultBodyMaterials` → `DefaultSkelMeshMaterials` en `.h` y `.cpp`
- [ ] Reescribir `BeginPlay` → cachear materiales del SKM por slot
- [ ] Reescribir `UpdateSkinVisual` → usar slots del SKM
- [ ] Añadir call a `UpdateSkinVisual` en `OnRep_PlayerState` (igual que el helmet retry)
- [ ] Verificar que `PostSeamlessTravel` de ambos GameModes incluye retry de skin (actualmente solo incluye helmet)
- [ ] Test en PIE 2P: cambiar skin → viaja al run → skin se mantiene

---

## Lo que NO cambia

- `FTN_SkinData` struct — no tocar
- `DT_Skins` DataTable — no tocar
- `EquippedSkinId` replicación — no tocar
- Sistema de helmets (socket `Sombrero`) — no tocar
- `MP_GameInstance::GetSkinDataTable()` — no tocar

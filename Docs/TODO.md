# TODO — Tortunabo

Notas técnicas pendientes que no son bugs sino mejoras de fondo / deuda
acumulada. Cuando algo de aquí se convierte en tarea concreta, pasa a
`QA_TESTING.md` o a un plan en `Docs/superpowers/plans/`.

---

## Pendientes

### 🟠 Protagonista: migrar a Skeletal Mesh
El `BP_TortugaCharacter` se compone hoy de un conjunto de primitivas /
Static Meshes ensamblados a mano. Hay que sustituirlo por un **Skeletal
Mesh** real con skeleton + physics asset propios.

**Por qué importa:**
- El ragdoll de knockdown (`bUsePhysicsRagdoll` en `ATortugaCharacter`)
  solo se activa si el mesh tiene un `PhysicsAsset` asignado — que requiere
  skeletal mesh.
- Animaciones (sprint, emotes, resbalón del plátano) saldrían de un
  AnimBlueprint, no de hack visual (tilt 180°, inclinación manual, etc.).
- El `KnockdownComponentName = "Cuerpo"` es un workaround sobre primitivas
  que se evapora cuando la cabeza, brazos y caparazón están unidos en un
  SkelMesh con bones.

**Qué cambiaría al hacerlo:**
- `GetMesh()` devolvería el USkeletalMeshComponent real.
- Route A (ragdoll físico) de `ApplyKnockdownVisual` activa automáticamente.
- Cosmética (`EquippedHelmetId`, skins) se attachea a sockets del skeleton.
- Se puede retirar el código de inclinación manual del knockdown tieso.

---

*TODO · Tortunabo · Última actualización: 2026-04-18*

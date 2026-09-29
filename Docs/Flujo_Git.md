# Flujo de trabajo con git (ramas, pull requests y main)

Desde el 29-09-2026 `main` está protegida: **solo @Mokius y @SkiTemplar** pueden meter cambios en ella, y siempre a través de
una pull request (PR) revisada y validada. Todo el equipo puede crear ramas, subir a ellas lo que quiera y abrir PR.

## Para todo el equipo

1. **Parte siempre de main actualizada**:
   ```
   git switch main
   git pull
   git switch -c tipo/descripcion-corta
   ```
   Nombres de rama: `feat/…` (algo nuevo), `fix/…` (arreglo), `docs/…`, `chore/…` (herramientas, assets de apoyo), `art/…`
   (arte y mapas). Ejemplos: `feat/cangrejo-ermitano`, `fix/gaviota-bola`.
2. **Trabaja y sube a tu rama** todas las veces que quieras (`git add`, `git commit`, `git push -u origin tu-rama`).
3. **Mantén tu rama al día con main** mientras trabajas (sobre todo antes de abrir la PR):
   ```
   git fetch origin
   git merge origin/main
   ```
   Resuelve los conflictos en tu rama, compila y prueba.
4. **Abre una pull request a main** en GitHub cuando esté lista. Rellena la plantilla (qué cambia, cómo probarlo y las
   casillas). La revisan @Mokius o @SkiTemplar: si piden cambios, súbelos a la misma rama y la PR se actualiza sola.
5. **No se puede** subir directamente a main, ni forzar (`push --force`) ni borrarla. Tampoco fusiones tú la PR: la fusiona
   quien la revisa.

## Validación obligatoria (pipeline)

Cada PR a main pasa el flujo «Validar para main» (`.github/workflows/validar-main.yml`) y no se puede fusionar hasta que
salga en verde:

- **Comprobaciones** (servidores de GitHub, 1-2 min; `.github/scripts/validar.py`): sin archivos de `Binaries/`,
  `Intermediate/`, `Saved/`, `DerivedDataCache/` ni `.vs/`; nada de más de 50 MB; sin marcadores de conflicto; código en UTF-8;
  Python sin errores de sintaxis; JSON bien formado; ninguna clave de localización (`NSLOCTEXT`) con dos textos distintos;
  traducciones (`.po`) sin marcadores ni plurales rotos. Se puede lanzar antes en local:
  `python .github/scripts/validar.py --base origin/main`.
- **Compilar y pruebas (UE 5.6)**: compila el editor con UBT y pasa todas las pruebas automáticas `Tortunabo.*`. Necesita un
  ejecutor propio con Unreal 5.6 (ver abajo); mientras no esté activado, este paso se salta.

## Assets binarios (.umap, .uasset)

Git no sabe mezclar mapas ni assets: si dos personas tocan el mismo `.umap` o `.uasset` en ramas distintas, uno de los dos
cambios se pierde. **Avisa en el grupo antes de tocar un mapa o un asset compartido** y no lo toques si otra persona lo está
editando. En la PR, di qué assets binarios cambias.

## Para @Mokius y @SkiTemplar (revisión y fusión)

- Revisar en GitHub (pestaña «Files changed»), bajar la rama y probarla si hace falta (`git fetch` y `git switch rama`).
- Fusionar solo con la validación en verde y la aprobación. Preferible «Create a merge commit» (conserva la historia de la
  rama) o «Squash and merge» para ramas con muchos commits pequeños. Borrar la rama al fusionar (lo ofrece GitHub).
- Una PR de uno de los dos la aprueba el otro.

## Reglas de main en GitHub (Settings → Branches)

Regla de protección de `main`:
- Require a pull request before merging: 1 aprobación, descartar aprobaciones al subir cambios nuevos, y aprobación de los
  propietarios del código (`.github/CODEOWNERS`: @Mokius y @SkiTemplar).
- Require status checks to pass: «Comprobaciones» y «Compilar y pruebas (UE 5.6)»; la rama debe estar al día con main.
- Require conversation resolution before merging.
- Restrict who can push to matching branches: solo @Mokius y @SkiTemplar (son los únicos que pueden fusionar).
- Sin push forzado ni borrado de la rama; se aplica también a los administradores.

## Ejecutor propio para compilar y probar (opcional, recomendable)

Los servidores de GitHub no tienen Unreal. Para que la pipeline compile y pase las pruebas hace falta un PC con UE 5.6 que
haga de ejecutor (self-hosted runner):

1. En GitHub: Settings → Actions → Runners → New self-hosted runner → Windows. Seguir los pasos que da GitHub en una carpeta
   propia (por ejemplo `C:\actions-runner`); al configurar, añadir las etiquetas `ue56` (además de las de serie `self-hosted`
   y `Windows`) y dejarlo como servicio.
2. En GitHub: Settings → Secrets and variables → Actions → Variables: crear `UE_RUNNER` = `true` (y `UE_ROOT` si el motor no
   está en `C:\Program Files\Epic Games\UE_5.6`).
3. Desde ese momento cada PR compila y pasa las pruebas en ese PC (la primera vez tarda más: compila todo).

Aviso: el ejecutor compila y ejecuta el código de las PR en ese PC. Solo tienen acceso de escritura al repositorio los
miembros del equipo; no aceptar ejecuciones de PR de forks.

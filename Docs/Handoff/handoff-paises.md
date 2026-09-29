# Traspaso: mapas de países (Tortunabo, rama macro-update)

Base: commit c8380abc6 (pipeline de países, sin mapas versionados).

## Hecho
- `Scripts/gen_terrain_country.py` + `terrain_geo/{country,countries,rally}.py`: país ISO, giro/rectángulo por presupuesto,
  puentes (cruces reales y automáticos), calzada MCP con perfil Lipschitz, relleno de agua en la banda salvo vanos,
  túneles con validación 3D (`check_tunnel`), lazos (túnel + calzada vieja), puentes de valle (Deck de terrain_path,
  máximo exacto, no `fuse`), terrazas TcT. Validación en `evaluate_country` (calzada tunnel-aware `road_ok`).
- Generados sin registrar (no versionados, descartables) en `Scripts/terrain_volumes/Variants/`: L10_japon_v2,
  I01_filipinas_v2, L02..L13. Resultado con la regla vieja: TcT (I01 v2, L05) válidos; Rally con calzada y 2 puentes
  válidos pero sin túneles ni lazos (salvo Japón: 1 túnel válido, lazo roto desde la boca del túnel).
- Rasters en `Scripts/terrain_geo/data/<id>_{dem,mask}.png` + `_geo.json` (firma de proyección).

## Corrección del director (pendiente, prioridad)
1. Exageración: sustituir `e01_calibration` por relieve suave: desnivel máx. ≤ 6-8 % del lado menor y pendiente media
   ≤ 15°; filtrar el MDE (gaussiana más ancha) para masas, no pinchos. Documentar la cifra por mapa (manifest geo + CREDITS).
2. Camino primero: donde la calzada supere 12°, elegir túnel (roca ≥ 8 m, gálibo ≥ 5, ancho ≥ 12) o modelar
   (rebajar collado con taludes ≤ 35°, media ladera). Contar túneles y cortes en el manifest.
3. Validador: calzada ≤ 12° sostenida, ancho ≥ 12 m (hoy se valida 10 m), túneles, histograma de pendientes.
4. Lámina con perfil longitudinal de la calzada y túneles/cortes/puentes marcados. Mirarlas.
5. index.json: marcar como descartados L10_japon_fuji e I01_filipinas_rara (descripción) y registrar los nuevos
   con `gen_terrain_country.py <ids> --register-only` justo antes de cada commit (leer-modificar-escribir).

## Fallos conocidos
- Lazo de Japón: `lazo_cortes` desde la boca del túnel (302,392); sospecha: la rama vieja y el hueco del túnel se
  solapan cerca de la boca. Depurar con `Saved/_top_<id>.npz` (top, corner, path) y `road_breaks`.
- `find_shortcuts` no encuentra túneles en países de relieve suave: con la corrección el túnel debe salir de la regla
  de pendiente > 12°, no de atajos.
- Rusia y China con la regla vieja estaban en marcha en segundo plano (log en `Saved/_countries.log`).

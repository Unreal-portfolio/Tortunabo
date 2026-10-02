"""Especificación del mapa de Supervivencia (Docs/Mapa_Supervivencia.md). Un mapa nuevo por nivel, entero, de
una pasada, rectangular y alargado, hecho con el generador del Coop adaptado. En el formato de intercambio el inicio
va al oeste y la meta al este."""

from __future__ import annotations

LENGTH_M = 200                    # Este (columnas de top); de 400 a 200 m para que cada nivel dure < 5 min (#273)
WIDTH_M = 75                      # Norte (filas de top)
MIN_ASPECT = 2.0                  # largo / ancho mínimo: «alargado»
DIFFICULTIES = (1, 2, 3, 4, 5)    # el nivel N de la partida pide dificultad min(N, 5)
START_MARGIN_M = 12               # el inicio y la meta caen a esta distancia del borde corto
MIN_PATH_WIDTH_M = 3.0            # ancho mínimo del camino de inicio a meta (caben varias tortugas)
MAX_JUMP_M = 4.0                  # los huecos de salto del Coop se cruzan si su salto más largo cabe en el dive (#273)
JUMP_WEIGHT = 0.25                # peso en el reto de cada hueco saltado por cada 100 m de camino
MAX_GEN_SECONDS = 3.0             # un nivel no puede hacer esperar más que una pantalla de carga
TRI_PER_KM2 = 3_000_000           # presupuesto de malla: la densidad de C01 (1,1 M triángulos en 0,36 km²)
MAX_TRIANGLES = int(TRI_PER_KM2 * LENGTH_M * WIDTH_M / 1_000_000)   # 45 000
MIN_VARIETY = 0.05                # diferencia RMS normalizada entre dos semillas: «nuevo cada vez»
MIN_DIFFICULTY_RANK = 0.5         # Spearman mínimo entre dificultad pedida y reto medido

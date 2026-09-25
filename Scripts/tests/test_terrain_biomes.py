"""Tests del generador de mapas de biomas (Scripts/gen_terrain_biomes.py).

    uv run --with pytest --with numpy --with scipy --with pillow --with scikit-image \
        pytest Scripts/tests
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from gen_terrain_biomes import (BIOME_ORDER, MIN_SEGMENT, ambient_biomes, biome_mask, build_map,  # noqa: E402
                                plan_biomes, window)
from terrain_gen.biomes import (TUNNEL_EDGE_CLEAR_M, WATER_TOP_M, design_sand_tunnel,  # noqa: E402
                                design_water_path, mouth_point)
from terrain_gen.core import DIST_TO_EDGE, HEIGHT_ZERO, RES, UNITS_PER_M, WALKABLE_STEP_M, WATER_M  # noqa: E402

SEEDS = (20260925, 7, 1234)


@pytest.fixture(scope="module", params=SEEDS)
def biome_map(request):
    return build_map(request.param)


def test_los_tramos_siguen_el_orden_arena_agua_algas(biome_map):
    biomes = biome_map.path_biomes
    runs = [b for k, b in enumerate(biomes) if k == 0 or biomes[k - 1] != b]
    assert tuple(runs) == BIOME_ORDER
    assert all(biomes.count(b) >= MIN_SEGMENT for b in BIOME_ORDER)


def test_plan_biomes_rechaza_un_camino_demasiado_corto():
    with pytest.raises(ValueError):
        plan_biomes(np.random.default_rng(0), MIN_SEGMENT * len(BIOME_ORDER) - 1)


def test_las_celdas_que_tocan_agua_del_camino_son_agua():
    cells = [(0, 0), (0, 1), (0, 2), (1, 2), (2, 2), (2, 1), (2, 0), (3, 0), (4, 0)]
    biomes = ["sand"] * 3 + ["water"] * 3 + ["algae"] * 3
    result = ambient_biomes(cells, biomes, 5)
    assert result[(1, 1)] == "water"          # rodeada de agua del camino
    assert result[(1, 3)] == "water"          # diagonal a (2, 2)
    assert result[(4, 4)] == "algae" or result[(4, 4)] == "water"


def test_las_celdas_vecinas_del_agua_son_mayoritariamente_agua(biome_map):
    water_cells = {c for c, b in zip(biome_map.cells, biome_map.path_biomes) if b == "water"}
    grid = int(round((biome_map.terrain.shape[0] - 1) / (RES - 1)))
    for col, row in water_cells:
        for dc, dr in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            neighbour = (col + dc, row + dr)
            if not (0 <= neighbour[0] < grid and 0 <= neighbour[1] < grid) or neighbour in biome_map.cells:
                continue
            flooded = float(np.mean(biome_map.terrain[window(*neighbour)] < WATER_M))
            assert flooded >= 0.6, f"vecino {neighbour} de agua con {flooded:.0%} de agua"


def test_hay_un_tunel_en_el_tramo_de_arena(biome_map):
    sand = {c for c, b in zip(biome_map.cells, biome_map.path_biomes) if b == "sand"}
    assert any(c in sand for c in biome_map.layers)


def test_las_capas_del_tunel_estan_sobre_el_suelo_y_cosidas_en_el_borde(biome_map):
    floor_q = np.clip(np.rint(biome_map.terrain * UNITS_PER_M) + HEIGHT_ZERO, 1, 65535).astype(np.int64)
    for cell, (roof, ceiling) in biome_map.layers.items():
        floor = floor_q[window(*cell)]
        valid = roof > 0
        assert valid.any()
        assert np.array_equal(valid, ceiling > 0)
        assert np.all(roof[valid].astype(np.int64) >= ceiling[valid])
        assert np.all(ceiling[valid].astype(np.int64) >= floor[valid])
        # Borde lateral: muestras de la huella con un vecino fuera y sin altura sobre el suelo.
        clearance = (ceiling[valid].astype(np.int64) - floor[valid]) / UNITS_PER_M
        assert clearance.max() >= 4.5, "la boveda no deja paso"
        assert np.any(roof[valid].astype(np.int64) == floor[valid]), "sin costura lateral"


def test_el_tunel_no_se_acerca_al_borde():
    rng = np.random.default_rng(3)
    for _ in range(10):
        design = design_sand_tunnel(rng, {"S": mouth_point("S", 5.0), "N": mouth_point("N", -8.0)})
        if design is None or design.roof_delta is None:
            continue
        valid = ~np.isnan(design.roof_delta)
        assert DIST_TO_EDGE[valid].min() >= TUNNEL_EDGE_CLEAR_M - 8.0


def test_el_pasillo_de_agua_es_seco_de_boca_a_boca():
    rng = np.random.default_rng(11)
    mouths = {"S": mouth_point("S", 10.0), "E": mouth_point("E", -12.0)}
    floor = design_water_path(rng, mouths).floor
    dry = np.where(floor > WATER_M + 0.2, floor, 1e6)
    start: tuple[int, int] = (0, int(round(10.0 + 50.0)))
    goal: tuple[int, int] = (int(round(-12.0 + 50.0)), RES - 1)
    assert floor[start] == pytest.approx(WATER_TOP_M, abs=0.05)
    seen = np.zeros_like(dry, dtype=bool)
    stack: list[tuple[int, int]] = [start]
    seen[start] = True
    while stack:
        i, j = stack.pop()
        for ni, nj in ((i + 1, j), (i - 1, j), (i, j + 1), (i, j - 1)):
            if 0 <= ni < RES and 0 <= nj < RES and not seen[ni, nj] and abs(dry[ni, nj] - dry[i, j]) <= WALKABLE_STEP_M:
                seen[ni, nj] = True
                stack.append((ni, nj))
    assert seen[goal]


def test_la_mascara_marca_algas_solo_en_su_bioma(biome_map):
    mask, pairs = biome_mask(biome_map)
    for cell, (own, _) in pairs.items():
        foliage = (mask[window(*cell)] & 0xFF).astype(np.float64)
        if own == "sand":
            assert foliage.mean() < 40.0
        if own == "algae" and cell not in biome_map.cells:
            assert foliage.mean() > 100.0

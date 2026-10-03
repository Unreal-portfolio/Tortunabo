"""Formato de intercambio entre generadores y banco: un .npz por mapa. Lo escribe tanto un generador Python
como el export de C++ (Mokius), así que solo lleva datos simples."""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from . import spec


@dataclass
class SurvivalMap:
    top: np.ndarray                       # cota de lo alto de cada columna (m), una muestra por metro, [Norte, Este]
    start: tuple[int, int]                # índices de top
    goal: tuple[int, int]
    seed: int
    difficulty: int
    algorithm: str = ""
    gen_seconds: float = 0.0
    triangles: int | None = None          # de la malla final; si falta se cuenta la rejilla completa
    # huecos de salto: una fila por hueco, (fila, columna) de cada borde y el salto más largo (m); vacío si no hay
    jumps: np.ndarray = field(default_factory=lambda: np.zeros((0, 5)))

    def triangle_count(self) -> int:
        if self.triangles is not None:
            return int(self.triangles)
        return 2 * (self.top.shape[0] - 1) * (self.top.shape[1] - 1)

    def save(self, path: Path) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        np.savez_compressed(path, top=self.top.astype(np.float32), start=np.array(self.start), goal=np.array(self.goal),
                            seed=self.seed, difficulty=self.difficulty, algorithm=self.algorithm,
                            gen_seconds=self.gen_seconds, triangles=-1 if self.triangles is None else self.triangles,
                            jumps=np.asarray(self.jumps, dtype=np.float64).reshape(-1, 5))

    @staticmethod
    def load(path: Path) -> "SurvivalMap":
        with np.load(path, allow_pickle=False) as z:
            triangles = int(z["triangles"])
            jumps = z["jumps"].reshape(-1, 5) if "jumps" in z.files else np.zeros((0, 5))
            return SurvivalMap(top=z["top"].astype(np.float64), start=(int(z["start"][0]), int(z["start"][1])),
                               goal=(int(z["goal"][0]), int(z["goal"][1])), seed=int(z["seed"]),
                               difficulty=int(z["difficulty"]), algorithm=str(z["algorithm"]),
                               gen_seconds=float(z["gen_seconds"]), triangles=None if triangles < 0 else triangles,
                               jumps=jumps.astype(np.float64))


def expected_shape() -> tuple[int, int]:
    return spec.WIDTH_M + 1, spec.LENGTH_M + 1

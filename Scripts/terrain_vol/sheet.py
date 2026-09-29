"""Lamina de revision de un mapa (Docs/Mapas/<id>.png, < 1 MB): vista cenital con cotas sobre el agua (tintas
hipsometricas, curvas de nivel rotuladas, inicio y final) y una perspectiva sombreada. Necesita matplotlib:

    uv run --with matplotlib ... (lo llaman gen_terrain_geo/island/arena con --sheet)
"""

from __future__ import annotations

import io
from pathlib import Path

import numpy as np
from PIL import Image

from .layout import MAP_MIN_M, WATER_M

MAX_BYTES = 1_000_000


def _colormap():
    from matplotlib.colors import LinearSegmentedColormap
    stops = [(0.0, (0.05, 0.18, 0.38)), (0.40, (0.20, 0.52, 0.72)), (0.495, (0.55, 0.78, 0.86)),
             (0.505, (0.93, 0.86, 0.62)), (0.60, (0.82, 0.70, 0.45)), (0.75, (0.66, 0.52, 0.32)),
             (0.90, (0.52, 0.40, 0.28)), (1.0, (0.96, 0.95, 0.93))]
    return LinearSegmentedColormap.from_list("tn_hypso", stops)


def _save_small(fig, path: Path) -> int:
    buffer = io.BytesIO()
    fig.savefig(buffer, format="png", dpi=fig.dpi)
    image = Image.open(io.BytesIO(buffer.getvalue())).convert("RGB")
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path, optimize=True)
    if path.stat().st_size > MAX_BYTES:
        image.quantize(colors=256, method=Image.Quantize.MEDIANCUT).save(path, optimize=True)
    return path.stat().st_size


def render_sheet(top: np.ndarray, path: Path, title: str, subtitle: str = "", start: tuple[int, int] | None = None,
                 end: tuple[int, int] | None = None, marks: dict[str, tuple[int, int]] | None = None,
                 contour_step_m: float | None = None, perspective_z_scale: float = 1.0) -> int:
    """top: cota absoluta (m) de cada muestra de 1 m, [Norte, Este] desde MAP_MIN_M. Devuelve los bytes del PNG."""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.colors import LightSource, TwoSlopeNorm

    rel = top - WATER_M
    size = top.shape[0] - 1
    extent = (MAP_MIN_M, MAP_MIN_M + size, MAP_MIN_M, MAP_MIN_M + size)
    vmax = max(float(rel.max()), 2.0)
    norm = TwoSlopeNorm(vcenter=0.0, vmin=min(float(rel.min()), -1.0), vmax=vmax)
    cmap = _colormap()
    step = contour_step_m or float(np.clip(np.round(vmax / 12.0), 1.0, 10.0))

    fig = plt.figure(figsize=(16, 8), dpi=90)
    fig.suptitle(title, fontsize=15, fontweight="bold", x=0.02, ha="left")
    if subtitle:
        fig.text(0.02, 0.945, subtitle, fontsize=9.5, ha="left", va="top", wrap=True)

    ax = fig.add_axes((0.035, 0.06, 0.44, 0.78))
    shade = LightSource(azdeg=315, altdeg=40).hillshade(rel, vert_exag=2.0)
    rgb = cmap(norm(rel))[..., :3] * (0.55 + 0.45 * shade[..., None])
    ax.imshow(rgb, origin="lower", extent=(extent[2], extent[3], extent[0], extent[1]))
    axis = MAP_MIN_M + np.arange(top.shape[0])
    levels = np.arange(step, vmax + step, step)
    cs = ax.contour(axis, axis, rel, levels=levels, colors="k", linewidths=0.45, alpha=0.6)
    ax.clabel(cs, cs.levels[::2] if len(cs.levels) > 8 else cs.levels, fmt="%g", fontsize=7, inline=True)
    ax.contour(axis, axis, rel, levels=[0.0], colors=(0.05, 0.25, 0.5), linewidths=0.9)
    for label, point, color in (("inicio", start, "lime"), ("final", end, "red")):
        if point is not None:
            ax.plot(MAP_MIN_M + point[1], MAP_MIN_M + point[0], "o", ms=9, mec="k", mfc=color)
            ax.annotate(label, (MAP_MIN_M + point[1], MAP_MIN_M + point[0]), xytext=(6, 6), textcoords="offset points",
                        fontsize=9, fontweight="bold", color="k", backgroundcolor=(1, 1, 1, 0.6))
    for label, point in (marks or {}).items():
        ax.plot(MAP_MIN_M + point[1], MAP_MIN_M + point[0], "^", ms=7, mec="k", mfc="yellow")
        ax.annotate(label, (MAP_MIN_M + point[1], MAP_MIN_M + point[0]), xytext=(5, -10), textcoords="offset points",
                    fontsize=7.5, color="k", backgroundcolor=(1, 1, 1, 0.5))
    ax.set_xlabel("Este (m de juego)")
    ax.set_ylabel("Norte (m de juego)")
    ax.set_title(f"Cenital: cotas sobre el agua, curvas cada {step:g} m (cima {vmax:.1f} m)", fontsize=10)
    sm = plt.cm.ScalarMappable(norm=norm, cmap=cmap)
    fig.colorbar(sm, ax=ax, fraction=0.04, pad=0.01, label="m sobre el agua")

    ax3 = fig.add_axes((0.49, 0.0, 0.51, 0.84), projection="3d")
    stride = max(1, size // 180)
    sub = rel[::stride, ::stride]
    xs = MAP_MIN_M + np.arange(sub.shape[1]) * stride
    ys = MAP_MIN_M + np.arange(sub.shape[0]) * stride
    Xg, Yg = np.meshgrid(xs, ys)
    shade3 = LightSource(azdeg=315, altdeg=35).hillshade(sub, vert_exag=2.0)
    colors = cmap(norm(np.maximum(sub, -0.5)))
    colors[..., :3] *= (0.5 + 0.5 * shade3[..., None])
    ax3.plot_surface(Xg, Yg, np.maximum(sub, -0.2) * perspective_z_scale, facecolors=colors, rstride=1, cstride=1,
                     linewidth=0, antialiased=False, shade=False)
    ax3.view_init(elev=38, azim=-62)
    ax3.set_box_aspect((1.0, 1.0, max(0.12, min(0.5, vmax * perspective_z_scale / size * 1.3))), zoom=1.35)
    ax3.set_axis_off()
    ax3.set_title("Perspectiva desde el Suroeste" + (f" (relieve x{perspective_z_scale:g})" if perspective_z_scale != 1 else ""),
                  fontsize=10)
    written = _save_small(fig, path)
    plt.close(fig)
    return written


def _thousands(n: int) -> str:
    return f"{n:,}".replace(",", ".")


def sheet_subtitle(description: str, grid: int, report: dict, size_mb: float, extra: str = "") -> str:
    """Segunda linea de la lamina: tamaño, malla, pendientes y veredicto de los validadores."""
    b = report["budget"]
    slope = report["slope_deg"]
    parts = [f"{grid * 100} x {grid * 100} m", f"{_thousands(b['triangles'])} triángulos (tope {_thousands(b['budget_triangles'])})",
             f"{size_mb:.2f} MB".replace(".", ","), f"pendiente p50/p90 {slope['p50']:.0f}°/{slope['p90']:.0f}°"]
    if extra:
        parts.append(extra)
    parts.append("válido" if report["ok"] else "NO válido")
    return f"{description}\n" + "; ".join(parts) + "."

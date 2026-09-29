"""Planos del cooperativo para el tráiler (PIE en LVL_ProcMap): vuelos por el camino del mapa procedural a distintas
alturas del recorrido (cada tramo cae en un bioma distinto) y el grupo de tortugas."""
import math

import unreal

import captura
from rodaje import actors, follow_actor, pawn, tp, world, xyz


def gen(w=None):
    g = actors("TN_ProcMapGenerator", w or world())
    return g[0] if g else None


def ready():
    g = gen()
    return bool(g and g.is_map_ready())


def path_at(p, w=None):
    g = gen(w)
    # El progreso del generador va en cm a lo largo del camino principal; aquí p es la fracción del recorrido.
    r = g.get_path_location_at_progress(float(p) * g.get_main_path_length())
    loc, d = (r[0], r[1]) if isinstance(r, tuple) else (r, unreal.Vector(1, 0, 0))
    return [loc.x, loc.y, loc.z], [d.x, d.y, d.z]


def fly(name, p0, p1, frames=150, height=900.0, side=500.0, fov=78.0, tags=None):
    """Vuelo por el camino de p0 a p1 (fracciones del recorrido), a una altura sobre él y mirando hacia delante."""
    keys = []
    n = 5
    for k in range(n + 1):
        t = k / n
        loc, d = path_at(p0 + (p1 - p0) * t)
        dn = math.hypot(d[0], d[1]) or 1.0
        fx, fy = d[0] / dn, d[1] / dn
        eye = [loc[0] - fx * 600 - fy * side, loc[1] - fy * 600 + fx * side, loc[2] + height]
        ahead, _ = path_at(min(1.0, p0 + (p1 - p0) * t + 0.02))
        keys.append({"f": int(t * (frames - 1)), "eye": eye, "look": [ahead[0], ahead[1], ahead[2] + 150], "fov": fov})
    return captura.shot(name, frames, keys=keys, fov=fov, tags=tags or ["cooperativo", "bioma", "vuelo"],
                        desc="Vuelo por el camino del mapa procedural (%.2f-%.2f del recorrido)" % (p0, p1))


def coop_group(p=0.12):
    """El jugador y tres tortugas de reparto andando juntas por el camino."""
    w = world()
    loc, d = path_at(p, w)
    yaw = math.degrees(math.atan2(d[1], d[0]))
    turtles = [pawn(w)] + [a for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Pawn) if a.actor_has_tag("TrailerPuppet")]
    side = [-d[1], d[0]]
    for k, t in enumerate(turtles[:4]):
        off = (k - 1.5) * 170.0
        tp(t, [loc[0] + side[0] * off, loc[1] + side[1] * off, loc[2] + 150], yaw)
    dirv = unreal.Vector(d[0], d[1], 0.0)

    def push(ww, i):
        if i >= -5:
            for t in turtles[:4]:
                t.add_movement_input(dirv, 0.8, True)
    return captura.shot("coop_group", 180, fov=72, tags=["cooperativo", "tortugas", "grupo"],
                        desc="Cuatro tortugas avanzando juntas en el cooperativo", each=push,
                        follow=follow_actor(lambda ww: pawn(ww), (d[0] * 850 + side[0] * 300, d[1] * 850 + side[1] * 300, 230),
                                            (0, 0, 60), smooth=0.2, fov=72))

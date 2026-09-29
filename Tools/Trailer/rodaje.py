"""Rodaje del tráiler: planos coreografiados sobre captura.py (dentro del editor, en PIE de la carrera o del lobby).

Cada función de plano prepara la escena (teletransporta la tortuga, crea piezas con TN.Beach.Place, lanza comandos) y
llama a captura.shot(...). Se lanzan de uno en uno con ue.py: `import rodaje; rodaje.<plano>()` y se espera a que
captura.status()["busy"] sea False.
"""
import math

import unreal

import captura


# ── Utilidades ────────────────────────────────────────────────────────────────

def world():
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    return ues.get_game_world()


def pawn(w=None):
    return unreal.GameplayStatics.get_player_pawn(w or world(), 0)


def pc(w=None):
    return unreal.GameplayStatics.get_player_controller(w or world(), 0)


def xyz(actor_or_vec):
    v = actor_or_vec.get_actor_location() if hasattr(actor_or_vec, "get_actor_location") else actor_or_vec
    return [v.x, v.y, v.z]


def cmd(s, w=None):
    unreal.SystemLibrary.execute_console_command(w or world(), s)


def tp(p, where, yaw=0.0):
    """Teletransporta una tortuga (con su control, si lo tiene) y la deja quieta."""
    p.set_actor_location_and_rotation(unreal.Vector(*[float(c) for c in where]), unreal.Rotator(0.0, 0.0, float(yaw)), False, True)
    mc = p.get_movement_component()
    if mc:
        mc.stop_movement_immediately()
    c = p.get_controller()
    if c:
        c.set_control_rotation(unreal.Rotator(0.0, -8.0, float(yaw)))


def actors(cls_name, w=None):
    cls = unreal.load_class(None, "/Script/Tortunabo." + cls_name)
    return list(unreal.GameplayStatics.get_all_actors_of_class(w or world(), cls)) if cls else []


def nearest(cls_name, to, w=None):
    best, bd = None, 1e30
    for a in actors(cls_name, w):
        d = sum((xyz(a)[i] - to[i]) ** 2 for i in range(3))
        if d < bd:
            best, bd = a, d
    return best


def ground(x, y, w=None):
    """Altura del suelo en (x, y) con una traza desde muy arriba (en el mundo de la partida)."""
    hit = unreal.SystemLibrary.line_trace_single(w or world(), unreal.Vector(x, y, 30000.0), unreal.Vector(x, y, -5000.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
    if hit:
        return hit.to_tuple()[4].z if hasattr(hit, "to_tuple") else 0.0
    return 0.0


def follow_actor(get_actor, offset, look_offset=(0, 0, 60), smooth=0.18, fov=70.0, world_space=True):
    """Cámara que sigue a un actor con un desfase (en el mundo o relativo a su guiñada), suavizada."""
    st = {"eye": None, "look": None}

    def f(w, i):
        a = get_actor(w)
        if not a:
            return st["eye"] or [0, 0, 1000], st["look"] or [1000, 0, 1000], fov
        p = xyz(a)
        if world_space:
            eye = [p[0] + offset[0], p[1] + offset[1], p[2] + offset[2]]
        else:
            yaw = math.radians(a.get_actor_rotation().yaw)
            cx, sx = math.cos(yaw), math.sin(yaw)
            eye = [p[0] + offset[0] * cx - offset[1] * sx, p[1] + offset[0] * sx + offset[1] * cx, p[2] + offset[2]]
        look = [p[0] + look_offset[0], p[1] + look_offset[1], p[2] + look_offset[2]]
        if st["eye"] is None or i <= 0:
            st["eye"], st["look"] = eye, look
        else:
            k = smooth
            st["eye"] = [st["eye"][j] + (eye[j] - st["eye"][j]) * k for j in range(3)]
            st["look"] = [st["look"][j] + (look[j] - st["look"][j]) * k for j in range(3)]
        return st["eye"], st["look"], fov
    return f


def orbit(center, radius, height, deg0, deg1, frames, fov=60.0, look_z=0.0):
    keys = []
    n = 6
    for k in range(n + 1):
        t = k / n
        a = math.radians(deg0 + (deg1 - deg0) * t)
        keys.append({"f": int(t * (frames - 1)), "eye": [center[0] + radius * math.cos(a), center[1] + radius * math.sin(a), center[2] + height],
                     "look": [center[0], center[1], center[2] + look_z], "fov": fov})
    return keys


def run_forward(p, yaw_deg=0.0, scale=1.0):
    """Para `each`: empuja a la tortuga hacia delante (entrada de movimiento) cada tic."""
    d = unreal.Vector(math.cos(math.radians(yaw_deg)), math.sin(math.radians(yaw_deg)), 0.0)

    def f(w, i):
        if p and i >= 0:
            p.add_movement_input(d, scale, True)
    return f

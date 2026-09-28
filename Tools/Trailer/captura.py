"""Captura de planos para el tráiler (Tools/Trailer/CONTRATO.md): se ejecuta DENTRO del editor de Unreal (Python del
editor), con el editor lanzado con paso fijo («-UseFixedTimeStep -FPS=30»), así cada tic del motor avanza exactamente
1/30 s del mundo y cada fotograma capturado es un fotograma del vídeo, vaya el editor a la velocidad que vaya.

Uso (desde ue.py, en llamadas sueltas):
    import sys; sys.path.insert(0, r"<repo>/Tools/Trailer"); import captura
    captura.shot("beach_fly_start", frames=150, keys=[...], tags=[...], desc="...")   # empieza; vuelve enseguida
    captura.status()                                                                  # {'busy':…, 'frame':…}

Cada plano: una SceneCapture2D que sigue una trayectoria de cámara (claves con Catmull-Rom y suavizado) o a un actor,
capturada en cada tic tras el tic del mundo (post-tick de Slate) y exportada a PNG en
Saved/Trailer/footage/<plano>/frame_00000.png. Después, png_a_jpg() (fuera del editor) las pasa a JPG. Los «eventos»
(llamadas en un fotograma concreto) sirven para coreografiar: mover la tortuga, lanzar comandos de consola…
"""
import json
import math
import os
import time

import unreal

ROOT = r"C:\Users\mokiu\Documents\Unreal Projects\Tortunabo\Saved\Trailer\footage"
W, H = 1920, 1080

_state = {"busy": False, "name": None, "frame": 0, "frames": 0, "handle": None, "error": None, "log": []}


def _world():
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    return ues.get_game_world() or ues.get_editor_world()


def _lerp(a, b, t):
    return a + (b - a) * t


def _vec(v):
    return unreal.Vector(float(v[0]), float(v[1]), float(v[2]))


def _catmull(p0, p1, p2, p3, t):
    t2, t3 = t * t, t * t * t
    return [0.5 * ((2 * p1[i]) + (-p0[i] + p2[i]) * t + (2 * p0[i] - 5 * p1[i] + 4 * p2[i] - p3[i]) * t2
                   + (-p0[i] + 3 * p1[i] - 3 * p2[i] + p3[i]) * t3) for i in range(3)]


def _ease(t, kind):
    if kind == "in":
        return t * t
    if kind == "out":
        return 1 - (1 - t) * (1 - t)
    if kind == "inout":
        return t * t * (3 - 2 * t)
    return t


def _path(points, t):
    """Punto de la trayectoria Catmull-Rom por los puntos (lista de [x,y,z]) en t∈[0,1] (velocidad por tramos)."""
    n = len(points)
    if n == 1:
        return list(points[0])
    seg = min(int(t * (n - 1)), n - 2)
    local = t * (n - 1) - seg
    p0 = points[max(seg - 1, 0)]
    p1 = points[seg]
    p2 = points[seg + 1]
    p3 = points[min(seg + 2, n - 1)]
    return _catmull(p0, p1, p2, p3, local)


def look_rot(eye, target, roll=0.0):
    d = [target[i] - eye[i] for i in range(3)]
    yaw = math.degrees(math.atan2(d[1], d[0]))
    pitch = math.degrees(math.atan2(d[2], math.hypot(d[0], d[1])))
    return unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw)


def _new_capture(world, fov):
    """SceneCapture2D para el plano. En el editor se crea; en la partida (PIE) no se pueden crear actores desde Python,
    así que se usa la que se dejó en el nivel antes de darle a Play (etiqueta «TrailerCam», ver place_rig)."""
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    owned = False
    if world == ues.get_editor_world():
        eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        cap = eas.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0), transient=True)
        owned = True
    else:
        caps = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SceneCapture2D) if a.actor_has_tag("TrailerCam")]
        if not caps:
            raise RuntimeError("no hay SceneCapture2D «TrailerCam» en la partida: place_rig() antes de darle a Play")
        cap = caps[0]
    comp = cap.get_editor_property("capture_component2d")
    rt = unreal.RenderingLibrary.create_render_target2d(world, W, H, unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB,
                                                        unreal.LinearColor(0, 0, 0, 1))
    comp.set_editor_property("texture_target", rt)
    comp.set_editor_property("fov_angle", fov)
    comp.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    comp.set_editor_property("capture_every_frame", False)
    comp.set_editor_property("capture_on_movement", False)
    # Estado de render persistente: antialiasing temporal, desenfoque de movimiento y exposición con historia.
    comp.set_editor_property("always_persist_rendering_state", True)
    return cap, comp, rt, owned


def place_rig(puppets=3, base=(-2600.0, -600.0, 5400.0)):
    """En el nivel del editor (fuera de PIE): la cámara «TrailerCam» y tortugas de reparto («TrailerPuppet_N»), que pasan
    a la partida al darle a Play. No se guarda el nivel: al cerrar, se descartan."""
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in eas.get_all_level_actors():
        if a.actor_has_tag("TrailerCam") or a.actor_has_tag("TrailerPuppet"):
            eas.destroy_actor(a)
    cam = eas.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(-6000, 0, 12000), unreal.Rotator(0, 0, 0))
    cam.tags = ["TrailerCam"]
    cam.set_actor_label("TrailerCam")
    cls = unreal.load_class(None, "/Game/Blueprints/Characters/BP_TortugaCharacter.BP_TortugaCharacter_C")
    made = []
    if cls:
        for k in range(puppets):
            p = eas.spawn_actor_from_class(cls, unreal.Vector(base[0], base[1] + 400 * k, base[2]), unreal.Rotator(0, 0, 0))
            p.tags = ["TrailerPuppet"]
            p.set_actor_label("TrailerPuppet_%d" % k)
            try:
                p.set_editor_property("auto_possess_ai", unreal.AutoPossessAI.PLACED_IN_WORLD_OR_SPAWNED)
            except Exception:  # noqa: BLE001
                pass
            made.append(p.get_name())
    return {"cam": cam.get_name(), "puppets": made, "class": bool(cls)}


def shot(name, frames, keys=None, follow=None, events=None, fov=70.0, warmup=20, tags=None, desc="", each=None):
    """Empieza a capturar un plano.

    keys: lista de claves {"f": fotograma, "eye": [x,y,z], "look": [x,y,z], "fov": opcional, "ease": opcional}; la
    cámara pasa por los ojos con Catmull-Rom y mira a un punto interpolado. follow: función(frame) -> (eye, look, fov) que
    manda sobre keys (para seguir a un actor). events: {fotograma: función(world) o texto de consola}. warmup: tics sin
    exportar al principio (el antialiasing temporal y la exposición se asientan). each: función(world, frame) que se
    llama en cada tic antes de colocar la cámara (para mover tortugas, empujar…).
    """
    if _state["busy"]:
        raise RuntimeError("ya hay un plano en marcha: " + str(_state["name"]))
    world = _world()
    out = os.path.join(ROOT, name)
    os.makedirs(out, exist_ok=True)
    for f in os.listdir(out):
        if f.startswith("frame_"):
            os.remove(os.path.join(out, f))
    cap, comp, rt, owned = _new_capture(world, fov)
    keys = sorted(keys or [], key=lambda k: k["f"])
    events = dict(events or {})
    _state.update({"busy": True, "name": name, "frame": -warmup, "frames": frames, "error": None, "t0": time.time()})

    def camera_at(i):
        if follow:
            return follow(world, i)
        if not keys:
            return [0, 0, 1000], [1000, 0, 1000], fov
        fi = max(0, i)
        # Tramo de claves en el que cae este fotograma.
        if fi <= keys[0]["f"]:
            k = keys[0]
            return k["eye"], k["look"], k.get("fov", fov)
        if fi >= keys[-1]["f"]:
            k = keys[-1]
            return k["eye"], k["look"], k.get("fov", fov)
        eyes = [k["eye"] for k in keys]
        looks = [k["look"] for k in keys]
        # Tiempo normalizado por claves (cada clave a su fotograma).
        for s in range(len(keys) - 1):
            if keys[s]["f"] <= fi <= keys[s + 1]["f"]:
                span = max(1, keys[s + 1]["f"] - keys[s]["f"])
                local = _ease((fi - keys[s]["f"]) / span, keys[s + 1].get("ease", "inout" if len(keys) == 2 else "lin"))
                t = (s + local) / (len(keys) - 1)
                f0, f1 = keys[s].get("fov", fov), keys[s + 1].get("fov", fov)
                return _path(eyes, t), _path(looks, t), _lerp(f0, f1, local)
        return keys[-1]["eye"], keys[-1]["look"], fov

    def tick(delta):
        try:
            i = _state["frame"]
            ev = events.pop(i, None)
            if ev is not None:
                if isinstance(ev, str):
                    unreal.SystemLibrary.execute_console_command(world, ev)
                else:
                    ev(world)
            if each is not None:
                each(world, i)
            eye, look, f = camera_at(i)
            cap.set_actor_location_and_rotation(_vec(eye), look_rot(eye, look), False, True)
            comp.set_editor_property("fov_angle", float(f))
            comp.capture_scene()
            if i >= 0:
                unreal.RenderingLibrary.export_render_target(world, rt, out, "frame_%05d.png" % i)
            _state["frame"] = i + 1
            if i + 1 >= frames:
                _finish(name, frames, tags, desc)
        except Exception as e:  # noqa: BLE001 — cualquier fallo corta el plano y queda anotado
            _state["error"] = repr(e)
            _finish(name, _state["frame"], tags, desc)

    def _finish(n, count, t, d):
        if _state["handle"] is not None:
            unreal.unregister_slate_post_tick_callback(_state["handle"])
            _state["handle"] = None
        if owned:
            try:
                cap.destroy_actor()
            except Exception:  # noqa: BLE001
                pass
        meta = {"name": n, "frames": int(count), "fps": 30, "desc": d, "tags": t or [], "format": "png"}
        with open(os.path.join(out, "shot.json"), "w", encoding="utf-8") as fh:
            json.dump(meta, fh, ensure_ascii=False, indent=1)
        idx_path = os.path.join(ROOT, "index.json")
        idx = {}
        if os.path.exists(idx_path):
            try:
                with open(idx_path, encoding="utf-8-sig") as fh:
                    idx = json.load(fh)
            except (ValueError, OSError):
                idx = {}
            if not isinstance(idx, dict):
                idx = {}
        idx[n] = meta
        with open(idx_path, "w", encoding="utf-8") as fh:
            json.dump(idx, fh, ensure_ascii=False, indent=1)
        _state["busy"] = False
        _state["log"].append({"name": n, "frames": count, "secs": round(time.time() - _state["t0"], 1), "error": _state["error"]})

    _state["handle"] = unreal.register_slate_post_tick_callback(tick)
    return {"started": name, "frames": frames}


def status():
    return {k: _state[k] for k in ("busy", "name", "frame", "frames", "error")} | {"log": _state["log"][-5:]}


def abort():
    if _state["handle"] is not None:
        unreal.unregister_slate_post_tick_callback(_state["handle"])
        _state["handle"] = None
    _state["busy"] = False
    return "abortado"

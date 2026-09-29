"""Pone B / Círculo del mando (Gamepad_FaceButton_Right) en IA_Shell dentro de IMC_Player.

Meterse en el caparazón (IA_Shell) solo iba con Ctrl izquierdo; con el mando no tenía botón. Este script lo deja en B / Círculo,
el botón que en los menús es «volver» (los menús se lo comen: Docs/Menu_Pausa.md, «Mando: B / Círculo para meterse en el caparazón»).

Se ejecuta UNA vez con el editor abierto (y sin PIE), desde la consola del editor (Cmd) o desde el panel de salida:

    py "C:/Users/mokiu/Documents/Unreal Projects/Tortunabo/Scripts/imc_player_shell_b.py"

Es idempotente: si IA_Shell ya va con B, no toca nada. Si B ya la usa OTRA acción de IMC_Player, no cambia nada y lo dice (habría
que elegir a mano). Lo demás de IMC_Player (teclas, modificadores, disparadores) queda como está. Guarda el asset; después
hay que hacer commit de Content/Blueprints/Gameplay/Controls/IMC_Player.uasset.

Sin ejecutarlo también funciona: UTN_GameSettingsSubsystem pone B como tecla de serie de IA_Shell cuando el asset no la trae
(«teclas de serie del código»); en cuanto el asset la trae, esa lista queda vacía y manda el asset.
"""

import unreal

IMC_PATH = "/Game/Blueprints/Gameplay/Controls/IMC_Player"
ACTION_PATH = "/Game/Blueprints/Gameplay/Controls/IA_Shell"
KEY_NAME = "Gamepad_FaceButton_Right"


def key_name(mapping):
    key = mapping.get_editor_property("key")
    return str(key.get_editor_property("key_name"))


def action_name(mapping):
    action = mapping.get_editor_property("action")
    return action.get_name() if action else "(sin acción)"


def dump(imc, title):
    unreal.log("[IMC_Player] " + title)
    for mapping in imc.get_editor_property("mappings"):
        unreal.log("    %-22s %s" % (action_name(mapping), key_name(mapping)))


def main():
    imc = unreal.EditorAssetLibrary.load_asset(IMC_PATH)
    shell = unreal.EditorAssetLibrary.load_asset(ACTION_PATH)
    if imc is None or shell is None:
        unreal.log_error("[IMC_Player] No se pudo cargar %s o %s." % (IMC_PATH, ACTION_PATH))
        return False

    dump(imc, "antes")
    mappings = list(imc.get_editor_property("mappings"))

    # Choque: B ya la usa otra acción.
    for mapping in mappings:
        if key_name(mapping) == KEY_NAME and action_name(mapping) != shell.get_name():
            unreal.log_error("[IMC_Player] %s ya es de %s: no se cambia nada. Quitarla de ahí a mano y volver a ejecutar."
                             % (KEY_NAME, action_name(mapping)))
            return False

    # IA_Shell: sus botones de mando de ahora (los de teclado se quedan).
    own_pad_keys = [key_name(m) for m in mappings
                    if action_name(m) == shell.get_name() and key_name(m).startswith("Gamepad_")]
    if own_pad_keys == [KEY_NAME]:
        unreal.log("[IMC_Player] IA_Shell ya va con %s: nada que hacer." % KEY_NAME)
        return True

    imc.modify(True)
    for old in own_pad_keys:
        old_key = unreal.Key()
        old_key.set_editor_property("key_name", old)
        imc.unmap_key(shell, old_key)
        unreal.log("[IMC_Player] IA_Shell: fuera %s." % old)

    new_key = unreal.Key()
    new_key.set_editor_property("key_name", KEY_NAME)
    imc.map_key(shell, new_key)

    # Se comprueba releyendo el asset antes de guardar.
    dump(imc, "despues")
    ok = any(action_name(m) == shell.get_name() and key_name(m) == KEY_NAME for m in imc.get_editor_property("mappings"))
    if not ok:
        unreal.log_error("[IMC_Player] La asignación no aparece tras map_key: no se guarda.")
        return False

    saved = unreal.EditorAssetLibrary.save_loaded_asset(imc, only_if_is_dirty=False)
    unreal.log("[IMC_Player] Guardado (%s). IA_Shell: Ctrl izquierdo y %s." % ("ok" if saved else "FALLO", KEY_NAME))
    return bool(saved)


main()

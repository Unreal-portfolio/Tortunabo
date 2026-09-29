"""Crea los materiales de la cagada de gaviota de la carrera en la playa (ATN_BeachGullZone).

Se ejecuta DENTRO del editor de Unreal (no hace falta compilar antes; el C++ carga los materiales por ruta y, si no
existen, usa un plan B):
    exec(open(r"<repo>/Scripts/create_poop_decal.py", encoding="utf-8").read())

Crea en /Game/ProcMap/Materials:
  - M_PoopSplatDecal: material de decal (dominio "Deferred Decal", mezcla "Translucent") con una salpicadura de
    cagada dibujada con código, sin texturas: mancha irregular con brazos, gotas sueltas alrededor, corazón más
    oscuro y blanco roto con un toque verdoso. Parámetros: Seed (forma de la mancha; el C++ pone uno distinto a cada
    una) y Fade (1 = fresca; baja hasta 0 y la mancha se seca desde los bordes hasta desaparecer). El C++ lo proyecta
    con un UDecalComponent enganchado al hueso de la espalda de la tortuga (va con el ragdoll y con la bola).
  - M_ProcFXHard: como M_ProcFXSoft (translúcido, sin luz, del color y el alfa del vértice por Opacity) pero SIN fundido
    por profundidad. Los avisos duros de las gaviotas (sombra de la cagada y del picado, signo de exclamación) lo
    usan: con el fundido por profundidad de M_ProcFXSoft (80 cm) un disco a 25 cm de la arena se veía a la mitad.

Idempotente: si los assets existen, se les rehace el grafo (se puede volver a ejecutar tras cambiar la forma o los
colores). Los guarda al acabar.
"""

import unreal

FOLDER = "/Game/ProcMap/Materials"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
asset_lib = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

_warnings = []


def expr(material, cls, x, y):
    return mel.create_material_expression(material, cls, x, y)


def fresh_material(name):
    path = f"{FOLDER}/{name}"
    if asset_lib.does_asset_exist(path):
        material = asset_lib.load_asset(path)
        mel.delete_all_material_expressions(material)
    else:
        asset_lib.make_directory(FOLDER)
        material = asset_tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    return material


def scalar(material, name, value, x, y):
    node = expr(material, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", value)
    return node


def set_first(obj, names, value, what):
    """Pone la primera propiedad que exista de names (el nombre en Python cambia según la versión); avisa si ninguna."""
    last = None
    for name in names:
        try:
            obj.set_editor_property(name, value)
            return True
        except Exception as error:  # noqa: BLE001 - el editor lanza excepciones de varios tipos
            last = error
    _warnings.append(f"{what}: no se pudo poner ({last}). Ponlo a mano en el editor.")
    return False


def custom_node(material, code, inputs, output_type, x, y, description, extra_outputs=()):
    """Nodo Custom con entradas (nombre, expresión) y salidas adicionales (nombre, tipo)."""
    custom = expr(material, unreal.MaterialExpressionCustom, x, y)
    custom.set_editor_property("code", code)
    custom.set_editor_property("output_type", output_type)
    custom.set_editor_property("description", description)
    ins = []
    for name, _ in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        ins.append(ci)
    custom.set_editor_property("inputs", ins)
    outs = []
    for name, otype in extra_outputs:
        co = unreal.CustomOutput()
        co.set_editor_property("output_name", name)
        co.set_editor_property("output_type", otype)
        outs.append(co)
    if outs:
        custom.set_editor_property("additional_outputs", outs)
    for name, src in inputs:
        mel.connect_material_expressions(src, "", custom, name)
    return custom


def save(asset):
    asset_lib.save_loaded_asset(asset)
    return asset


# ── Cagada: decal ────────────────────────────────────────────────────────────

POOP_HLSL = r"""
// Salpicadura de cagada de gaviota: mancha irregular con brazos de salpicadura, gotas sueltas alrededor y un corazon
// mas oscuro. UV: 0..1 en la cara del decal; Seed cambia la forma de una mancha a otra; Fade: 1 = fresca, baja hasta 0
// y la mancha se seca desde los bordes hasta desaparecer. Salidas: color base, Alpha y Rough.
struct FTNPoop
{
	float PHash(float2 P)
	{
		float3 P3 = frac(float3(P.x, P.y, P.x) * 0.1031);
		P3 += dot(P3, float3(P3.y, P3.z, P3.x) + 33.33);
		return frac((P3.x + P3.y) * P3.z);
	}
	float PNoise(float2 X)
	{
		float2 I = floor(X);
		float2 F = X - I;
		float2 U = F * F * (3.0 - 2.0 * F);
		return lerp(lerp(PHash(I), PHash(I + float2(1.0, 0.0)), U.x), lerp(PHash(I + float2(0.0, 1.0)), PHash(I + float2(1.0, 1.0)), U.x), U.y);
	}
	// Grosor en P (-1..1): 1 en el centro y 0 en el borde irregular (con brazos de salpicadura que se estiran).
	float PThick(float2 P, float S)
	{
		float R = length(P);
		float A = atan2(P.y, P.x);
		float Blob = 0.34 + 0.045 * sin(3.0 * A + S * 1.7) + 0.03 * sin(5.0 * A + S * 2.9) + 0.02 * sin(8.0 * A + S * 4.1);
		float ArmsA = pow(saturate(0.5 + 0.5 * sin(5.0 * A + S * 3.3)), 6.0) * 0.20;
		float ArmsB = pow(saturate(0.5 + 0.5 * sin(9.0 * A + S * 5.7)), 9.0) * 0.15;
		float Edge = Blob + (ArmsA + ArmsB) * (0.55 + 0.45 * PNoise(P * 5.0 + S));
		Edge += (PNoise(P * 11.0 + S * 3.0) - 0.5) * 0.05;
		return saturate((Edge - R) / max(Edge, 0.05) * 1.8);
	}
	// Gotas sueltas alrededor: 14 gotitas alargadas hacia fuera, cada una en su sitio (cobertura 0..1).
	float PDrops(float2 P, float S)
	{
		float Cov = 0.0;
		for (int I = 0; I < 14; ++I)
		{
			float Fi = float(I) + 1.0 + S * 0.37;
			float Ang = 6.2831853 * PHash(float2(Fi, 1.7));
			float Dist = lerp(0.46, 0.92, PHash(float2(Fi, 5.3)));
			float Rad = lerp(0.02, 0.07, pow(PHash(float2(Fi, 9.1)), 2.0)) * (1.25 - 0.6 * Dist);
			float2 Dir = float2(cos(Ang), sin(Ang));
			float2 Q = P - Dir * Dist;
			float Along = dot(Q, Dir) * 0.55;
			float Across = dot(Q, float2(-Dir.y, Dir.x));
			Cov = max(Cov, saturate((Rad - sqrt(Along * Along + Across * Across)) / (Rad * 0.35)));
		}
		return Cov;
	}
};
FTNPoop TN;
float2 P = (UV - 0.5) * 2.0;
float T = TN.PThick(P, Seed);
float D = TN.PDrops(P, Seed);
float Body = max(saturate(T * 4.0), D);
float Height = max(T, D * 0.5);

// Se seca: primero se van los bordes y lo fino, luego el centro; con Fade = 0 no queda nada.
float Dry = 1.0 - saturate(Fade);
float Grain = TN.PNoise(P * 9.0 + Seed * 2.0);
float Keep = saturate((Height + 0.3 * Grain + 0.5 - Dry * 1.8) * 6.0);
Alpha = Body * Keep * saturate(Fade * 6.0);

// Blanco roto con un toque verdoso (lineal), corazon oscuro y motas.
float3 White = float3(0.80, 0.86, 0.68);
float3 Dark = float3(0.10, 0.10, 0.06);
float Core = smoothstep(0.55, 0.95, T) * saturate(0.35 + 0.9 * TN.PNoise(P * 6.0 + Seed * 5.0));
float Fleck = step(0.78, TN.PNoise(P * 17.0 + Seed)) * T * 0.4;
float3 Col = lerp(White, Dark, saturate(Core * 0.55 + Fleck));
Col *= 0.9 + 0.2 * TN.PNoise(P * 13.0 + Seed);
// Lo grueso brilla (humedo) y al secarse se apaga.
Rough = lerp(0.62, 0.25, saturate(T * 1.5) * saturate(Fade * 1.2));
return Col;
"""


def build_poop_decal():
    m = fresh_material("M_PoopSplatDecal")
    # Deferred Decal + Translucent + "Decal Blend Mode: Translucent": pinta color y brillo sobre lo que toque sin cambiar
    # su normal (la normal no se conecta), con la opacidad de la mancha.
    set_first(m, ["material_domain"], unreal.MaterialDomain.MD_DEFERRED_DECAL, "Material Domain = Deferred Decal")
    set_first(m, ["blend_mode"], unreal.BlendMode.BLEND_TRANSLUCENT, "Blend Mode = Translucent")
    set_first(m, ["decal_blend_mode"], unreal.DecalBlendMode.DBM_TRANSLUCENT, "Decal Blend Mode = Translucent")

    uv = expr(m, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    uv.set_editor_property("coordinate_index", 0)
    seed = scalar(m, "Seed", 1.0, -900, 200)
    fade = scalar(m, "Fade", 1.0, -900, 320)
    custom = custom_node(m, POOP_HLSL, [("UV", uv), ("Seed", seed), ("Fade", fade)], unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                         -500, 100, "PoopSplat",
                         extra_outputs=[("Alpha", unreal.CustomMaterialOutputType.CMOT_FLOAT1),
                                        ("Rough", unreal.CustomMaterialOutputType.CMOT_FLOAT1)])
    mel.connect_material_property(custom, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(custom, "Alpha", unreal.MaterialProperty.MP_OPACITY)
    mel.connect_material_property(custom, "Rough", unreal.MaterialProperty.MP_ROUGHNESS)
    spec = expr(m, unreal.MaterialExpressionConstant, -500, 400)
    spec.set_editor_property("r", 0.4)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(m)
    save(m)
    return m


# ── Avisos duros: translúcido sin luz y sin fundido por profundidad ───────────

def build_fx_hard():
    m = fresh_material("M_ProcFXHard")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    vertex_color = expr(m, unreal.MaterialExpressionVertexColor, -700, 0)
    mel.connect_material_property(vertex_color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    opacity = scalar(m, "Opacity", 1.0, -700, 250)
    alpha = expr(m, unreal.MaterialExpressionMultiply, -450, 200)
    mel.connect_material_expressions(vertex_color, "A", alpha, "A")
    mel.connect_material_expressions(opacity, "", alpha, "B")
    mel.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(m)
    save(m)
    return m


def main():
    decal = build_poop_decal()
    hard = build_fx_hard()
    for asset in (decal, hard):
        unreal.log(f"[create_poop_decal] {asset.get_path_name()}")
    try:
        unreal.log(f"[create_poop_decal] dominio={decal.get_editor_property('material_domain')} "
                   f"mezcla={decal.get_editor_property('blend_mode')}")
    except Exception as error:  # noqa: BLE001
        unreal.log_warning(f"[create_poop_decal] no se pudo leer el dominio: {error}")
    for warning in _warnings:
        unreal.log_warning(f"[create_poop_decal] {warning}")
    if _warnings:
        unreal.log_warning("[create_poop_decal] En M_PoopSplatDecal: Material Domain = Deferred Decal, Blend Mode = Translucent, "
                           "Decal Blend Mode = Translucent (ver Detalles del material).")
    unreal.log("[create_poop_decal] Hecho. Los materiales tardan 1-2 min en compilar sus sombreadores.")


main()

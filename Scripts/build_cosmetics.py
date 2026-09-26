"""Crea el contenido de la tienda y el probador del lobby (cosméticos de verdad, no los prototipos de las estatuas).

Se ejecuta DENTRO del editor de Unreal:
    exec(open(r"<repo>/Scripts/build_cosmetics.py", encoding="utf-8").read())

Crea o rehace:
  /Game/Cosmetics/Materials
    - M_CosmeticVertexColor: cascos low-poly (color de vértice; el alfa del vértice es el brillo metálico).
    - M_TurtleBody: cuerpo y caparazón de la tortuga de demo (TotugaDemo_Rig, ranura "lambert4"). Como en esa malla
      el caparazón y el cuerpo comparten material, las zonas salen de la posición local antes del skinning:
      caparazón = detrás del torso entre la cintura y el cuello; barriga = delante del torso. Parámetros: BodyColor,
      BellyColor, BellyAmount, ShellColor, ShellColor2, ShellPattern (0 liso, 1 escamas, 2 lunares, 3 olas,
      4 estrellas, 5 grietas de lava, 6 ajedrez, 7 sandía), PatternScale, ShellShine, ShellGlow, ShellMatchBody.
    - M_TurtleHelmetSlot: ranura "lambert2" (casco rojo de serie + lengua) con el casco recortado: se usa cuando la
      tortuga lleva un casco de la tienda. Solo queda la lengua.
  /Game/UI/Shop/M_UI_Preview: pinta en la UI la captura de la vista previa (SceneColorHDR: alfa invertido y sin
    curva de tono; Exposure la ajusta).
  /Game/Cosmetics/Helmets/SM_Helmet_<Id>: los cascos de Scripts/cosmetics_meshes.py.
  DT_Helmets y DT_Skins (/Game/Blueprints/Gameplay/Cosmetics): catálogo de la tienda (precio 0 por ahora); solo
  si el C++ ya tiene las columnas nuevas de FTN_SkinData (Color, Pattern...).

Los colores se escriben en sRGB hexadecimal y se pasan a lineal.
"""

import importlib
import math
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else r"C:\Users\mokiu\Documents\Unreal Projects\Tortunabo\Scripts"
if HERE not in sys.path:
    sys.path.insert(0, HERE)
import cosmetics_meshes as CM  # noqa: E402

importlib.reload(CM)

MAT_FOLDER = "/Game/Cosmetics/Materials"
HELMET_FOLDER = "/Game/Cosmetics/Helmets"
UI_FOLDER = "/Game/UI/Shop"
DT_HELMETS = "/Game/Blueprints/Gameplay/Cosmetics/DT_Helmets"
DT_SKINS = "/Game/Blueprints/Gameplay/Cosmetics/DT_Skins"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
asset_lib = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def srgb_to_linear(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def lin_color(hex_rgb, alpha=1.0):
    r, g, b = (hex_rgb >> 16) & 255, (hex_rgb >> 8) & 255, hex_rgb & 255
    return unreal.LinearColor(srgb_to_linear(r / 255.0), srgb_to_linear(g / 255.0), srgb_to_linear(b / 255.0), alpha)


def expr(material, cls, x, y):
    return mel.create_material_expression(material, cls, x, y)


def fresh_material(folder, name):
    path = f"{folder}/{name}"
    if asset_lib.does_asset_exist(path):
        material = asset_lib.load_asset(path)
        mel.delete_all_material_expressions(material)
    else:
        asset_lib.make_directory(folder)
        material = asset_tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    return material


def scalar(material, name, value, x, y):
    s = expr(material, unreal.MaterialExpressionScalarParameter, x, y)
    s.set_editor_property("parameter_name", name)
    s.set_editor_property("default_value", value)
    return s


def vector(material, name, value, x, y):
    v = expr(material, unreal.MaterialExpressionVectorParameter, x, y)
    v.set_editor_property("parameter_name", name)
    v.set_editor_property("default_value", value)
    return v


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


# ── Materiales ───────────────────────────────────────────────────────────────

def build_vertex_color_material():
    m = fresh_material(MAT_FOLDER, "M_CosmeticVertexColor")
    vc = expr(m, unreal.MaterialExpressionVertexColor, -600, 0)
    rough = expr(m, unreal.MaterialExpressionLinearInterpolate, -300, 200)
    rough.set_editor_property("const_a", 0.62)
    rough.set_editor_property("const_b", 0.24)
    mel.connect_material_expressions(vc, "A", rough, "Alpha")
    metal = expr(m, unreal.MaterialExpressionMultiply, -300, 80)
    metal.set_editor_property("const_b", 0.95)
    mel.connect_material_expressions(vc, "A", metal, "A")
    mel.connect_material_property(vc, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    asset_lib.save_loaded_asset(m)
    return m


TURTLE_BODY_HLSL = r"""
// P: posición local de TotugaDemo_Rig antes del skinning (mira a +Y; ~53 de alto). Zonas (ver build_cosmetics.py).
float YFront = lerp(0.4, 3.2, saturate((P.z - 29.0) / 9.0));
float ShellM = step(P.y, YFront) * step(22.3, P.z) * step(P.z, 38.4) * step(abs(P.x), 7.2);
float BellyM = smoothstep(2.0, 3.4, P.y) * smoothstep(21.0, 23.0, P.z) * smoothstep(36.6, 34.8, P.z) * smoothstep(6.2, 4.6, abs(P.x));
float3 BodyCol = lerp(BodyColor, BellyColor, BellyM * BellyAmount);

// Dibujo del caparazón visto desde atrás (X, Z), centrado en el lomo.
float2 Q = float2(P.x, P.z - 30.0) / max(PatternScale, 0.05);
int Mode = (int)round(ShellPattern);
float Pat = 0.0;
float Relief = 1.0;
if (Mode == 1 || Mode == 2)
{
    // Panal de hexágonos: escamas (juntas) o lunares (centros).
    float S = Mode == 1 ? 4.2 : 3.1;
    float2 R = float2(1.0, 1.7320508) * S;
    float2 H = R * 0.5;
    float2 A = (frac(Q / R) - 0.5) * R;
    float2 B = (frac((Q - H) / R) - 0.5) * R;
    float2 G = dot(A, A) < dot(B, B) ? A : B;
    float2 AG = abs(G);
    float HexD = max(dot(AG, float2(0.5, 0.8660254)), AG.x) / S;
    if (Mode == 1)
    {
        Pat = smoothstep(0.40, 0.46, HexD);
        Relief = lerp(1.12, 0.88, saturate(HexD * 2.0));
    }
    else
    {
        Pat = 1.0 - smoothstep(0.24, 0.29, length(G) / S);
    }
}
else if (Mode == 3)
{
    // Olas: franjas onduladas de espuma.
    float W = Q.y + 0.7 * sin(Q.x * 0.9);
    float F = frac(W / 3.4);
    Pat = smoothstep(0.06, 0.14, F) * smoothstep(0.46, 0.38, F);
}
else if (Mode == 4)
{
    // Galaxia: estrellitas sueltas y nebulosa suave.
    float2 Cell = floor(Q / 1.7);
    float2 Rnd = frac(sin(float2(dot(Cell, float2(127.1, 311.7)), dot(Cell, float2(269.5, 183.3)))) * 43758.5453);
    float2 Loc = frac(Q / 1.7) - 0.5 - (Rnd - 0.5) * 0.55;
    float Size = lerp(0.07, 0.2, Rnd.x) * step(0.42, Rnd.y);
    Pat = 1.0 - smoothstep(Size * 0.5, Size, length(Loc));
    Relief = 0.85 + 0.35 * (0.5 + 0.5 * sin(Q.x * 0.7 + 1.3) * sin(Q.y * 0.6));
}
else if (Mode == 5)
{
    // Lava: grietas de Voronoi que brillan.
    float2 G2 = Q / 2.6;
    float2 Base = floor(G2);
    float F1 = 8.0, F2 = 8.0;
    [unroll] for (int j = -1; j <= 1; ++j)
    {
        [unroll] for (int i = -1; i <= 1; ++i)
        {
            float2 C = Base + float2(i, j);
            float2 Rn = frac(sin(float2(dot(C, float2(127.1, 311.7)), dot(C, float2(269.5, 183.3)))) * 43758.5453);
            float D = length(G2 - C - Rn);
            if (D < F1) { F2 = F1; F1 = D; } else if (D < F2) { F2 = D; }
        }
    }
    Pat = 1.0 - smoothstep(0.04, 0.12, F2 - F1);
    Relief = lerp(0.8, 1.1, saturate(F1));
}
else if (Mode == 6)
{
    // Ajedrez.
    float2 K = floor(Q / 2.4);
    Pat = abs(fmod(K.x + K.y, 2.0));
}
else if (Mode == 7)
{
    // Sandía: franjas verticales temblonas.
    float F = frac((Q.x + 0.45 * sin(Q.y * 1.1)) / 2.8);
    Pat = smoothstep(0.08, 0.16, F) * smoothstep(0.56, 0.48, F);
}

float3 C1 = lerp(ShellColor, BodyColor * 0.72, ShellMatchBody) * Relief;
float3 ShellCol = lerp(C1, ShellColor2, Pat);
Metal = ShellM * ShellShine;
Rough = lerp(0.62, lerp(0.5, 0.2, ShellShine), ShellM);
float Twinkle = 0.8 + 0.2 * sin(TimeS * 2.3 + Q.x * 1.7 + Q.y);
Emis = ShellM * ShellGlow * Pat * ShellColor2 * Twinkle;
return lerp(BodyCol, ShellCol, ShellM);
"""


def build_turtle_body_material():
    m = fresh_material(MAT_FOLDER, "M_TurtleBody")
    pre = expr(m, unreal.MaterialExpressionPreSkinnedPosition, -1300, -200)
    interp = expr(m, unreal.MaterialExpressionVertexInterpolator, -1050, -200)
    mel.connect_material_expressions(pre, "", interp, "")
    green = 0x3A9A3F
    inputs = [
        ("P", interp),
        ("BodyColor", vector(m, "BodyColor", lin_color(green), -1100, 0)),
        ("BellyColor", vector(m, "BellyColor", lin_color(0xF3E3A6), -1100, 120)),
        ("BellyAmount", scalar(m, "BellyAmount", 0.0, -1100, 240)),
        ("ShellColor", vector(m, "ShellColor", lin_color(0x2F7A34), -1100, 320)),
        ("ShellColor2", vector(m, "ShellColor2", lin_color(0x1F5424), -1100, 440)),
        ("ShellPattern", scalar(m, "ShellPattern", 0.0, -1100, 560)),
        ("PatternScale", scalar(m, "PatternScale", 1.0, -1100, 640)),
        ("ShellShine", scalar(m, "ShellShine", 0.0, -1100, 720)),
        ("ShellGlow", scalar(m, "ShellGlow", 0.0, -1100, 800)),
        ("ShellMatchBody", scalar(m, "ShellMatchBody", 1.0, -1100, 880)),
        ("TimeS", expr(m, unreal.MaterialExpressionTime, -1100, 960)),
    ]
    custom = custom_node(m, TURTLE_BODY_HLSL, inputs, unreal.CustomMaterialOutputType.CMOT_FLOAT3, -600, 200, "TurtleBody",
                         extra_outputs=[("Metal", unreal.CustomMaterialOutputType.CMOT_FLOAT1),
                                        ("Rough", unreal.CustomMaterialOutputType.CMOT_FLOAT1),
                                        ("Emis", unreal.CustomMaterialOutputType.CMOT_FLOAT3)])
    mel.connect_material_property(custom, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(custom, "Metal", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(custom, "Rough", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(custom, "Emis", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    m.set_editor_property("used_with_skeletal_mesh", True)
    mel.recompile_material(m)
    asset_lib.save_loaded_asset(m)
    return m


HELMET_SLOT_HLSL = r"""
// Ranura del casco de serie de TotugaDemo_Rig: solo queda la lengua (delante de la boca).
float Tongue = step(abs(P.x), 2.3) * step(9.6, P.y) * step(P.z, 43.4) * step(39.8, P.z);
return lerp(1.0, Tongue, HideHelmet);
"""


def build_helmet_slot_material():
    m = fresh_material(MAT_FOLDER, "M_TurtleHelmetSlot")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    pre = expr(m, unreal.MaterialExpressionPreSkinnedPosition, -1000, 200)
    interp = expr(m, unreal.MaterialExpressionVertexInterpolator, -760, 200)
    mel.connect_material_expressions(pre, "", interp, "")
    hide = scalar(m, "HideHelmet", 1.0, -760, 320)
    custom = custom_node(m, HELMET_SLOT_HLSL, [("P", interp), ("HideHelmet", hide)], unreal.CustomMaterialOutputType.CMOT_FLOAT1,
                         -450, 220, "HelmetSlotMask")
    color = vector(m, "TongueColor", unreal.LinearColor(0.0835, 0.0178, 0.0004, 1.0), -450, 0)
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(custom, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    rough = expr(m, unreal.MaterialExpressionConstant, -450, 120)
    rough.set_editor_property("r", 0.55)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    m.set_editor_property("used_with_skeletal_mesh", True)
    mel.recompile_material(m)
    asset_lib.save_loaded_asset(m)
    return m


PREVIEW_HLSL = r"""
// Captura SceneColorHDR: color lineal sin curva de tono y alfa = 1 - cobertura.
float4 S = Capture;
float3 X = max(S.rgb * Exposure, 0.0);
float3 Mapped = saturate((X * (2.51 * X + 0.03)) / (X * (2.43 * X + 0.59) + 0.14));
float Cover = saturate(1.0 - S.a);
return float4(Mapped, Cover);
"""


def build_preview_ui_material():
    m = fresh_material(UI_FOLDER, "M_UI_Preview")
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    tex = expr(m, unreal.MaterialExpressionTextureSampleParameter2D, -900, 0)
    tex.set_editor_property("parameter_name", "Capture")
    tex.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/DefaultTexture"))
    tex.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    expo = scalar(m, "Exposure", 1.0, -900, 260)
    # El Custom recibe el float4 del sampler (RGBA).
    append = expr(m, unreal.MaterialExpressionAppendVector, -650, 0)
    mel.connect_material_expressions(tex, "RGB", append, "A")
    mel.connect_material_expressions(tex, "A", append, "B")
    custom = custom_node(m, PREVIEW_HLSL, [("Capture", append), ("Exposure", expo)], unreal.CustomMaterialOutputType.CMOT_FLOAT4,
                         -420, 60, "PreviewCapture")
    rgb = expr(m, unreal.MaterialExpressionComponentMask, -200, 20)
    for c, v in (("r", True), ("g", True), ("b", True), ("a", False)):
        rgb.set_editor_property(c, v)
    mel.connect_material_expressions(custom, "", rgb, "")
    alpha = expr(m, unreal.MaterialExpressionComponentMask, -200, 160)
    for c, v in (("r", False), ("g", False), ("b", False), ("a", True)):
        alpha.set_editor_property(c, v)
    mel.connect_material_expressions(custom, "", alpha, "")
    mel.connect_material_property(rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(m)
    asset_lib.save_loaded_asset(m)
    return m


# ── Cascos ───────────────────────────────────────────────────────────────────

def helmet_buffers(mesh):
    """Triángulos de cosmetics_meshes a buffers de GeometryScript (caras planas, color lineal dos veces)."""
    buf = unreal.GeometryScriptSimpleMeshBuffers()
    verts, normals, colors, tris, uvs = [], [], [], [], []
    for (a, b, c, col, shine) in mesh.tris:
        n = CM.norm(CM.cross(CM.sub(b, a), CM.sub(c, a)))
        r, g, bb = (col >> 16) & 255, (col >> 8) & 255, col & 255
        # El color de vértice del StaticMesh se guarda en sRGB y el nodo VertexColor lo lee tal cual: se decodifica
        # dos veces para que el material reciba el color lineal de la paleta (ver reference_ue_runtime_mesh).
        lc = unreal.LinearColor(srgb_to_linear(srgb_to_linear(r / 255.0)), srgb_to_linear(srgb_to_linear(g / 255.0)),
                                srgb_to_linear(srgb_to_linear(bb / 255.0)), float(shine))
        base = len(verts)
        for p in (a, b, c):
            verts.append(unreal.Vector(p[0], p[1], p[2]))
            normals.append(unreal.Vector(n[0], n[1], n[2]))
            colors.append(lc)
            uvs.append(unreal.Vector2D(p[0] / 20.0, p[2] / 20.0))
        # UE pinta la cara cuya normal es (C-A)x(B-A) (ver TN_ProcMapMeshKit.h): cosmetics_meshes orienta
        # (B-A)x(C-A) hacia fuera, así que se emite A, C, B. Si no, los cascos se ven del revés (huecos desde arriba).
        tris.append(unreal.IntVector(base, base + 2, base + 1))
    buf.set_editor_property("vertices", verts)
    buf.set_editor_property("normals", normals)
    buf.set_editor_property("vertex_colors", colors)
    buf.set_editor_property("uv0", uvs)
    buf.set_editor_property("triangles", tris)
    return buf


def build_helmet_meshes(material):
    asset_lib.make_directory(HELMET_FOLDER)
    out = {}
    for sid, fn, _, _ in CM.HELMETS:
        mesh = fn()
        dm = unreal.DynamicMesh()
        unreal.GeometryScript_MeshEdits.append_buffers_to_mesh(dm, helmet_buffers(mesh), 0)
        path = f"{HELMET_FOLDER}/SM_Helmet_{sid}"
        if asset_lib.does_asset_exist(path):
            sm = asset_lib.load_asset(path)
            opts = unreal.GeometryScriptCopyMeshToAssetOptions()
            opts.set_editor_property("enable_recompute_normals", False)
            opts.set_editor_property("enable_recompute_tangents", True)
            opts.set_editor_property("replace_materials", True)
            opts.set_editor_property("new_materials", [material])
            opts.set_editor_property("new_material_slot_names", ["Helmet"])
            unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(dm, sm, opts, unreal.GeometryScriptMeshWriteLOD())
        else:
            opts = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
            opts.set_editor_property("enable_recompute_normals", False)
            opts.set_editor_property("enable_recompute_tangents", True)
            opts.set_editor_property("enable_collision", False)
            opts.set_editor_property("enable_nanite", False)
            sm, _outcome = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dm, path, opts)
            sm.set_material(0, material)
        asset_lib.save_loaded_asset(sm)
        out[sid] = sm
        unreal.log(f"[Cosméticos] {path}: {len(mesh.tris)} triángulos")
    return out


# ── Catálogo (DT_Helmets y DT_Skins) ─────────────────────────────────────────

def lin_json(hex_rgb):
    c = lin_color(hex_rgb)
    return {"R": round(c.r, 5), "G": round(c.g, 5), "B": round(c.b, 5), "A": 1.0}


# (id, nombre, color principal, segundo color, dibujo, escala del dibujo, brillo, luz propia, lo que dice el tendero)
SHELLS = [
    ("Scutes", "Escamas clásicas", 0xC8A165, 0x5E3F22, "Scutes", 1.0, 0.0, 0.0, "Hexágonos color miel con juntas de chocolate. El caparazón de tortuga por excelencia."),
    ("Coral", "Coral con lunares", 0xFF7A5C, 0xFFF1DC, "Spots", 1.0, 0.0, 0.0, "Coral del arrecife con lunares de espuma. Alegre y veraniego."),
    ("Waves", "Oleaje", 0x1E5FA8, 0xE8FBFF, "Waves", 1.0, 0.0, 0.0, "Olas que rompen en tu espalda. Para las tortugas más marineras."),
    ("Gold", "Oro pirata", 0xFFC93C, 0xB8860B, "Scutes", 1.0, 1.0, 0.0, "Brilla más que el tesoro de un galeón. Cuidado con las gaviotas."),
    ("Lava", "Volcán", 0x2A2626, 0xFF7A1A, "Lava", 1.0, 0.0, 3.0, "Roca volcánica con grietas que brillan. Calentito, calentito."),
    ("Galaxy", "Galaxia", 0x1A1F4D, 0xFFF3B0, "Stars", 1.0, 0.0, 4.0, "Un cielo estrellado para las noches de carrera."),
    ("Melon", "Sandía", 0x8FD16A, 0x2E7D32, "Melon", 1.0, 0.0, 0.0, "Rayas de sandía fresquita. Nadie se lo va a comer, tranquilidad."),
    ("Checker", "Tablero", 0xF4EFE2, 0x26232E, "Checker", 1.0, 0.0, 0.0, "Cuadros blancos y negros, como la bandera de meta."),
    ("Moss", "Musgo", 0x5E8C3A, 0x9CCB5E, "Spots", 0.7, 0.0, 0.0, "Musgo de la selva. Camuflaje perfecto entre los helechos."),
    ("Candy", "Algodón de azúcar", 0xFF9EC8, 0xFFFFFF, "Waves", 0.8, 0.0, 0.0, "Rosa de feria con remolinos de nube. Dulce, dulce."),
]

# (id, nombre, color, barriga, cuánto se nota la barriga, lo que dice el tendero)
BODIES = [
    ("Ocean", "Azul océano", 0x3A8FD9, 0xE8F4FF, 0.45, "Del color del mar abierto. Te camuflas al nadar."),
    ("Bubblegum", "Rosa chicle", 0xF28DB2, 0xFFF0F5, 0.45, "Rosa chicle con la barriga de nata. Muy dulce."),
    ("Lavender", "Lavanda", 0x9B6BD6, 0xF1E6FF, 0.45, "Morado lavanda: huele a playa tranquila."),
    ("Sunny", "Amarillo sol", 0xF4C542, 0xFFF8DC, 0.45, "Amarillo como el sol de mediodía. Se te ve desde lejos."),
    ("Coral", "Rojo coral", 0xE8574A, 0xFFE3D6, 0.45, "Rojo coral, el color de los valientes del arrecife."),
    ("Mint", "Menta", 0x7FE0C0, 0xF0FFF8, 0.4, "Menta fresquita para los días de calor."),
    ("Orange", "Naranja", 0xF28C38, 0xFFEBD2, 0.45, "Naranja atardecer. Combina con todo."),
    ("Snow", "Blanco nieve", 0xEDEFF2, 0xFFFFFF, 0.3, "Blanco nieve. Muy elegante, pero no te revuelques en la arena."),
    ("Charcoal", "Carbón", 0x3B3F4A, 0xB8BEC8, 0.4, "Gris carbón con la barriga plateada. Misteriosa."),
    ("Sand", "Arena", 0xE3C79A, 0xFFF5E1, 0.45, "Color arena de playa. Es el que llevo yo, ¿se nota?"),
    ("Lime", "Lima", 0x9ED94B, 0xF6FFE0, 0.4, "Verde lima, ácido y veloz."),
    ("Forest", "Verde bosque", 0x2E6B3A, 0xD8E8B0, 0.45, "Verde oscuro de la selva profunda."),
]


def fill_tables():
    """Rehace las filas de DT_Helmets y DT_Skins (todo a precio 0 hasta que llegue la economía)."""
    import json
    helmets = []
    for sid, _fn, name, desc in CM.HELMETS:
        row = "Helmet_" + sid
        helmets.append({
            "Name": row, "HelmetId": row, "DisplayName": name,
            "DisplayMesh": f"{HELMET_FOLDER}/SM_Helmet_{sid}.SM_Helmet_{sid}",
            "Icon": "None",
            "MeshScale": {"X": 1.0, "Y": 1.0, "Z": 1.0},
            "MeshOffset": {"X": 0.0, "Y": 0.0, "Z": 0.0},
            "MeshRotation": {"Pitch": 0.0, "Yaw": 0.0, "Roll": 0.0},
            "Price": 0, "Description": desc,
        })
    # Todas las columnas, también las vacías: si falta alguna, el importador abre un diálogo modal con avisos que
    # bloquea el editor (y la ejecución remota de Python) hasta que alguien pulsa OK.
    empty_slots = {"BellyMaterial": "None", "EyeShineMaterial": "None", "EyesMouthMaterial": "None", "SkinMaterial": "None",
                   "ShellMaterial": "None", "Icon": "None"}
    # Las columnas de materiales por ranura (malla unificada) van vacías a propósito: si faltan en el JSON, el
    # importador avisa con un diálogo modal que bloquea el editor.
    empty_slots = {"BellyMaterial": "None", "EyeShineMaterial": "None", "EyesMouthMaterial": "None",
                   "SkinMaterial": "None", "ShellMaterial": "None", "Icon": "None"}
    skins = []
    for sid, name, c1, c2, pattern, scale, shine, glow, desc in SHELLS:
        row = "Shell_" + sid
        skins.append({
            "Name": row, "SkinId": row, "DisplayName": name, "Category": "Shell", "Price": 0, "Description": desc,
            "Color": lin_json(c1), "Color2": lin_json(c2), "BellyAmount": 0.0, "Pattern": pattern,
            "PatternScale": scale, "Shine": shine, "Glow": glow, **empty_slots,
        })
    for sid, name, c1, c2, belly, desc in BODIES:
        row = "Body_" + sid
        skins.append({
            "Name": row, "SkinId": row, "DisplayName": name, "Category": "Body", "Price": 0, "Description": desc,
            "Color": lin_json(c1), "Color2": lin_json(c2), "BellyAmount": belly, "Pattern": "Plain",
            "PatternScale": 1.0, "Shine": 0.0, "Glow": 0.0, **empty_slots,
        })
    for path, rows in ((DT_HELMETS, helmets), (DT_SKINS, skins)):
        dt = asset_lib.load_asset(path)
        ok = unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(dt, json.dumps(rows, ensure_ascii=False))
        asset_lib.save_loaded_asset(dt)
        unreal.log(f"[Cosméticos] {path}: {len(rows)} filas ({'bien' if ok else 'ERROR'})")


def tables_ready():
    """Las columnas nuevas (Color, Pattern...) existen si el módulo C++ ya está compilado con ellas."""
    try:
        return hasattr(unreal.TN_SkinData(), "pattern")
    except Exception:
        return False


def main():
    vc = build_vertex_color_material()
    build_turtle_body_material()
    build_helmet_slot_material()
    build_preview_ui_material()
    build_helmet_meshes(vc)
    if tables_ready():
        fill_tables()
    else:
        unreal.log_warning("[Cosméticos] Falta compilar el C++ con las columnas nuevas de FTN_SkinData: tablas sin tocar.")
    unreal.log("[Cosméticos] Listo.")


if __name__ == "__main__":
    main()

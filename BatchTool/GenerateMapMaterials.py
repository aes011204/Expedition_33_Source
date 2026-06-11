"""
GenerateMapMaterials.py

Reads MI JSON files from a _MI folder and generates:
  1. Resources/Materials/[LevelName].material.json  (all MI instances in one bundle)
  2. NeedTextureJson/[LevelName]_textures.txt       (texture extraction list for fmodel)
  3. NeedTextureJson/[LevelName]_missing.txt        (textures referenced but not on disk)

Texture path format inside the JSON:
  Map/[LevelName]/Textures/[TextureName].png
  Map/[LevelName]/Textures/Normal/[TextureName].png  (for normal-channel textures)
Engine resolves this as: Resources/ + above path.

ORM cases detected automatically from texture name suffixes:
  _NMAo  = Normal(RG) + Metallic(B) + AO(A)
  _NAo   = Normal(RG) + AO(A)
  _AR    = Albedo(RGB) + Roughness(A)
  other  = standard ORM (R=AO, G=Roughness, B=Metallic)

Usage:
    python GenerateMapMaterials.py --level-dir SmallLevel_SimonArea
    python GenerateMapMaterials.py --level-dir SmallLevel_SimonArea --overwrite
"""

import argparse
import json
from pathlib import Path


# ---------------------------------------------------------------------------
# Channel source constants (Engine_Shader_Defines.hlsli)
# ---------------------------------------------------------------------------
CHAN_CONST0 = 0   # Always 0.0
CHAN_CONST1 = 1   # Always 1.0
CHAN_R = 2
CHAN_G = 3
CHAN_B = 4
CHAN_A = 5


# ---------------------------------------------------------------------------
# Slot candidate tables
# ---------------------------------------------------------------------------
SLOT_CANDIDATES: dict[str, list[str]] = {
    "baseColor":   ["Texture1_AlbedoAlpha", "Texture1_AlbedoRoughness", "BaseLayer_AlbedoRoughness",
                    "1stLayer_AlbedoRoughness", "Albedo", "PM_Diffuse"],
    "normal":      ["PM_Normals", "Normal Map", "Texture2_NormalRoughEmi", "Texture2_Normal+MEHAo"],
    "orm":         ["ORM Map", "PM_SpecularMasks", "Grunge_Texture", "Roughness"],
    "metallic":    ["Metallic Map", "MetallicMap", "Metallic"],
    "emissive":    [],
    "opacityMask": ["Opacity"],
}

AR_BASECOLOR_KEYS = {
    "texture1_albedoroughness",
    "baselayer_albedoroughness",
    "1stlayer_albedoroughness",
}

SCALAR_TO_CB: dict[str, str] = {
    "normal intensity": "normalStrength",
    "roughness":        "roughnessScale",
    "metalness":        "metalnessScale",
    "ao scale":         "aoScale",
}

DEFAULT_CB = {
    "normalStrength":        1.0,
    "roughnessScale":        1.0,
    "metalnessScale":        1.0,
    "aoScale":               1.0,
    "opacityMaskClipValue":  0.3333,
    "emissiveIntensity":     0.0,
    "emissiveMaskMul":       0.0,
    "emissiveColor":         [1.0, 1.0, 1.0, 0.0],
    "baseColorTint":         [1.0, 1.0, 1.0, 1.0],
    "uvScale":               [1.0, 1.0],
    "uvOffset":              [0.0, 0.0],
}

TEMPLATES = [
    {"id": 0, "name": "OpaquePBR",      "supportsBaseColor": True, "supportsNormal": True,
     "supportsORM": True, "supportsEmissive": True, "supportsOpacityMask": False},
    {"id": 1, "name": "MaskedPBR",      "supportsBaseColor": True, "supportsNormal": True,
     "supportsORM": True, "supportsEmissive": True, "supportsOpacityMask": True},
    {"id": 2, "name": "EmissivePBR",    "supportsBaseColor": True, "supportsNormal": True,
     "supportsORM": True, "supportsEmissive": True, "supportsOpacityMask": False},
    {"id": 3, "name": "TransparentPBR", "supportsBaseColor": True, "supportsNormal": True,
     "supportsORM": True, "supportsEmissive": True, "supportsOpacityMask": False},
]


# ---------------------------------------------------------------------------
# JSON / UE path utilities
# ---------------------------------------------------------------------------

def load_json(path: Path) -> dict | list:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def normalize_mi_data(mi_data: dict | list) -> dict:
    """Convert raw UE export list format to the expected dict format.

    UE raw export: [{"Type": "MaterialInstanceConstant", "Properties": {...}}]
    Expected:      {"Textures": {...}, "Parameters": {...}}
    """
    if isinstance(mi_data, dict):
        return mi_data

    raw = mi_data[0] if mi_data else {}
    props = raw.get("Properties", {})

    textures: dict[str, str] = {}
    for tv in props.get("TextureParameterValues", []):
        name = tv.get("ParameterInfo", {}).get("Name", "")
        obj = tv.get("ParameterValue", {})
        obj_path = obj.get("ObjectPath", "")
        obj_name_raw = obj.get("ObjectName", "")
        asset_name = obj_name_raw.split("'")[1] if "'" in obj_name_raw else ""
        if name and obj_path and asset_name:
            base_path = obj_path.rsplit(".", 1)[0]
            textures[name] = f"{base_path}.{asset_name}"

    scalars: dict[str, float] = {}
    for sv in props.get("ScalarParameterValues", []):
        name = sv.get("ParameterInfo", {}).get("Name", "")
        if name:
            scalars[name] = float(sv.get("ParameterValue", 0.0))

    colors: dict[str, dict] = {}
    for vv in props.get("VectorParameterValues", []):
        name = vv.get("ParameterInfo", {}).get("Name", "")
        if name:
            colors[name] = vv.get("ParameterValue", {})

    return {
        "Textures": textures,
        "Parameters": {
            "BlendMode": 0,
            "Scalars": scalars,
            "Colors": colors,
            "Properties": {
                "BasePropertyOverrides": props.get("BasePropertyOverrides", {}),
            },
        },
    }


def ue_path_to_texture_name(ue_path: str) -> str:
    """'/Game/Meshes/Manor/T_Candle.T_Candle' -> 'T_Candle'"""
    if not ue_path:
        return ""
    return ue_path.rsplit(".", 1)[-1]


def pick_texture_slot(textures: dict, candidates: list[str]) -> tuple[str, str]:
    """Returns (texture_name, ue_path) for the first matching candidate key."""
    for key in candidates:
        for param_name, ue_path in textures.items():
            if param_name.lower() == key.lower():
                name = ue_path_to_texture_name(ue_path)
                if name:
                    return name, ue_path
    return "", ""


def is_albedo_roughness_source(textures: dict, base_color_ue_path: str) -> bool:
    if not base_color_ue_path:
        return False
    for param_name, ue_path in textures.items():
        if ue_path != base_color_ue_path:
            continue
        if param_name.lower() in AR_BASECOLOR_KEYS:
            return True
    tex_name = ue_path_to_texture_name(base_color_ue_path).lower()
    return "roughness" in tex_name or tex_name.endswith("_ar")


# ---------------------------------------------------------------------------
# ORM case detection
# ---------------------------------------------------------------------------

def is_packed_normal_tex(tex_name: str) -> bool:
    """NMAo and NAo textures contain packed normal data and go in the Normal/ subfolder."""
    name_lower = tex_name.lower()
    return name_lower.endswith("_nmao") or name_lower.endswith("_nao")


def classify_orm_case(orm_name: str, ar_name: str, metallic_name: str) -> str:
    """
    Returns one of:
      'NMAo+AR'         Case B — NMAo (AO+Metallic in B/A) + AR (Roughness in A)
      'NMAo'            Case C — NMAo only, no separate AR
      'NAo+AR+Metallic' Case D — NAo (AO in A) + AR (Roughness in A) + separate Metallic
      'NAo+AR'          Case E — NAo + AR, no Metallic
      'AR'              Case F — AR-only base color, no dedicated ORM texture
      'ORM'             Case A — standard ORM (R=AO, G=Roughness, B=Metallic)
      'none'            Case G — no PBR data
    """
    name_lower = orm_name.lower() if orm_name else ""
    is_nmao = name_lower.endswith("_nmao")
    is_nao  = name_lower.endswith("_nao")

    if is_nmao and ar_name:
        return "NMAo+AR"
    if is_nmao:
        return "NMAo"
    if is_nao and ar_name and metallic_name:
        return "NAo+AR+Metallic"
    if is_nao and ar_name:
        return "NAo+AR"
    if not orm_name and ar_name:
        return "AR"
    if orm_name:
        return "ORM"
    return "none"


def get_orm_cb(orm_case: str) -> dict:
    """Returns CB channel-source overrides for the given ORM case.
    Only values that differ from shader defaults are strictly needed,
    but all relevant fields are written for clarity.
    Shader defaults: ormXxxSourceSlot=0, ormAoSource=R, ormRoughnessSource=G, ormMetalnessSource=B
    """
    if orm_case == "NMAo+AR":
        # NMAo slot: AO=A, Metallic=B  |  AR slot (orm2): Roughness=A
        return {
            "ormAoSourceSlot":        0, "ormAoSource":        CHAN_A,
            "ormRoughnessSourceSlot": 1, "ormRoughnessSource": CHAN_A,
            "ormMetalnessSourceSlot": 0, "ormMetalnessSource": CHAN_B,
        }
    if orm_case == "NMAo":
        # NMAo slot: AO=A, Metallic=B, Roughness=constant 1
        return {
            "ormAoSource":        CHAN_A,
            "ormRoughnessSource": CHAN_CONST1,
            "ormMetalnessSource": CHAN_B,
        }
    if orm_case == "NAo+AR+Metallic":
        # NAo slot: AO=A  |  AR slot (orm2): Roughness=A  |  Metallic slot (orm3): R
        return {
            "ormAoSourceSlot":        0, "ormAoSource":        CHAN_A,
            "ormRoughnessSourceSlot": 1, "ormRoughnessSource": CHAN_A,
            "ormMetalnessSourceSlot": 2, "ormMetalnessSource": CHAN_R,
        }
    if orm_case == "NAo+AR":
        # NAo slot: AO=A  |  AR slot (orm2): Roughness=A  |  No Metallic
        return {
            "ormAoSourceSlot":        0, "ormAoSource":        CHAN_A,
            "ormRoughnessSourceSlot": 1, "ormRoughnessSource": CHAN_A,
            "ormMetalnessSourceSlot": 0, "ormMetalnessSource": CHAN_CONST0,
            "metalnessScale": 0.0,
        }
    if orm_case == "AR":
        # AR is the only texture: AO=constant 1, Roughness=A, no Metallic
        return {
            "ormAoSource":        CHAN_CONST1,
            "ormRoughnessSource": CHAN_A,
            "ormMetalnessSource": CHAN_CONST0,
            "metalnessScale": 0.0,
        }
    # 'ORM' and 'none': standard R/G/B channels — no overrides needed
    return {}


def build_orm_slots(
    orm_case: str,
    orm_name: str, orm_ue: str,
    ar_name: str,  ar_ue: str,
    metallic_name: str, metallic_ue: str,
    level_name: str,
) -> dict:
    """Returns {slot: {path, override, ref?}} for orm / orm2 / orm3."""
    def tex_entry(name: str, ue_path: str) -> dict:
        return {
            "path":     f"Map/{level_name}/Textures/{name}.png",
            "override": True,
            "ref":      (name, ue_path),
        }
    empty = {"path": "", "override": False}

    result = {"orm": empty, "orm2": empty, "orm3": empty}

    if orm_case in ("ORM", "NMAo", "NMAo+AR", "NAo+AR", "NAo+AR+Metallic"):
        result["orm"] = tex_entry(orm_name, orm_ue)
        if orm_case in ("NMAo+AR", "NAo+AR", "NAo+AR+Metallic"):
            result["orm2"] = tex_entry(ar_name, ar_ue)
        if orm_case == "NAo+AR+Metallic":
            result["orm3"] = tex_entry(metallic_name, metallic_ue)
    elif orm_case == "AR":
        result["orm"] = tex_entry(ar_name, ar_ue)

    return result


# ---------------------------------------------------------------------------
# Render state / CB parsing
# ---------------------------------------------------------------------------

def parse_blend_mode(mi_data: dict) -> int:
    """Returns 0 (Opaque) or 1 (Masked)."""
    overrides = mi_data.get("Parameters", {}).get("Properties", {}).get("BasePropertyOverrides", {})
    if "BLEND_Masked" in overrides.get("BlendMode", ""):
        return 1
    if mi_data.get("Parameters", {}).get("BlendMode", 0) == 1:
        return 1
    return 0


def parse_render_state(mi_data: dict) -> dict:
    overrides = mi_data.get("Parameters", {}).get("Properties", {}).get("BasePropertyOverrides", {})
    return {
        "blendMode":             parse_blend_mode(mi_data),
        "twoSided":              overrides.get("TwoSided", False),
        "opacityMaskClipValue":  overrides.get("OpacityMaskClipValue", 0.3333),
    }


def parse_cb(mi_data: dict) -> dict:
    cb = dict(DEFAULT_CB)
    for param_name, value in mi_data.get("Parameters", {}).get("Scalars", {}).items():
        cb_key = SCALAR_TO_CB.get(param_name.lower())
        if cb_key:
            cb[cb_key] = float(value)
    overrides = mi_data.get("Parameters", {}).get("Properties", {}).get("BasePropertyOverrides", {})
    if "OpacityMaskClipValue" in overrides:
        cb["opacityMaskClipValue"] = overrides["OpacityMaskClipValue"]
    for param_name, color_val in mi_data.get("Parameters", {}).get("Colors", {}).items():
        key = param_name.lower()
        if key == "base tiling/offset":
            cb["uvScale"]  = [color_val.get("R", 1.0), color_val.get("G", 1.0)]
            cb["uvOffset"] = [color_val.get("B", 0.0), color_val.get("A", 0.0)]
        elif key == "albedo tint":
            cb["baseColorTint"] = [
                color_val.get("R", 1.0), color_val.get("G", 1.0),
                color_val.get("B", 1.0), color_val.get("A", 1.0),
            ]
    return cb


# ---------------------------------------------------------------------------
# Instance builder
# ---------------------------------------------------------------------------

def build_instance(mi_name: str, mi_data: dict, level_name: str) -> tuple[dict, dict]:
    """Returns (instance_entry, texture_refs).
    texture_refs: slot -> (texture_name, ue_path)
    """
    textures_raw: dict = mi_data.get("Textures", {})
    render_state = parse_render_state(mi_data)
    cb = parse_cb(mi_data)

    # Pick raw textures
    base_name,     base_ue     = pick_texture_slot(textures_raw, SLOT_CANDIDATES["baseColor"])
    normal_name,   normal_ue   = pick_texture_slot(textures_raw, SLOT_CANDIDATES["normal"])
    orm_name,      orm_ue      = pick_texture_slot(textures_raw, SLOT_CANDIDATES["orm"])
    metallic_name, metallic_ue = pick_texture_slot(textures_raw, SLOT_CANDIDATES["metallic"])
    opacity_name,  opacity_ue  = pick_texture_slot(textures_raw, SLOT_CANDIDATES["opacityMask"])

    # Determine AR base color
    is_ar_base = is_albedo_roughness_source(textures_raw, base_ue)
    ar_name = base_name if is_ar_base else ""
    ar_ue   = base_ue   if is_ar_base else ""

    # If no explicit normal but ORM candidate is a packed normal, use it for the normal slot
    if not normal_name and is_packed_normal_tex(orm_name):
        normal_name = orm_name
        normal_ue   = orm_ue

    orm_case = classify_orm_case(orm_name, ar_name, metallic_name)

    # Build texture slots
    tex_out: dict[str, str] = {}
    overrides: dict[str, bool] = {}
    tex_refs: dict[str, tuple[str, str]] = {}

    def add_tex(slot: str, name: str, ue_path: str, path: str = "") -> None:
        if name:
            tex_out[slot]  = path or f"Map/{level_name}/Textures/{name}.png"
            overrides[slot] = True
            tex_refs[slot]  = (name, ue_path)
        else:
            tex_out[slot]   = ""
            overrides[slot] = False

    add_tex("baseColor", base_name, base_ue)
    # Normal textures live in the Normal/ subfolder
    add_tex("normal", normal_name, normal_ue,
            f"Map/{level_name}/Textures/Normal/{normal_name}.png" if normal_name else "")

    orm_slots = build_orm_slots(orm_case, orm_name, orm_ue, ar_name, ar_ue,
                                metallic_name, metallic_ue, level_name)
    for slot, data in orm_slots.items():
        tex_out[slot]   = data["path"]
        overrides[slot] = data["override"]
        if data.get("ref"):
            tex_refs[slot] = data["ref"]

    tex_out["emissive"]   = ""
    overrides["emissive"] = False

    add_tex("opacityMask", opacity_name, opacity_ue)

    # Apply ORM channel-source CB overrides on top of MI-derived values
    for k, v in get_orm_cb(orm_case).items():
        cb[k] = v

    instance = {
        "assetName":   mi_name,
        "template":    render_state["blendMode"],
        "renderState": render_state,
        "overrides":   overrides,
        "textures":    tex_out,
        "cb":          cb,
    }
    return instance, tex_refs


# ---------------------------------------------------------------------------
# Bundle builder
# ---------------------------------------------------------------------------

def build_bundle(level_name: str, mi_files: list[Path]) -> tuple[dict, dict]:
    """Returns (bundle_dict, texture_report).
    texture_report: texture_name -> {uePath, usedBy: [str]}
    """
    instances = []
    texture_report: dict[str, dict] = {}

    for mi_path in sorted(mi_files):
        mi_name = mi_path.stem
        try:
            mi_data = normalize_mi_data(load_json(mi_path))
        except Exception as e:
            print(f"  WARN: Could not read {mi_path.name}: {e}")
            continue

        instance, tex_refs = build_instance(mi_name, mi_data, level_name)
        instances.append(instance)

        for slot, (tex_name, ue_path) in tex_refs.items():
            if tex_name not in texture_report:
                texture_report[tex_name] = {"uePath": ue_path, "usedBy": []}
            texture_report[tex_name]["usedBy"].append(f"{mi_name} ({slot})")

    bundle = {"instances": instances, "mappings": [], "templates": TEMPLATES}
    return bundle, texture_report


def merge_existing_texture_slots(new_bundle: dict, existing_bundle: dict) -> int:
    """Preserve existing texture slots when the current MI export has no texture data."""
    existing_by_name = {
        inst.get("assetName"): inst
        for inst in existing_bundle.get("instances", [])
        if inst.get("assetName")
    }
    restored_count = 0
    for instance in new_bundle.get("instances", []):
        existing = existing_by_name.get(instance.get("assetName"))
        if not existing:
            continue
        textures  = instance.get("textures", {})
        overrides = instance.get("overrides", {})
        for slot, old_path in existing.get("textures", {}).items():
            if not old_path or textures.get(slot):
                continue
            if not existing.get("overrides", {}).get(slot, False):
                continue
            textures[slot]  = old_path
            overrides[slot] = True
            restored_count += 1
    return restored_count


# ---------------------------------------------------------------------------
# Report writers
# ---------------------------------------------------------------------------

def check_missing_textures(bundle: dict, resources_dir: Path) -> list[dict]:
    """Returns list of {slot, path, mi} for textures referenced but not on disk."""
    missing = []
    for instance in bundle.get("instances", []):
        mi_name   = instance.get("assetName", "?")
        textures  = instance.get("textures", {})
        overrides = instance.get("overrides", {})
        for slot, rel_path in textures.items():
            if not rel_path or not overrides.get(slot, False):
                continue
            if not (resources_dir / rel_path).exists():
                missing.append({"slot": slot, "path": rel_path, "mi": mi_name})
    return missing


def write_texture_report(report_path: Path, level_name: str,
                         texture_report: dict, level_tex_dir: str):
    lines = [
        f"Texture extraction list for: {level_name}",
        "Save extracted textures (.png) to:",
        f"  Resources/{level_tex_dir}",
        "=" * 60,
        "",
    ]
    for tex_name, info in sorted(texture_report.items()):
        lines.append(tex_name)
        lines.append(f"  Source : {info['uePath']}")
        lines.append(f"  Used by: {', '.join(info['usedBy'])}")
        lines.append("")
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text("\n".join(lines), encoding="utf-8")


def write_missing_report(report_path: Path, level_name: str, missing: list[dict]):
    lines = [
        f"Missing textures for: {level_name}",
        f"Total: {len(missing)}",
        "=" * 60,
        "",
    ]
    by_path: dict[str, list[str]] = {}
    for entry in missing:
        by_path.setdefault(entry["path"], []).append(f"{entry['mi']} ({entry['slot']})")
    for path, used_by in sorted(by_path.items()):
        lines.append(path)
        lines.append(f"  Used by: {', '.join(used_by)}")
        lines.append("")
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text("\n".join(lines), encoding="utf-8")


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def parse_args():
    parser = argparse.ArgumentParser(
        description="Generate .material.json bundle from a level's _MI folder."
    )
    parser.add_argument("--level-dir", required=True,
                        help="Level name (e.g. SmallLevel_SimonArea) or full path.")
    parser.add_argument("--overwrite", action="store_true",
                        help="Overwrite existing output files.")
    return parser.parse_args()


def main():
    args = parse_args()

    script_dir    = Path(__file__).parent
    resources_dir = script_dir.parent / "Client" / "Bin" / "Resources"

    level_name = Path(args.level_dir).name

    mi_dir = resources_dir / "Map" / (level_name + "_MI")
    if not mi_dir.exists():
        raise FileNotFoundError(f"_MI folder not found: {mi_dir}")

    out_bundle          = resources_dir / "Materials" / f"{level_name}.material.json"
    level_tex_dir       = f"Map/{level_name}/Textures"
    need_tex_dir        = resources_dir / "Map" / "NeedTextureJson"
    report_path         = need_tex_dir / f"{level_name}_textures.txt"
    missing_report_path = need_tex_dir / f"{level_name}_missing.txt"

    if out_bundle.exists() and not args.overwrite:
        print(f"Output already exists (use --overwrite): {out_bundle}")
        return

    mi_files = sorted(mi_dir.glob("*.json"))
    print(f"[1/3] Reading {len(mi_files)} MI JSON files from: {mi_dir}")

    print("[2/3] Building material bundle...")
    bundle, texture_report = build_bundle(level_name, mi_files)
    if out_bundle.exists() and args.overwrite:
        restored = merge_existing_texture_slots(bundle, load_json(out_bundle))
        if restored:
            print(f"  Preserved {restored} existing texture slots missing from current MI exports.")

    print("[3/3] Writing outputs...")
    out_bundle.parent.mkdir(parents=True, exist_ok=True)
    with out_bundle.open("w", encoding="utf-8") as f:
        json.dump(bundle, f, indent=2, ensure_ascii=False)

    write_texture_report(report_path, level_name, texture_report, level_tex_dir)

    missing = check_missing_textures(bundle, resources_dir)
    write_missing_report(missing_report_path, level_name, missing)

    print(f"\nDone.")
    print(f"  Bundle          : {out_bundle}")
    print(f"  Unique textures : {len(texture_report)}")
    print(f"  Missing on disk : {len(missing)}")
    print(f"  Texture report  : {report_path}")
    print(f"  Missing report  : {missing_report_path}")
    print(f"\nTexture save location:")
    print(f"  Resources/{level_tex_dir}/")


if __name__ == "__main__":
    main()

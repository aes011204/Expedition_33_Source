"""
GenerateMapMaterials.py

Reads MI JSON files from a _MI folder and generates:
  1. Resources/Materials/[LevelName].material.json  (all MI instances in one bundle)
  2. NoMatJson/[LevelName]_textures.txt             (texture extraction list for fmodel)

Texture path format inside the JSON:
  Map/[LevelName]/Textures/[TextureName].png
Engine resolves this as: Resources/ + Map/[LevelName]/Textures/[TextureName].png

Usage:
    python GenerateMapMaterials.py --level-dir SmallLevel_SimonArea
    python GenerateMapMaterials.py --level-dir SmallLevel_SimonArea --overwrite
"""

import argparse
import json
from pathlib import Path


# ---------------------------------------------------------------------------
# Slot candidate table — edit here to add ORM / Emissive / etc. later
# Higher index in the list = lower priority (first match wins)
# ---------------------------------------------------------------------------
SLOT_CANDIDATES: dict[str, list[str]] = {
    "baseColor":   ["Texture1_AlbedoAlpha", "Texture1_AlbedoRoughness", "BaseLayer_AlbedoRoughness", "1stLayer_AlbedoRoughness", "Albedo", "PM_Diffuse"],
    "normal":      ["PM_Normals", "Texture2_NormalRoughEmi", "Texture2_Normal+MEHAo"],
    "orm":         ["ORM Map", "PM_SpecularMasks", "Grunge_Texture", "Roughness"],
    "emissive":    [],
    "opacityMask": ["Opacity"],
}

# Scalar parameter names that map to CB fields
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

AR_BASECOLOR_KEYS = {
    "texture1_albedoroughness",
    "baselayer_albedoroughness",
    "1stlayer_albedoroughness",
}

TEMPLATES = [
    {"id": 0, "name": "OpaquePBR",   "supportsBaseColor": True, "supportsNormal": True,
     "supportsORM": True, "supportsEmissive": True, "supportsOpacityMask": False},
    {"id": 1, "name": "MaskedPBR",   "supportsBaseColor": True, "supportsNormal": True,
     "supportsORM": True, "supportsEmissive": True, "supportsOpacityMask": True},
    {"id": 2, "name": "EmissivePBR", "supportsBaseColor": True, "supportsNormal": True,
     "supportsORM": True, "supportsEmissive": True, "supportsOpacityMask": False},
    {"id": 3, "name": "TransparentPBR", "supportsBaseColor": True, "supportsNormal": True,
     "supportsORM": True, "supportsEmissive": True, "supportsOpacityMask": False},
]


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
        obj_path = obj.get("ObjectPath", "")         # e.g. /Game/.../Asset.0
        obj_name_raw = obj.get("ObjectName", "")     # e.g. Texture2D'AssetName'
        asset_name = obj_name_raw.split("'")[1] if "'" in obj_name_raw else ""
        if name and obj_path and asset_name:
            base_path = obj_path.rsplit(".", 1)[0]   # strip trailing .0 index
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
    # UE asset paths end with '.AssetName' after the last dot
    base = ue_path.rsplit(".", 1)[-1]
    return base


def is_albedo_roughness_source(textures: dict, base_color_ue_path: str) -> bool:
    if not base_color_ue_path:
        return False

    for param_name, ue_path in textures.items():
        if ue_path != base_color_ue_path:
            continue
        if param_name.lower() in AR_BASECOLOR_KEYS:
            return True

    texture_name = ue_path_to_texture_name(base_color_ue_path).lower()
    return "roughness" in texture_name or texture_name.endswith("_ar")


def pick_texture_slot(textures: dict, candidates: list[str]) -> tuple[str, str]:
    """
    Returns (texture_name, ue_path) for the first matching candidate key.
    texture_name is the bare asset name (no path, no extension).
    """
    for key in candidates:
        for param_name, ue_path in textures.items():
            if param_name.lower() == key.lower():
                name = ue_path_to_texture_name(ue_path)
                if name:
                    return name, ue_path
    return "", ""


def parse_blend_mode(mi_data: dict) -> int:
    """Returns 0 (Opaque) or 1 (Masked)."""
    overrides = mi_data.get("Parameters", {}).get("Properties", {}).get("BasePropertyOverrides", {})
    blend_str = overrides.get("BlendMode", "")
    if "BLEND_Masked" in blend_str:
        return 1
    # Parameters.BlendMode integer fallback
    blend_int = mi_data.get("Parameters", {}).get("BlendMode", 0)
    if blend_int == 1:
        return 1
    return 0


def parse_render_state(mi_data: dict) -> dict:
    overrides = mi_data.get("Parameters", {}).get("Properties", {}).get("BasePropertyOverrides", {})
    blend_mode = parse_blend_mode(mi_data)
    return {
        "blendMode": blend_mode,
        "twoSided": overrides.get("TwoSided", False),
        "opacityMaskClipValue": overrides.get("OpacityMaskClipValue", 0.3333),
    }


def parse_cb(mi_data: dict) -> dict:
    cb = dict(DEFAULT_CB)
    scalars = mi_data.get("Parameters", {}).get("Scalars", {})
    for param_name, value in scalars.items():
        cb_key = SCALAR_TO_CB.get(param_name.lower())
        if cb_key:
            cb[cb_key] = float(value)
    overrides = mi_data.get("Parameters", {}).get("Properties", {}).get("BasePropertyOverrides", {})
    if "OpacityMaskClipValue" in overrides:
        cb["opacityMaskClipValue"] = overrides["OpacityMaskClipValue"]

    colors = mi_data.get("Parameters", {}).get("Colors", {})
    for param_name, color_val in colors.items():
        key = param_name.lower()
        if key == "base tiling/offset":
            cb["uvScale"]  = [color_val.get("R", 1.0), color_val.get("G", 1.0)]
            cb["uvOffset"] = [color_val.get("B", 0.0), color_val.get("A", 0.0)]
        elif key == "albedo tint":
            cb["baseColorTint"] = [
                color_val.get("R", 1.0),
                color_val.get("G", 1.0),
                color_val.get("B", 1.0),
                color_val.get("A", 1.0),
            ]

    return cb


def build_instance(mi_name: str, mi_data: dict, level_name: str) -> tuple[dict, dict]:
    """
    Returns (instance_entry, texture_refs).
    texture_refs: slot -> (texture_name, ue_path)
    """
    textures_raw: dict = mi_data.get("Textures", {})
    render_state = parse_render_state(mi_data)
    cb = parse_cb(mi_data)
    template_id = render_state["blendMode"]  # 0=Opaque, 1=Masked

    tex_out: dict[str, str] = {}     # slot -> relative path for JSON
    tex_refs: dict[str, tuple[str, str]] = {}  # slot -> (name, ue_path) for report
    overrides: dict[str, bool] = {}

    for slot, candidates in SLOT_CANDIDATES.items():
        if not candidates:
            tex_out[slot] = ""
            overrides[slot] = False
            continue

        name, ue_path = pick_texture_slot(textures_raw, candidates)
        if name:
            rel_path = f"Map/{level_name}/Textures/{name}.png"
            tex_out[slot] = rel_path
            overrides[slot] = True
            tex_refs[slot] = (name, ue_path)
        else:
            tex_out[slot] = ""
            overrides[slot] = False

    force_pass_index = -1
    if (
        render_state["blendMode"] == 0
        and overrides.get("baseColor", False)
        and not overrides.get("orm", False)
        and is_albedo_roughness_source(textures_raw, tex_refs.get("baseColor", ("", ""))[1])
    ):
        force_pass_index = 8 if render_state.get("twoSided", False) else 7
        cb["metalnessScale"] = 0.0

    instance = {
        "assetName":   mi_name,
        "template":    template_id,
        "renderState": render_state,
        "overrides":   overrides,
        "textures":    tex_out,
        "cb":          cb,
    }
    if force_pass_index >= 0:
        instance["forcePassIndex"] = force_pass_index

    return instance, tex_refs


def build_bundle(level_name: str, mi_files: list[Path]) -> tuple[dict, dict]:
    """
    Returns (bundle_dict, texture_report).
    texture_report: texture_name -> { ue_path, used_by: [(mi_name, slot)] }
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

    bundle = {
        "instances": instances,
        "mappings":  [],
        "templates": TEMPLATES,
    }
    return bundle, texture_report


def merge_existing_texture_slots(new_bundle: dict, existing_bundle: dict) -> int:
    """Preserve existing texture slots when the current MI export has no texture data."""
    existing_by_name = {
        instance.get("assetName"): instance
        for instance in existing_bundle.get("instances", [])
        if instance.get("assetName")
    }

    restored_count = 0
    for instance in new_bundle.get("instances", []):
        existing = existing_by_name.get(instance.get("assetName"))
        if not existing:
            continue

        textures = instance.get("textures", {})
        overrides = instance.get("overrides", {})
        existing_textures = existing.get("textures", {})
        existing_overrides = existing.get("overrides", {})

        for slot, old_path in existing_textures.items():
            if not old_path:
                continue
            if textures.get(slot):
                continue
            if not existing_overrides.get(slot, False):
                continue

            textures[slot] = old_path
            overrides[slot] = True
            restored_count += 1

    return restored_count


def write_texture_report(report_path: Path, level_name: str,
                         texture_report: dict, level_tex_dir: str):
    lines = [
        f"Texture extraction list for: {level_name}",
        f"Save extracted textures (.png) to:",
        f"  Resources/{level_tex_dir}",
        "=" * 60,
        "",
    ]
    for tex_name, info in sorted(texture_report.items()):
        lines.append(f"{tex_name}")
        lines.append(f"  Source : {info['uePath']}")
        lines.append(f"  Used by: {', '.join(info['usedBy'])}")
        lines.append("")

    report_path.parent.mkdir(parents=True, exist_ok=True)
    with report_path.open("w", encoding="utf-8") as f:
        f.write("\n".join(lines))


def parse_args():
    parser = argparse.ArgumentParser(
        description="Generate .material.json bundle from a level's _MI folder."
    )
    parser.add_argument(
        "--level-dir",
        required=True,
        help="Level name (e.g. SmallLevel_SimonArea) or full path to the level model folder.",
    )
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="Overwrite existing output files.",
    )
    return parser.parse_args()


def main():
    args = parse_args()

    script_dir = Path(__file__).parent
    resources_dir = script_dir.parent / "Client" / "Bin" / "Resources"

    # Resolve level name
    level_input = Path(args.level_dir)
    if level_input.is_absolute() or level_input.exists():
        level_name = level_input.name
    else:
        level_name = level_input.name

    mi_dir = resources_dir / "Map" / (level_name + "_MI")
    if not mi_dir.exists():
        raise FileNotFoundError(f"_MI folder not found: {mi_dir}")

    out_bundle = resources_dir / "Materials" / f"{level_name}.material.json"
    level_tex_dir = f"Map/{level_name}/Textures"
    report_path = resources_dir / "Map" / "NeedTextureJson" / f"{level_name}_textures.txt"

    if out_bundle.exists() and not args.overwrite:
        print(f"Output already exists (use --overwrite): {out_bundle}")
        return

    mi_files = sorted(mi_dir.glob("*.json"))
    print(f"[1/3] Reading {len(mi_files)} MI JSON files from: {mi_dir}")

    print(f"[2/3] Building material bundle...")
    bundle, texture_report = build_bundle(level_name, mi_files)
    if out_bundle.exists() and args.overwrite:
        existing_bundle = load_json(out_bundle)
        restored_count = merge_existing_texture_slots(bundle, existing_bundle)
        if restored_count:
            print(f"  Preserved {restored_count} existing texture slots missing from current MI exports.")

    print(f"[3/3] Writing outputs...")
    out_bundle.parent.mkdir(parents=True, exist_ok=True)
    with out_bundle.open("w", encoding="utf-8") as f:
        json.dump(bundle, f, indent=2, ensure_ascii=False)

    write_texture_report(report_path, level_name, texture_report, level_tex_dir)

    print(f"\nDone.")
    print(f"  Bundle  : {out_bundle}")
    print(f"  Textures: {len(texture_report)} unique textures")
    print(f"  Report  : {report_path}")
    print(f"\nTexture save location:")
    print(f"  Resources/{level_tex_dir}/")


if __name__ == "__main__":
    main()

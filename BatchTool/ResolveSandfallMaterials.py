import argparse
import json
import os
from pathlib import Path


IMAGE_EXTENSIONS = {".png", ".dds", ".tga", ".jpg", ".jpeg", ".bmp"}


def load_json(path: Path):
    with path.open("r", encoding="utf-8") as file:
        return json.load(file)


def find_skeletal_mesh_record(sk_json_data):
    for record in sk_json_data:
        properties = record.get("Properties", {})
        if "SkeletalMaterials" in properties and "LODModels" in properties:
            return record
    raise ValueError("Could not find a SkeletalMesh record with SkeletalMaterials and LODModels.")


def ue_object_path_to_json_path(sandfall_root: Path, object_path: str) -> Path | None:
    if not object_path or not object_path.startswith("/Game/"):
        return None

    relative_path = object_path[len("/Game/") :]
    package_path = relative_path.split(".", 1)[0]
    return sandfall_root / "Content" / (package_path + ".json")


def extract_asset_name(object_path: str) -> str:
    if not object_path:
        return ""

    package_path = object_path.split(".", 1)[0]
    return package_path.rsplit("/", 1)[-1]


def build_texture_index(sandfall_root: Path):
    texture_index = {}

    for root, _, files in os.walk(sandfall_root):
        for file_name in files:
            file_path = Path(root) / file_name
            if file_path.suffix.lower() not in IMAGE_EXTENSIONS:
                continue

            stem_key = file_path.stem.lower()
            texture_index.setdefault(stem_key, []).append(file_path)

    return texture_index


def score_texture_candidate(candidate_path: Path, preferred_dirs: list[Path]) -> tuple[int, int, int]:
    candidate_text = str(candidate_path).lower()

    for index, preferred_dir in enumerate(preferred_dirs):
        preferred_text = str(preferred_dir).lower()
        if preferred_text and candidate_text.startswith(preferred_text):
            return (3, -index, -len(candidate_text))

    return (0, 0, -len(candidate_text))


def resolve_texture_file(asset_name: str, texture_index: dict, mi_json_path: Path) -> str | None:
    if not asset_name:
        return None

    candidates = texture_index.get(asset_name.lower(), [])
    if not candidates:
        return None

    preferred_dirs = [
        mi_json_path.parent,
        mi_json_path.parent.parent / "Texture",
        mi_json_path.parent.parent / "Textures",
        mi_json_path.parent / "Texture",
        mi_json_path.parent / "Textures",
    ]

    sorted_candidates = sorted(
        candidates,
        key=lambda candidate: score_texture_candidate(candidate, preferred_dirs),
        reverse=True,
    )
    return str(sorted_candidates[0])


def classify_texture_slot(parameter_name: str):
    key = parameter_name.lower()

    if "mask" in key:
        if key == "pm_specularmasks":
            return None, 0
        return "OpacityMask", 100

    if key in {"base color map", "pm_diffuse", "color_main"}:
        return "BaseColor", 100
    if "basecolor" in key:
        return "BaseColor", 80

    if key in {"normal map", "pm_normals", "normal_main"}:
        return "Normal", 100
    if "normal" in key:
        return "Normal", 60

    if key in {"orm map", "pm_specularmasks"}:
        return "ORM", 100
    if "occlusionroughnessmetallic" in key or "orm" in key:
        return "ORM", 80

    if key in {"glow_map", "pm_emissive"}:
        return "Emissive", 100
    if "emissive" in key or "glow" in key:
        return "Emissive", 80

    if key == "roughness_main" or "roughness" in key:
        return "Roughness", 70

    return None, 0


def normalize_textures(textures: dict, texture_index: dict, mi_json_path: Path):
    slots = {}
    extras = []

    for parameter_name, object_path in textures.items():
        asset_name = extract_asset_name(object_path)
        resolved_file = resolve_texture_file(asset_name, texture_index, mi_json_path)
        slot_name, score = classify_texture_slot(parameter_name)

        entry = {
            "parameter": parameter_name,
            "assetName": asset_name,
            "objectPath": object_path,
            "resolvedFile": resolved_file,
        }

        if slot_name is None:
            extras.append(entry)
            continue

        current = slots.get(slot_name)
        if current is None or score > current["score"]:
            slots[slot_name] = {
                "score": score,
                **entry,
            }

    for slot_name, entry in list(slots.items()):
        entry.pop("score", None)

    return slots, extras


def extract_render_state(mi_data: dict):
    parameters = mi_data.get("Parameters", {})
    properties = parameters.get("Properties", {})
    overrides = properties.get("BasePropertyOverrides", {})

    return {
        "BlendMode": overrides.get("BlendMode"),
        "TwoSided": overrides.get("TwoSided"),
        "OpacityMaskClipValue": overrides.get("OpacityMaskClipValue"),
        "ShadingModel": overrides.get("ShadingModel") or parameters.get("ShadingModel"),
    }


def collect_materials(sk_record: dict, sandfall_root: Path, texture_index: dict):
    properties = sk_record.get("Properties", {})
    skeletal_materials = properties.get("SkeletalMaterials", [])
    materials = []
    warnings = []

    for material_index, material_info in enumerate(skeletal_materials):
        material_ref = material_info.get("Material", {})
        object_path = material_ref.get("ObjectPath", "")
        mi_json_path = ue_object_path_to_json_path(sandfall_root, object_path)

        material_entry = {
            "materialIndex": material_index,
            "slotName": material_info.get("MaterialSlotName"),
            "materialObjectName": material_ref.get("ObjectName"),
            "materialObjectPath": object_path,
            "miJsonPath": str(mi_json_path) if mi_json_path else None,
            "slots": {},
            "extraTextures": [],
            "renderState": {},
        }

        if mi_json_path is None or not mi_json_path.exists():
            warnings.append(f"Missing MI json for material index {material_index}: {object_path}")
            materials.append(material_entry)
            continue

        mi_data = load_json(mi_json_path)
        textures = mi_data.get("Textures", {})
        slots, extras = normalize_textures(textures, texture_index, mi_json_path)

        material_entry["slots"] = slots
        material_entry["extraTextures"] = extras
        material_entry["renderState"] = extract_render_state(mi_data)
        materials.append(material_entry)

    return materials, warnings


def collect_sections(sk_record: dict, materials: list[dict]):
    properties = sk_record.get("Properties", {})
    lod_models = properties.get("LODModels", [])
    if not lod_models:
        return []

    material_lookup = {entry["materialIndex"]: entry for entry in materials}
    sections = []

    for section_index, section in enumerate(lod_models[0].get("Sections", [])):
        material_index = section.get("MaterialIndex", -1)
        material_entry = material_lookup.get(material_index, {})

        sections.append({
            "sectionIndex": section_index,
            "materialIndex": material_index,
            "slotName": material_entry.get("slotName"),
            "materialObjectName": material_entry.get("materialObjectName"),
            "numTriangles": section.get("NumTriangles"),
            "numVertices": section.get("NumVertices"),
        })

    return sections


def write_output(output_path: Path, payload: dict):
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", encoding="utf-8") as file:
        json.dump(payload, file, indent=2, ensure_ascii=False)


def parse_args():
    parser = argparse.ArgumentParser(description="Resolve Sandfall SK/MI json files into normalized material slot data.")
    parser.add_argument(
        "--sandfall-root",
        default=r"D:\Clair Obscur Expedition 33\Exports\Sandfall",
        help="Root folder containing the exported Sandfall Content tree.",
    )
    parser.add_argument(
        "--sk-json",
        required=True,
        help="Path to the exported SK_*.json file.",
    )
    parser.add_argument(
        "--out-json",
        required=True,
        help="Output json path for the resolved material mapping.",
    )
    return parser.parse_args()


def main():
    args = parse_args()

    sandfall_root = Path(args.sandfall_root)
    sk_json_path = Path(args.sk_json)
    output_path = Path(args.out_json)

    if not sandfall_root.exists():
        raise FileNotFoundError(f"Sandfall root not found: {sandfall_root}")
    if not sk_json_path.exists():
        raise FileNotFoundError(f"SK json not found: {sk_json_path}")

    print(f"[1/4] Indexing textures under: {sandfall_root}")
    texture_index = build_texture_index(sandfall_root)

    print(f"[2/4] Reading skeletal mesh json: {sk_json_path}")
    sk_json_data = load_json(sk_json_path)
    sk_record = find_skeletal_mesh_record(sk_json_data)

    print("[3/4] Resolving material instances and texture slots")
    materials, warnings = collect_materials(sk_record, sandfall_root, texture_index)

    print("[4/4] Building section map and writing output")
    sections = collect_sections(sk_record, materials)

    payload = {
        "source": {
            "sandfallRoot": str(sandfall_root),
            "skJson": str(sk_json_path),
        },
        "materials": materials,
        "lod0Sections": sections,
        "warnings": warnings,
    }

    write_output(output_path, payload)

    print(f"Done: {output_path}")
    if warnings:
        print(f"Warnings: {len(warnings)}")
        for warning in warnings:
            print(f"  - {warning}")


if __name__ == "__main__":
    main()

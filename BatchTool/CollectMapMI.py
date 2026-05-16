"""
CollectMapMI.py

Scans a level model JSON folder, collects StaticMesh MI references,
copies MI JSON files from Sandfall exports to a _MI output folder,
and reports three problem categories:

  Case A - MI reference exists but the JSON is missing in Sandfall exports
  Case B - Model JSON has no valid MI reference (no StaticMaterials / all null)
  Case C - .model and .json filename pair is broken (one side missing)

A report file is saved to Client/Bin/Resources/Map/NoMatJson/[LevelName]_report.txt

Usage:
    python CollectMapMI.py --level-dir SmallLevel_SimonArea
    python CollectMapMI.py --level-dir SmallLevel_SimonArea --overwrite
"""

import argparse
import json
import shutil
from pathlib import Path


def load_json(path: Path):
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def ue_object_path_to_json_path(sandfall_root: Path, object_path: str,
                                engine_root: Path | None = None) -> Path | None:
    if not object_path:
        return None
    if object_path.startswith("/Game/"):
        relative = object_path[len("/Game/"):]
        package = relative.split(".", 1)[0]
        return sandfall_root / "Content" / (package + ".json")
    if object_path.startswith("/Engine/") and engine_root is not None:
        relative = object_path[len("/Engine/"):]
        package = relative.split(".", 1)[0]
        return engine_root / "Content" / (package + ".json")
    return None


def collect_mi_paths_from_model_json(model_json_data: list) -> list[tuple[str, str]]:
    """Returns list of (ObjectName, ObjectPath) with valid /Game/ paths."""
    results = []
    for record in model_json_data:
        if record.get("Type") != "StaticMesh":
            continue
        materials = record.get("Properties", {}).get("StaticMaterials") or []
        for mat in materials:
            interface = mat.get("MaterialInterface") or {}
            obj_name = interface.get("ObjectName", "")
            obj_path = interface.get("ObjectPath", "")
            if obj_path and (obj_path.startswith("/Game/") or obj_path.startswith("/Engine/")):
                results.append((obj_name, obj_path))
    return results


def check_model_json_pairs(level_dir: Path) -> tuple[list[str], list[str]]:
    """
    Returns (json_without_model, model_without_json).
    Compares stems of .json and .model files in the level dir.
    """
    json_stems = {p.stem for p in level_dir.glob("*.json")}
    model_stems = {p.stem for p in level_dir.glob("*.model")}

    json_without_model = sorted(json_stems - model_stems)
    model_without_json = sorted(model_stems - json_stems)
    return json_without_model, model_without_json


def print_section(title: str, items: list[str]):
    print(f"\n[!] {title} ({len(items)}):")
    for item in items:
        print(f"  {item}")


def write_report(report_path: Path, level_name: str,
                 case_a: dict[str, list[tuple[str, str]]],
                 case_b: list[str],
                 case_c_no_model: list[str],
                 case_c_no_json: list[str],
                 case_d: dict[str, list[str]]):
    report_path.parent.mkdir(parents=True, exist_ok=True)
    lines = []
    lines.append(f"MI Collection Report: {level_name}")
    lines.append("=" * 60)

    lines.append(f"\n[Case A] MI reference missing in Sandfall exports ({len(case_a)} model(s))")
    if case_a:
        for model_stem, refs in sorted(case_a.items()):
            lines.append(f"  {model_stem}")
            for obj_name, _ in refs:
                lines.append(f"    - {obj_name}")
    else:
        lines.append("  (none)")

    lines.append(f"\n[Case B] No valid MI reference in model JSON ({len(case_b)} model(s))")
    if case_b:
        for name in case_b:
            lines.append(f"  {name}")
    else:
        lines.append("  (none)")

    lines.append(f"\n[Case C] .json exists but .model is missing ({len(case_c_no_model)} file(s))")
    if case_c_no_model:
        for name in case_c_no_model:
            lines.append(f"  {name}")
    else:
        lines.append("  (none)")

    lines.append(f"\n[Case C] .model exists but .json is missing ({len(case_c_no_json)} file(s))")
    if case_c_no_json:
        for name in case_c_no_json:
            lines.append(f"  {name}")
    else:
        lines.append("  (none)")

    lines.append(f"\n[Case D] Engine placeholder material (needs manual MI) ({len(case_d)} model(s))")
    if case_d:
        for model_stem, engine_paths in sorted(case_d.items()):
            lines.append(f"  {model_stem}")
            for ep in engine_paths:
                lines.append(f"    - {ep}")
    else:
        lines.append("  (none)")

    with report_path.open("w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


def parse_args():
    parser = argparse.ArgumentParser(
        description="Collect MI JSON files referenced by map model JSONs."
    )
    parser.add_argument(
        "--level-dir",
        required=True,
        help="Level model JSON folder name or full path.",
    )
    parser.add_argument(
        "--sandfall-root",
        default=r"D:\Clair Obscur Expedition 33\Exports\Sandfall",
        help="Root folder of the Sandfall exports.",
    )
    parser.add_argument(
        "--engine-root",
        default=r"D:\Clair Obscur Expedition 33\Exports\Engine",
        help="Root folder of the Engine exports (/Engine/ paths).",
    )
    parser.add_argument(
        "--out-dir",
        default=None,
        help="Output folder. Defaults to <level-dir>_MI next to level-dir.",
    )
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="Overwrite existing files in output folder.",
    )
    return parser.parse_args()


def main():
    args = parse_args()

    level_dir = Path(args.level_dir)
    sandfall_root = Path(args.sandfall_root)
    engine_root = Path(args.engine_root) if Path(args.engine_root).exists() else None
    if engine_root is None:
        print(f"  NOTE: Engine root not found, /Engine/ paths will be skipped: {args.engine_root}")

    # 이름만 입력한 경우 Map 폴더 기준으로 경로 보정
    if not level_dir.is_absolute() and not level_dir.exists():
        script_dir = Path(__file__).parent
        candidate = script_dir.parent / "Client" / "Bin" / "Resources" / "Map" / level_dir
        if candidate.exists():
            level_dir = candidate

    out_dir = Path(args.out_dir) if args.out_dir else level_dir.parent / (level_dir.name + "_MI")
    report_dir = level_dir.parent / "NoMatJson"
    report_path = report_dir / f"{level_dir.name}_report.txt"

    if not level_dir.exists():
        raise FileNotFoundError(f"Level dir not found: {level_dir}")
    if not sandfall_root.exists():
        raise FileNotFoundError(f"Sandfall root not found: {sandfall_root}")

    out_dir.mkdir(parents=True, exist_ok=True)

    # --- Case C: .model / .json pair check ---
    json_without_model, model_without_json = check_model_json_pairs(level_dir)

    # --- Scan model JSONs ---
    model_jsons = sorted(level_dir.glob("*.json"))
    print(f"[1/3] Scanning {len(model_jsons)} model JSON files in: {level_dir}")

    mi_map: dict[str, str] = {}          # ObjectPath -> ObjectName
    model_refs: dict[str, list[tuple[str, str]]] = {}  # stem -> [(obj_name, obj_path)]
    case_b_models: list[str] = []        # models with no valid MI reference

    for model_json_path in model_jsons:
        try:
            data = load_json(model_json_path)
        except Exception as e:
            print(f"  WARN: Could not read {model_json_path.name}: {e}")
            continue

        refs = collect_mi_paths_from_model_json(data)

        if not refs:
            # Case B: has StaticMesh but no MI, or no StaticMesh at all
            case_b_models.append(model_json_path.stem)
            continue

        for obj_name, obj_path in refs:
            if obj_path not in mi_map:
                mi_map[obj_path] = obj_name
            model_refs.setdefault(model_json_path.stem, []).append((obj_name, obj_path))

    print(f"[2/3] Found {len(mi_map)} unique MI references across {len(model_refs)} models")

    # --- Copy MI JSONs ---
    print(f"[3/3] Copying MI JSON files to: {out_dir}")
    copied = 0
    skipped = 0
    missing_paths: set[str] = set()

    for obj_path, obj_name in sorted(mi_map.items()):
        src = ue_object_path_to_json_path(sandfall_root, obj_path, engine_root)
        if src is None or not src.exists():
            missing_paths.add(obj_path)
            continue

        dst = out_dir / src.name
        if dst.exists() and not args.overwrite:
            skipped += 1
            continue

        shutil.copy2(src, dst)
        copied += 1

    # --- Case A: models referencing missing MI ---
    case_a_models: dict[str, list[tuple[str, str]]] = {}
    for model_stem, refs in sorted(model_refs.items()):
        for obj_name, obj_path in refs:
            if obj_path in missing_paths:
                case_a_models.setdefault(model_stem, []).append((obj_name, obj_path))

    # --- Case D: models whose every MI is an Engine placeholder ---
    ENGINE_MAT_PREFIX = "/Engine/EngineMaterials/"
    case_d_models: dict[str, list[str]] = {}
    for model_stem, refs in sorted(model_refs.items()):
        engine_refs = [obj_path for _, obj_path in refs if obj_path.startswith(ENGINE_MAT_PREFIX)]
        if engine_refs and len(engine_refs) == len(refs):
            case_d_models[model_stem] = engine_refs

    # --- Console output ---
    print(f"\nDone.")
    print(f"  Copied : {copied}")
    print(f"  Skipped: {skipped} (already exist, use --overwrite to replace)")
    print(f"  Missing: {len(missing_paths)} MI JSON(s) not found in Sandfall exports")

    if case_a_models:
        print_section("Case A - MI reference missing in Sandfall exports", [])
        for model_stem, refs in case_a_models.items():
            print(f"  {model_stem}")
            for obj_name, obj_path in refs:
                src = ue_object_path_to_json_path(sandfall_root, obj_path, engine_root)
                print(f"    - {obj_name}")
                print(f"      Expected: {src}")

    if case_b_models:
        print_section("Case B - No valid MI reference in model JSON", case_b_models)

    if json_without_model:
        print_section("Case C - .json exists but .model is missing", json_without_model)

    if model_without_json:
        print_section("Case C - .model exists but .json is missing", model_without_json)

    if case_d_models:
        print(f"\n[!] Case D - Engine placeholder material (needs manual MI) ({len(case_d_models)}):")
        for model_stem, engine_paths in case_d_models.items():
            print(f"  {model_stem}")
            for ep in engine_paths:
                print(f"    - {ep}")

    # --- Write report ---
    write_report(report_path, level_dir.name, case_a_models, case_b_models,
                 json_without_model, model_without_json, case_d_models)
    print(f"\nReport saved to: {report_path}")


if __name__ == "__main__":
    main()

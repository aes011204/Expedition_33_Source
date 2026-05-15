"""
CollectMapTextures.py

Reads a _textures.txt report and copies texture files (+ matching .json if present)
from Sandfall/Engine exports to the level's Textures folder.

Path conversion:
  /Game/X   -> [sandfall-root]/Content/X
  /Engine/X -> [engine-root]/Content/X

Usage:
    python CollectMapTextures.py --level-dir SmallLevel_SimonArea
    python CollectMapTextures.py --level-dir SmallLevel_SimonArea --overwrite
"""

import argparse
import shutil
from pathlib import Path


IMAGE_EXTENSIONS = {".png", ".dds", ".tga", ".jpg", ".jpeg", ".bmp"}


def ue_path_to_disk_path(ue_path: str, sandfall_root: Path,
                         engine_root: Path | None) -> Path | None:
    if not ue_path:
        return None
    if ue_path.startswith("/Game/"):
        relative = ue_path[len("/Game/"):]
        package = relative.split(".", 1)[0]
        return sandfall_root / "Content" / package
    if ue_path.startswith("/Engine/") and engine_root is not None:
        relative = ue_path[len("/Engine/"):]
        package = relative.split(".", 1)[0]
        return engine_root / "Content" / package
    return None


def find_texture_file(base_path: Path) -> Path | None:
    """base_path has no extension — search for any image file with that stem."""
    for ext in IMAGE_EXTENSIONS:
        candidate = base_path.with_suffix(ext)
        if candidate.exists():
            return candidate
    return None


def parse_texture_report(report_path: Path) -> list[tuple[str, str]]:
    """
    Returns list of (texture_name, ue_path) from the _textures.txt report.
    Skips header lines and empty lines.
    """
    entries = []
    current_name = ""
    with report_path.open("r", encoding="utf-8") as f:
        for line in f:
            stripped = line.strip()
            if not stripped or stripped.startswith("=") or stripped.startswith("Save") \
                    or stripped.startswith("Texture extraction") \
                    or stripped.startswith("Resources/"):
                continue
            if stripped.startswith("Source :"):
                ue_path = stripped[len("Source :"):].strip()
                if current_name and ue_path:
                    entries.append((current_name, ue_path))
            elif stripped.startswith("Used by:"):
                continue
            else:
                current_name = stripped
    return entries


def parse_args():
    parser = argparse.ArgumentParser(
        description="Copy texture files from Sandfall exports to level Textures folder."
    )
    parser.add_argument(
        "--level-dir",
        required=True,
        help="Level name (e.g. SmallLevel_SimonArea) or full path.",
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
        "--overwrite",
        action="store_true",
        help="Overwrite existing files in output folder.",
    )
    return parser.parse_args()


def main():
    args = parse_args()

    script_dir = Path(__file__).parent
    resources_dir = script_dir.parent / "Client" / "Bin" / "Resources"

    level_name = Path(args.level_dir).name
    sandfall_root = Path(args.sandfall_root)
    engine_root = Path(args.engine_root) if Path(args.engine_root).exists() else None

    if engine_root is None:
        print(f"  NOTE: Engine root not found, /Engine/ paths will be skipped: {args.engine_root}")

    report_path = resources_dir / "Map" / "NeedTextureJson" / f"{level_name}_textures.txt"
    out_dir = resources_dir / "Map" / level_name / "Textures"

    if not report_path.exists():
        raise FileNotFoundError(f"Texture report not found: {report_path}\n"
                                f"Run GenerateMapMaterials.py first.")
    if not sandfall_root.exists():
        raise FileNotFoundError(f"Sandfall root not found: {sandfall_root}")

    out_dir.mkdir(parents=True, exist_ok=True)

    entries = parse_texture_report(report_path)
    print(f"[1/2] Found {len(entries)} texture entries in: {report_path.name}")
    print(f"[2/2] Copying to: {out_dir}")

    copied = 0
    skipped = 0
    missing = []

    for tex_name, ue_path in entries:
        base_path = ue_path_to_disk_path(ue_path, sandfall_root, engine_root)
        if base_path is None:
            missing.append((tex_name, ue_path, "unsupported path prefix"))
            continue

        # Copy texture image file
        src_tex = find_texture_file(base_path)
        if src_tex is None:
            missing.append((tex_name, ue_path, f"no image file at {base_path}.*"))
        else:
            dst_tex = out_dir / src_tex.name
            if dst_tex.exists() and not args.overwrite:
                skipped += 1
            else:
                shutil.copy2(src_tex, dst_tex)
                copied += 1

        # Copy matching .json file if present
        src_json = base_path.with_suffix(".json")
        if src_json.exists():
            dst_json = out_dir / src_json.name
            if not dst_json.exists() or args.overwrite:
                shutil.copy2(src_json, dst_json)

    print(f"\nDone.")
    print(f"  Copied : {copied}")
    print(f"  Skipped: {skipped} (already exist, use --overwrite to replace)")
    print(f"  Missing: {len(missing)}")

    if missing:
        print(f"\n[!] Missing textures ({len(missing)}):")
        for tex_name, ue_path, reason in missing:
            print(f"  {tex_name}")
            print(f"    Source: {ue_path}")
            print(f"    Reason: {reason}")


if __name__ == "__main__":
    main()

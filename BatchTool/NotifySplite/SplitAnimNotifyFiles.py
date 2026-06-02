import argparse
import json
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parents[1]
DEFAULT_INPUT = REPO_ROOT / "Client/Bin/Resources/Data/json/AnimNotifies"


def sanitize_file_name(value: str) -> str:
    result = value
    for ch in '<>:"/\\|?*':
        result = result.replace(ch, "_")
    return result or "Animation"


def split_notify_file(source_path: Path, dry_run: bool) -> int:
    with source_path.open("r", encoding="utf-8-sig") as file:
        root = json.load(file)

    animations = root.get("animations", [])
    if not isinstance(animations, list):
        print(f"[skip] {source_path}: animations is not an array")
        return 0

    model_name = source_path.name.removesuffix(".notify.json")
    output_dir = source_path.parent / model_name

    if not dry_run:
        output_dir.mkdir(parents=True, exist_ok=True)

    written_count = 0
    seen_animation_names = set()
    for animation in animations:
        if not isinstance(animation, dict):
            continue

        animation_name = animation.get("animation_name", "")
        if not animation_name:
            continue

        if animation_name in seen_animation_names:
            print(f"[skip-duplicate] {source_path.name}: {animation_name}")
            continue
        seen_animation_names.add(animation_name)

        split_root = {
            "version": root.get("version", 2),
            "model_guid": root.get("model_guid", ""),
            "animations": [animation],
        }

        target_path = output_dir / f"{sanitize_file_name(animation_name)}.notify.json"
        if dry_run:
            print(f"[dry-run] {target_path}")
        else:
            with target_path.open("w", encoding="utf-8", newline="\n") as file:
                json.dump(split_root, file, indent=4, ensure_ascii=False)
                file.write("\n")
            print(f"[write] {target_path}")

        written_count += 1

    return written_count


def main() -> int:
    parser = argparse.ArgumentParser(description="Split model notify json files into one file per animation.")
    parser.add_argument("--input", default=DEFAULT_INPUT, type=Path, help="AnimNotifies folder path.")
    parser.add_argument("--dry-run", action="store_true", help="Print target files without writing.")
    args = parser.parse_args()

    input_dir = args.input.resolve()
    if not input_dir.exists():
        print(f"[error] input folder not found: {input_dir}")
        return 1

    total_files = 0
    total_animations = 0

    for source_path in sorted(input_dir.glob("*.notify.json")):
        count = split_notify_file(source_path, args.dry_run)
        total_files += 1
        total_animations += count

    print(f"[done] source_files={total_files}, split_animations={total_animations}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

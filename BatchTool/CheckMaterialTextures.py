"""
CheckMaterialTextures.py

Reads a .material.json file and reports all material instances
that have no textures assigned (all overrides false or all texture
paths empty).

Usage:
    python CheckMaterialTextures.py --level SmallLevel_MF_Zone_01
    python CheckMaterialTextures.py --file path/to/custom.material.json
"""

import argparse
import json
from pathlib import Path

RESOURCES_DIR = Path(__file__).parent.parent / "Client" / "Bin" / "Resources"


def check(material_path: Path) -> None:
    with open(material_path, encoding="utf-8") as f:
        data = json.load(f)

    instances = data.get("instances", [])
    empty = []

    for inst in instances:
        name = inst.get("assetName", "?")
        overrides = inst.get("overrides", {})
        textures = inst.get("textures", {})

        all_overrides_false = not any(overrides.values())
        all_textures_empty = all(v == "" for v in textures.values())

        if all_overrides_false or all_textures_empty:
            empty.append((name, overrides, textures))

    total = len(instances)
    count = len(empty)

    print(f"파일  : {material_path}")
    print(f"전체  : {total}개  /  텍스처 없음: {count}개")
    print("=" * 60)

    if not empty:
        print("텍스처가 비어 있는 항목이 없습니다.")
        return

    for name, overrides, textures in empty:
        bc  = textures.get("baseColor", "")
        n   = textures.get("normal", "")
        orm = textures.get("orm", "")
        print(f"[{name}]")
        print(f"  baseColor  override={overrides.get('baseColor')}  \"{bc}\"")
        print(f"  normal     override={overrides.get('normal')}  \"{n}\"")
        if overrides.get("orm") is not None:
            print(f"  orm        override={overrides.get('orm')}  \"{orm}\"")
        print()


def main() -> None:
    parser = argparse.ArgumentParser(description="material.json 텍스처 누락 검사")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--level", metavar="LEVEL_NAME",
                       help="레벨 이름 (예: SmallLevel_MF_Zone_01)")
    group.add_argument("--file", metavar="PATH",
                       help="material.json 파일 직접 지정")
    args = parser.parse_args()

    if args.level:
        material_path = RESOURCES_DIR / "Materials" / f"{args.level}.material.json"
    else:
        material_path = Path(args.file)

    if not material_path.exists():
        print(f"파일을 찾을 수 없습니다: {material_path}")
        raise SystemExit(1)

    check(material_path)


if __name__ == "__main__":
    main()

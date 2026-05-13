"""
OriginJson 파일을 읽어 필요한 모델 파일을 FModel 익스포트 폴더에서 찾아
지정한 목적지 폴더로 복사합니다.

경로 변환:
  /Game/X/Y/SM_Name.2  →  {EXPORT_ROOT}/Content/X/Y/SM_Name.*

Usage:
  python list_models.py <OriginJson 파일명> [--export D:/path/to/Exports/Sandfall] [--dest ../Client/Bin/Resources/Map/MyMap]

예시:
  python list_models.py SmallLevel_SimonArea.json
  python list_models.py SmallLevel_SimonArea.json --export "D:/Clair Obscur Expedition 33/Exports/Sandfall"
  python list_models.py SmallLevel_SimonArea.json --dest ../Client/Bin/Resources/Map/SmallLevel_SimonArea
"""
import json
import os
import glob
import shutil
import argparse

ORIGIN_DIR   = "../Client/Bin/Resources/Map/OriginJson"
OUT_DIR      = "../Client/Bin/Resources/Map/NeedModelJson"
DEFAULT_EXPORT_ROOT = "D:/Clair Obscur Expedition 33/Exports/Sandfall"


def ue5_path_to_content_relative(object_path: str) -> tuple[str, str]:
    """
    /Game/X/Y/SM_Name.2  →  (Content/X/Y,  SM_Name)
    반환값: (폴더 상대경로, 파일 stem)
    """
    # 끝의 .숫자 제거
    path = object_path.rsplit(".", 1)[0] if "." in object_path.rsplit("/", 1)[-1] else object_path

    # /Game/ → Content/
    if path.startswith("/Game/"):
        path = "Content/" + path[len("/Game/"):]
    elif path.startswith("Game/"):
        path = "Content/" + path[len("Game/"):]

    folder = "/".join(path.split("/")[:-1])
    stem   = path.split("/")[-1]
    return folder, stem


def get_mesh_object_paths(origin_path: str) -> set[str]:
    """OriginJson + _Generated_ 에서 StaticMesh ObjectPath 수집"""
    paths = set()

    def extract(root: list):
        for comp in root:
            comp_type = comp.get("Type", "")
            if comp_type in ("StaticMeshComponent", "InstancedStaticMeshComponent", "FoliageInstancedStaticMeshComponent"):
                sm = comp.get("Properties", {}).get("StaticMesh")
                if sm:
                    paths.add(sm["ObjectPath"])

    with open(origin_path, "r", encoding="utf-8") as f:
        root = json.load(f)
    if isinstance(root, list):
        extract(root)

    map_name      = os.path.splitext(os.path.basename(origin_path))[0]
    generated_dir = os.path.join(os.path.dirname(origin_path), map_name, "_Generated_")
    if os.path.isdir(generated_dir):
        for gen_file in glob.glob(os.path.join(generated_dir, "*.json")):
            with open(gen_file, "r", encoding="utf-8") as f:
                gen_root = json.load(f)
            if isinstance(gen_root, list):
                extract(gen_root)

    return paths


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("filename",
                        help="OriginJson 폴더 안의 파일명 (예: SmallLevel_SimonArea.json)")
    parser.add_argument("--export", default=DEFAULT_EXPORT_ROOT,
                        help=f"FModel 익스포트 루트 폴더 (기본값: {DEFAULT_EXPORT_ROOT})")
    parser.add_argument("--dest", default="",
                        help="복사 목적지 폴더 (기본값: Client/Bin/Resources/Map/<맵이름>)")
    args = parser.parse_args()

    script_dir  = os.path.dirname(os.path.abspath(__file__))
    origin_path = os.path.join(script_dir, ORIGIN_DIR, args.filename)

    if not os.path.exists(origin_path):
        print(f"오류: 파일을 찾을 수 없습니다 → {origin_path}")
        return

    map_name = os.path.splitext(args.filename)[0]
    dest_dir = args.dest if args.dest else os.path.join(
        script_dir, "../Client/Bin/Resources/Map", map_name)
    dest_dir = os.path.normpath(dest_dir)
    os.makedirs(dest_dir, exist_ok=True)

    export_root = os.path.normpath(args.export)

    print(f"OriginJson: {origin_path}")
    print(f"익스포트 루트: {export_root}")
    print(f"복사 목적지: {dest_dir}")
    print()

    object_paths = get_mesh_object_paths(origin_path)
    print(f"고유 메시: {len(object_paths)}종\n")

    results      = []
    copied_count = 0
    missing_count = 0

    for obj_path in sorted(object_paths):
        folder_rel, stem = ue5_path_to_content_relative(obj_path)
        search_dir = os.path.join(export_root, folder_rel.replace("/", os.sep))
        pattern    = os.path.join(search_dir, stem + ".*")
        found_files = glob.glob(pattern)

        if found_files:
            copied = []
            for src in found_files:
                dst = os.path.join(dest_dir, os.path.basename(src))
                shutil.copy2(src, dst)
                copied.append(os.path.basename(src))
                copied_count += 1
            results.append({
                "UE5Path":    obj_path,
                "SearchDir":  search_dir,
                "Stem":       stem,
                "Found":      True,
                "CopiedFiles": copied,
            })
        else:
            missing_count += 1
            results.append({
                "UE5Path":   obj_path,
                "SearchDir": search_dir,
                "Stem":      stem,
                "Found":     False,
                "CopiedFiles": [],
            })

    # 결과 JSON 저장
    output = {
        "MapName":      map_name,
        "ExportRoot":   str(export_root),
        "DestDir":      str(dest_dir),
        "TotalMeshes":  len(results),
        "MissingCount": missing_count,
        "CopiedFiles":  copied_count,
        "Models":       results,
    }

    out_dir  = os.path.normpath(os.path.join(script_dir, OUT_DIR))
    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, f"{map_name}_models.json")
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(output, f, indent=4, ensure_ascii=False)

    # 콘솔 요약
    print(f"총 메시:   {len(results)}종")
    print(f"복사 완료: {len(results) - missing_count}종 ({copied_count}개 파일)")
    print(f"누락:      {missing_count}종")

    if missing_count:
        print("\n[누락 메시]")
        for r in results:
            if not r["Found"]:
                print(f"  X {r['Stem']}  ({r['SearchDir']})")

    print(f"\n결과 JSON: {out_path}")


if __name__ == "__main__":
    main()

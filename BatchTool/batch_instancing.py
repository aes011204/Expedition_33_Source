"""
SaveJson의 StaticMeshes 중 같은 모델이 threshold 이상인 것을
InstancedMeshes로 묶어 새 SaveJson을 생성합니다.

Usage:
  python batch_instancing.py <SaveJson 파일명> [--threshold 5] [--out 출력파일명]

예시:
  python batch_instancing.py Level_Main_GestralVillage_V2.json
  python batch_instancing.py Level_Main_GestralVillage_V2.json --threshold 3
  python batch_instancing.py Level_Main_GestralVillage_V2.json --out Level_Main_GestralVillage_V2_Batched.json
"""
import json
import argparse
import os
from collections import defaultdict

SAVE_DIR = "../Client/Bin/Resources/Map/SaveJson"


def batch(input_path: str, output_path: str, threshold: int):
    with open(input_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    map_data = data["MapData"]
    static_meshes   = map_data.get("StaticMeshes", [])
    instanced_meshes = map_data.get("InstancedMeshes", [])

    # ModelPath 기준으로 그룹화 (NoBatch=true 제외)
    groups = defaultdict(list)
    no_batch = []

    for item in static_meshes:
        if item.get("NoBatch", False):
            no_batch.append(item)
            continue
        model_path = item.get("ModelPath", "")
        if model_path:
            groups[model_path].append(item)

    remaining_statics = list(no_batch)
    new_instanced = []
    batched_count = 0

    for model_path, items in groups.items():
        if len(items) < threshold:
            remaining_statics.extend(items)
            continue

        entry = {
            "Id": model_path.replace(".model", "") + "_AutoBatch",
            "ModelPath": model_path,
            "Instances": [],
        }

        for item in items:
            entry["Instances"].append({
                "Position": item.get("Position", [0.0, 0.0, 0.0]),
                "Rotation": item.get("Rotation", [0.0, 0.0, 0.0, 1.0]),
                "Scale":    item.get("Scale",    [1.0, 1.0, 1.0]),
            })

        new_instanced.append(entry)
        batched_count += len(items)

    map_data["StaticMeshes"]   = remaining_statics
    map_data["InstancedMeshes"] = instanced_meshes + new_instanced

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=4, ensure_ascii=False)

    print(f"완료")
    print(f"  입력  : {input_path}")
    print(f"  출력  : {output_path}")
    print(f"  threshold : {threshold}개 이상")
    print(f"  StaticMesh  {len(static_meshes)}개 → {len(remaining_statics)}개 (배칭된 {batched_count}개)")
    print(f"  InstancedMesh {len(instanced_meshes)}종 → {len(instanced_meshes) + len(new_instanced)}종 ({len(new_instanced)}종 추가)")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("filename", help="SaveJson 폴더 안의 파일명 (예: Level_Main_GestralVillage_V2.json)")
    parser.add_argument("--threshold", type=int, default=5, help="배칭 최소 인스턴스 수 (기본값: 5)")
    parser.add_argument("--out", type=str, default="", help="출력 파일명 (기본값: 원본명_Batched.json)")
    args = parser.parse_args()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    input_path = os.path.join(script_dir, SAVE_DIR, args.filename)

    if not os.path.exists(input_path):
        print(f"오류: 파일을 찾을 수 없습니다 → {input_path}")
        return

    if args.out:
        out_filename = args.out
    else:
        base, ext = os.path.splitext(args.filename)
        out_filename = f"{base}_Batched{ext}"

    output_path = os.path.join(script_dir, SAVE_DIR, out_filename)

    batch(input_path, output_path, args.threshold)


if __name__ == "__main__":
    main()

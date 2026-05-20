"""
split_map.py — SaveJson을 XZ 격자로 분할

사용법:
    python split_map.py [--input PATH] [--output DIR] [--grid NxM]

기본값:
    --input   Client/Bin/Resources/Map/SaveJson/Level_SpringMeadows_Main_V2.json
    --output  Client/Bin/Resources/Map/SaveJson/Chunks/Level_SpringMeadows_Main_V2/
    --grid    4x4

출력:
    chunk_ROW_COL.json   — 각 청크의 맵 데이터 (MapLoader 호환 포맷)
    manifest.json        — 청크 레이아웃 및 바운딩박스 정보
    (stdout)             — ASCII 격자로 청크별 오브젝트 수 시각화
"""

import json
import math
import os
import argparse
from pathlib import Path

DEFAULT_INPUT = r"../Client/Bin/Resources/Map/SaveJson/Level_SpringMeadows_Main_V2.json"
DEFAULT_OUTPUT = r"../Client/Bin/Resources/Map/SaveJson/Chunks/Level_SpringMeadows_Main_V2"


def get_position(item):
    """StaticMesh 또는 Instance에서 (x, z) 반환"""
    p = item["Position"]
    return p[0], p[2]


def chunk_index(x, z, x_min, z_min, x_step, z_step, cols, rows):
    col = min(int((x - x_min) / x_step), cols - 1)
    row = min(int((z - z_min) / z_step), rows - 1)
    return row, col


def main():
    parser = argparse.ArgumentParser(description="SaveJson 격자 분할기")
    parser.add_argument("--input", default=DEFAULT_INPUT)
    parser.add_argument("--output", default=DEFAULT_OUTPUT)
    parser.add_argument("--grid", default="4x4", help="ColsxRows (예: 4x4, 3x3)")
    args = parser.parse_args()

    grid_cols, grid_rows = map(int, args.grid.lower().split("x"))

    input_path = Path(args.input)
    output_dir = Path(args.output)

    print(f"[split_map] 입력: {input_path}")
    print(f"[split_map] 출력: {output_dir}")
    print(f"[split_map] 격자: {grid_cols}열 × {grid_rows}행")
    print(f"[split_map] JSON 파싱 중... (252MB, 시간 소요)")

    with open(input_path, "r", encoding="utf-8") as f:
        root = json.load(f)

    map_data = root["MapData"]
    map_name = map_data.get("MapName", "Unknown")
    model_folder = map_data.get("ModelFolder", "")

    static_meshes = map_data.get("StaticMeshes", [])
    instanced_meshes = map_data.get("InstancedMeshes", [])

    # 비공간 데이터 (전체 청크에 복사)
    spawn_points = map_data.get("SpawnPoints", [])
    encounter_zones = map_data.get("EncounterZones", [])
    trigger_volumes = map_data.get("TriggerVolumes", [])
    camera_zones = map_data.get("CameraZones", [])
    post_process_volumes = map_data.get("PostProcessVolumes", [])
    lights = map_data.get("Lights", [])
    model_overrides = map_data.get("ModelOverrides", {})

    # 바운딩박스 계산
    print("[split_map] 바운딩박스 계산 중...")
    all_x, all_z = [], []
    for item in static_meshes:
        x, z = get_position(item)
        all_x.append(x); all_z.append(z)
    for im in instanced_meshes:
        for inst in im.get("Instances", []):
            x, z = get_position(inst)
            all_x.append(x); all_z.append(z)

    x_min, x_max = min(all_x), max(all_x)
    z_min, z_max = min(all_z), max(all_z)

    # 경계에 여유 추가 (엣지 오브젝트가 마지막 청크 바깥으로 나가지 않도록)
    x_min -= 1.0; z_min -= 1.0
    x_max += 1.0; z_max += 1.0

    x_span = x_max - x_min
    z_span = z_max - z_min
    x_step = x_span / grid_cols
    z_step = z_span / grid_rows

    print(f"[split_map] X: {x_min:.0f} ~ {x_max:.0f} ({x_span:.0f} 유닛), 청크폭: {x_step:.0f}")
    print(f"[split_map] Z: {z_min:.0f} ~ {z_max:.0f} ({z_span:.0f} 유닛), 청크깊이: {z_step:.0f}")

    # 청크 초기화
    chunks = {}  # (row, col) -> {"StaticMeshes": [], "InstancedMeshes_map": {}}
    for row in range(grid_rows):
        for col in range(grid_cols):
            chunks[(row, col)] = {"StaticMeshes": [], "InstancedMeshes_map": {}}

    # StaticMesh 분배
    print("[split_map] StaticMesh 분배 중...")
    for item in static_meshes:
        x, z = get_position(item)
        row, col = chunk_index(x, z, x_min, z_min, x_step, z_step, grid_cols, grid_rows)
        chunks[(row, col)]["StaticMeshes"].append(item)

    # InstancedMesh 분배 (모델별로 인스턴스를 쪼갬)
    print("[split_map] InstancedMesh 분배 중...")
    for im in instanced_meshes:
        model_id = im["Id"]
        model_path = im["ModelPath"]
        for inst in im.get("Instances", []):
            x, z = get_position(inst)
            row, col = chunk_index(x, z, x_min, z_min, x_step, z_step, grid_cols, grid_rows)
            im_map = chunks[(row, col)]["InstancedMeshes_map"]
            if model_id not in im_map:
                im_map[model_id] = {"Id": model_id, "ModelPath": model_path, "Instances": []}
            im_map[model_id]["Instances"].append(inst)

    # 출력 디렉토리 생성
    output_dir.mkdir(parents=True, exist_ok=True)

    # 청크 파일 저장
    print("[split_map] 청크 파일 저장 중...")
    manifest_chunks = []
    sm_count_grid = [[0] * grid_cols for _ in range(grid_rows)]
    inst_count_grid = [[0] * grid_cols for _ in range(grid_rows)]

    for row in range(grid_rows):
        for col in range(grid_cols):
            key = (row, col)
            chunk = chunks[key]
            sm_list = chunk["StaticMeshes"]
            im_list = list(chunk["InstancedMeshes_map"].values())

            total_instances = sum(len(im["Instances"]) for im in im_list)
            sm_count_grid[row][col] = len(sm_list)
            inst_count_grid[row][col] = total_instances

            chunk_name = f"chunk_{row:02d}_{col:02d}"
            filename = f"{chunk_name}.json"
            filepath = output_dir / filename

            chunk_x_min = x_min + col * x_step
            chunk_x_max = chunk_x_min + x_step
            chunk_z_min = z_min + row * z_step
            chunk_z_max = chunk_z_min + z_step

            chunk_data = {
                "MapData": {
                    "MapName": f"{map_name}_{chunk_name}",
                    "ModelFolder": model_folder,
                    "StaticMeshes": sm_list,
                    "InstancedMeshes": im_list,
                    "SpawnPoints": spawn_points,
                    "EncounterZones": encounter_zones,
                    "TriggerVolumes": trigger_volumes,
                    "CameraZones": camera_zones,
                    "PostProcessVolumes": post_process_volumes,
                    "Lights": lights,
                }
            }
            if model_overrides:
                chunk_data["MapData"]["ModelOverrides"] = model_overrides

            with open(filepath, "w", encoding="utf-8") as f:
                json.dump(chunk_data, f, ensure_ascii=False, separators=(",", ":"))

            file_size_mb = filepath.stat().st_size / (1024 * 1024)

            manifest_chunks.append({
                "file": filename,
                "row": row,
                "col": col,
                "x_min": round(chunk_x_min, 2),
                "x_max": round(chunk_x_max, 2),
                "z_min": round(chunk_z_min, 2),
                "z_max": round(chunk_z_max, 2),
                "static_mesh_count": len(sm_list),
                "instanced_mesh_models": len(im_list),
                "total_instances": total_instances,
                "size_mb": round(file_size_mb, 2),
            })

            if len(sm_list) > 0 or total_instances > 0:
                print(f"  [{row},{col}] {filename} | SM:{len(sm_list):5d} | 인스턴스:{total_instances:7d} | {file_size_mb:.1f}MB")

    # manifest 저장
    manifest = {
        "source": str(input_path.name),
        "grid_cols": grid_cols,
        "grid_rows": grid_rows,
        "bounds": {"x_min": round(x_min, 2), "x_max": round(x_max, 2),
                   "z_min": round(z_min, 2), "z_max": round(z_max, 2)},
        "chunk_size": {"x": round(x_step, 2), "z": round(z_step, 2)},
        "chunks": manifest_chunks,
    }
    with open(output_dir / "manifest.json", "w", encoding="utf-8") as f:
        json.dump(manifest, f, ensure_ascii=False, indent=2)

    # ASCII 격자 시각화 (인스턴스 수 기준)
    print()
    print("=== 청크별 총 오브젝트 수 (SM + 인스턴스) ===")
    print(f"    {'  '.join(f'C{c:02d}' for c in range(grid_cols))}")
    for row in range(grid_rows - 1, -1, -1):  # Z 큰 쪽이 위
        cells = []
        for col in range(grid_cols):
            total = sm_count_grid[row][col] + inst_count_grid[row][col]
            if total == 0:
                cells.append(" --- ")
            elif total < 10000:
                cells.append(f"{total:5d}")
            else:
                cells.append(f"{total//1000:4d}k")
        print(f"R{row:02d} {'  '.join(cells)}")

    print()
    print(f"[split_map] 완료. manifest.json 포함 {grid_rows * grid_cols + 1}개 파일 → {output_dir}")
    print(f"[split_map] 에디터에서 원하는 chunk_ROW_COL.json 파일을 로드하면 됩니다.")


if __name__ == "__main__":
    main()
"""
UE5 OriginJson의 _Generated_ 폴더에서 지형(Landscape) 타일 정보를 추출해
SaveJson 포맷의 terrain 전용 파일을 생성합니다.

Usage:
  python extract_terrain.py <맵 이름>

예시:
  python extract_terrain.py Level_SpringMeadows_Main_V2
"""
import json
import os
import glob
import math
import argparse

ORIGIN_DIR = "../Client/Bin/Resources/Map/OriginJson"
SAVE_DIR   = "../Client/Bin/Resources/Map/SaveJson"

# MapEditor 실행 경로(MapEditor/Bin/Debug/)에서의 상대 경로
RUNTIME_ORIGIN_PREFIX = "../../../Client/Bin/Resources/Map/OriginJson"


def parse_landscape_transform(origin_path: str) -> dict | None:
    """OriginJson에서 Landscape RootComponent의 위치/크기를 파싱해 게임 좌표로 변환합니다."""
    with open(origin_path, "r", encoding="utf-8") as f:
        root = json.load(f)

    for obj in root:
        if obj.get("Name", "") != "RootComponent0":
            continue
        outer = obj.get("Outer", {})
        if not isinstance(outer, dict):
            continue
        if "Landscape" not in outer.get("ObjectName", ""):
            continue

        props = obj.get("Properties", {})
        loc = props.get("RelativeLocation")
        scl = props.get("RelativeScale3D")
        if not loc or not scl:
            continue

        # UE → 게임 좌표 변환 (Build_TerrainFromScene 기준)
        # UE: X=앞, Y=오른쪽, Z=위  →  Game: X=앞, Y=위, Z=오른쪽
        ue_loc_x, ue_loc_y, ue_loc_z = loc["X"], loc["Y"], loc["Z"]
        ue_scl_x, ue_scl_z           = scl["X"], scl["Z"]

        world_offset_x = ue_loc_x * 0.01
        world_offset_y = ue_loc_z * 0.01
        world_offset_z = ue_loc_y * 0.01
        scale_xz       = ue_scl_x * 0.01
        scale_y = ue_scl_z * 0.01

        return {
            "WorldOffsetX": round(world_offset_x, 4),
            "WorldOffsetY": round(world_offset_y, 4),
            "WorldOffsetZ": round(world_offset_z, 4),
            "ScaleXZ":      round(scale_xz, 6),
            "ScaleY":       round(scale_y, 4),
        }

    return None


def find_heightmaps(generated_dir: str) -> list:
    pattern = os.path.join(generated_dir, "Heightmap_*.png")
    files = glob.glob(pattern)

    def get_num(path):
        name = os.path.splitext(os.path.basename(path))[0]  # "Heightmap_0"
        return int(name.split("_")[-1])

    return sorted(files, key=get_num)


def build_tiles(heightmap_files: list, grid_cols: int) -> list:
    tiles = []
    for i, filepath in enumerate(heightmap_files):
        filename = os.path.basename(filepath)
        tiles.append({
            "File": filename,
            "GridX": i % grid_cols,
            "GridY": i // grid_cols,
        })
    return tiles


def extract_terrain(map_name: str):
    script_dir    = os.path.dirname(os.path.abspath(__file__))
    origin_path   = os.path.join(script_dir, ORIGIN_DIR, f"{map_name}.json")
    generated_dir = os.path.join(script_dir, ORIGIN_DIR, map_name, "_Generated_")
    save_path     = os.path.join(script_dir, SAVE_DIR, f"{map_name}_terrain.json")

    if not os.path.exists(origin_path):
        print(f"오류: OriginJson을 찾을 수 없습니다 → {origin_path}")
        return

    if not os.path.isdir(generated_dir):
        print(f"오류: _Generated_ 폴더가 없습니다 → {generated_dir}")
        return

    # 1. Landscape 위치/크기 파싱
    print("Landscape 트랜스폼 파싱 중...")
    transform = parse_landscape_transform(origin_path)
    if not transform:
        print("오류: OriginJson에서 Landscape RootComponent를 찾을 수 없습니다.")
        return
    print(f"  WorldOffset: ({transform['WorldOffsetX']}, {transform['WorldOffsetY']}, {transform['WorldOffsetZ']})")
    print(f"  ScaleXZ: {transform['ScaleXZ']},  ScaleY: {transform['ScaleY']}")

    # 2. 하이트맵 파일 목록
    heightmap_files = find_heightmaps(generated_dir)
    if not heightmap_files:
        print(f"오류: Heightmap_*.png 파일이 없습니다 → {generated_dir}")
        return

    count     = len(heightmap_files)
    grid_cols = int(math.sqrt(count))
    if grid_cols * grid_cols != count:
        print(f"경고: 하이트맵 {count}개는 정사각형 그리드가 아닙니다. cols={grid_cols} 로 가정합니다.")
    else:
        print(f"하이트맵 {count}개 발견 → {grid_cols}x{grid_cols} 그리드")

    heightmap_folder_runtime = f"{RUNTIME_ORIGIN_PREFIX}/{map_name}/_Generated_/"

    output = {
        "Terrain": {
            **transform,
            "HeightmapFolder": heightmap_folder_runtime,
            "Tiles":           build_tiles(heightmap_files, grid_cols),
        }
    }

    os.makedirs(os.path.dirname(save_path), exist_ok=True)
    with open(save_path, "w", encoding="utf-8") as f:
        json.dump(output, f, indent=4, ensure_ascii=False)

    print(f"완료: {save_path}")
    print(f"  HeightmapFolder: {heightmap_folder_runtime}")
    print(f"  타일 수:          {count}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("mapname", help="맵 이름 (예: Level_SpringMeadows_Main_V2)")
    args = parser.parse_args()
    extract_terrain(args.mapname)


if __name__ == "__main__":
    main()
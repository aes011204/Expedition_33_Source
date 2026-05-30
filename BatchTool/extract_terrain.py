"""
UE5 OriginJson의 _Generated_ 폴더에서 지형(Landscape) 타일 정보를 추출해
SaveJson 포맷의 terrain 전용 파일을 생성합니다.

각 LandscapeStreamingProxy는 별도 해시 폴더에 Heightmap_N.png를 가집니다.
SectionBaseX/Y로 실제 그리드 위치를 계산합니다.

Usage:
  python extract_terrain.py <맵 이름>

예시:
  python extract_terrain.py SmallLevel_MF_Zone_01
"""
import json
import os
import glob
import argparse
import re

ORIGIN_DIR = "../Client/Bin/Resources/Map/OriginJson"
SAVE_DIR   = "../Client/Bin/Resources/Map/SaveJson"

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

        ue_loc_x, ue_loc_y, ue_loc_z = loc["X"], loc["Y"], loc["Z"]
        ue_scl_x, ue_scl_z           = scl["X"], scl["Z"]

        return {
            "WorldOffsetX": round(ue_loc_x * 0.01, 4),
            "WorldOffsetY": round(ue_loc_z * 0.01, 4),
            "WorldOffsetZ": round(ue_loc_y * 0.01, 4),
            "ScaleXZ":      round(ue_scl_x * 0.01, 6),
            "ScaleY":       round(ue_scl_z * 0.01, 4),
        }

    return None


def extract_proxy_hash(object_path: str) -> str | None:
    """ObjectPath에서 proxy 해시 이름을 추출합니다.
    예: '/Game/.../_Generated_/1E1H9PJ7T5XMX7ZF9XMBDABHT.19' → '1E1H9PJ7T5XMX7ZF9XMBDABHT'
    """
    match = re.search(r'_Generated_/([^./]+)', object_path)
    return match.group(1) if match else None


def parse_landscape_components(generated_dir: str) -> list:
    """_Generated_ 폴더의 JSON 파일에서 LandscapeComponent 데이터를 파싱합니다.
    각 컴포넌트의 SectionBaseX/Y와 proxy별 Heightmap 파일 경로를 반환합니다."""
    components = []
    json_files = glob.glob(os.path.join(generated_dir, "*.json"))

    for json_file in json_files:
        with open(json_file, "r", encoding="utf-8") as f:
            data = json.load(f)
        if not isinstance(data, list):
            continue

        for item in data:
            if item.get("Type") != "LandscapeComponent":
                continue

            props = item.get("Properties", {})
            section_base_x = props.get("SectionBaseX")
            section_base_y = props.get("SectionBaseY")
            hm_texture     = props.get("HeightmapTexture", {})

            if section_base_x is None or section_base_y is None:
                continue

            hm_obj_name = hm_texture.get("ObjectName", "")
            hm_obj_path = hm_texture.get("ObjectPath", "")

            hm_match = re.search(r'(Heightmap_\d+)', hm_obj_name)
            if not hm_match:
                continue

            proxy_hash = extract_proxy_hash(hm_obj_path)
            if not proxy_hash:
                continue

            components.append({
                "SectionBaseX":  section_base_x,
                "SectionBaseY":  section_base_y,
                "HeightmapFile": f"{proxy_hash}/{hm_match.group(1)}.png",
            })

    return components


def build_tiles(components: list, component_size_quads: int = 126) -> list:
    """SectionBaseX/Y 기준으로 정확한 GridX/GridY를 계산해 타일 목록을 반환합니다."""
    if not components:
        return []

    min_x = min(c["SectionBaseX"] for c in components)
    min_y = min(c["SectionBaseY"] for c in components)

    tiles = []
    for c in sorted(components, key=lambda x: (x["SectionBaseY"], x["SectionBaseX"])):
        tiles.append({
            "File":  c["HeightmapFile"],
            "GridX": (c["SectionBaseX"] - min_x) // component_size_quads,
            "GridY": (c["SectionBaseY"] - min_y) // component_size_quads,
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

    print("Landscape 트랜스폼 파싱 중...")
    transform = parse_landscape_transform(origin_path)
    if not transform:
        print("오류: OriginJson에서 Landscape RootComponent를 찾을 수 없습니다.")
        return
    print(f"  WorldOffset: ({transform['WorldOffsetX']}, {transform['WorldOffsetY']}, {transform['WorldOffsetZ']})")
    print(f"  ScaleXZ: {transform['ScaleXZ']},  ScaleY: {transform['ScaleY']}")

    print("LandscapeComponent 파싱 중...")
    components = parse_landscape_components(generated_dir)
    if not components:
        print("오류: LandscapeComponent를 찾을 수 없습니다.")
        return
    print(f"  컴포넌트 {len(components)}개 발견")

    tiles = build_tiles(components)
    grid_w = max(t["GridX"] for t in tiles) + 1
    grid_h = max(t["GridY"] for t in tiles) + 1
    print(f"  그리드: {grid_w}x{grid_h} ({len(tiles)}타일)")

    output = {
        "Terrain": {
            **transform,
            "HeightmapFolder": f"{RUNTIME_ORIGIN_PREFIX}/{map_name}/_Generated_/",
            "Tiles": tiles,
        }
    }

    os.makedirs(os.path.dirname(save_path), exist_ok=True)
    with open(save_path, "w", encoding="utf-8") as f:
        json.dump(output, f, indent=4, ensure_ascii=False)

    print(f"완료: {save_path}")
    print(f"  타일 수: {len(tiles)}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("mapname", help="맵 이름 (예: SmallLevel_MF_Zone_01)")
    args = parser.parse_args()
    extract_terrain(args.mapname)


if __name__ == "__main__":
    main()

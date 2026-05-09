"""
UE5 OriginJson을 읽어 스케일을 보정한 뒤 SaveJson 포맷으로 저장합니다.
_Generated_ 서브폴더가 있으면 World Partition 스트리밍 셀도 함께 처리합니다.

Usage:
  python scale_map.py <OriginJson 파일명>  [--factor 0.001]

예시:
  python scale_map.py SmallLevel_SimonArea.json
  python scale_map.py SmallLevel_SimonArea.json --factor 0.01
"""
import json
import math
import argparse
import os
import glob

ORIGIN_DIR = "../Client/Bin/Resources/Map/OriginJson"
SAVE_DIR   = "../Client/Bin/Resources/Map/SaveJson"


# ── UE → DX 좌표 변환 (MEUe5Importer 동일 로직) ──────────────────────────

def convert_translation(x, y, z, factor):
    # UE cm → 엔진 단위(/100), Y↔Z 스왑, 스케일 보정
    return [x / 100.0 * factor, z / 100.0 * factor, y / 100.0 * factor]

def convert_scale(x, y, z, factor):
    # Y↔Z 스왑 + factor 균등 적용 (위치와 동일 배율)
    return [x * factor, z * factor, y * factor]

def euler_to_quat(pitch_deg, yaw_deg, roll_deg):
    # UE Pitch/Yaw/Roll → DX Quat (MEUe5Importer::ParseSingle 동일)
    p = math.radians(-pitch_deg)
    y = math.radians(-yaw_deg)
    r = math.radians(roll_deg)

    cp, sp = math.cos(p * 0.5), math.sin(p * 0.5)
    cy, sy = math.cos(y * 0.5), math.sin(y * 0.5)
    cr, sr = math.cos(r * 0.5), math.sin(r * 0.5)

    # qp = AxisAngle(Right,  p)  → (sp, 0, 0, cp)
    # qy = AxisAngle(Up,     y)  → (0, sy, 0, cy)
    # qr = AxisAngle(Forward,r)  → (0, 0, sr, cr)
    # result = qr * qp * qy
    qpx, qpy, qpz, qpw = sp, 0.0, 0.0, cp
    qyx, qyy, qyz, qyw = 0.0, sy,  0.0, cy
    qrx, qry, qrz, qrw = 0.0, 0.0, sr,  cr

    # qr * qp
    ax = qrw*qpx + qrx*qpw + qry*qpz - qrz*qpy
    ay = qrw*qpy - qrx*qpz + qry*qpw + qrz*qpx
    az = qrw*qpz + qrx*qpy - qry*qpx + qrz*qpw
    aw = qrw*qpw - qrx*qpx - qry*qpy - qrz*qpz

    # (qr*qp) * qy
    rx = aw*qyx + ax*qyw + ay*qyz - az*qyy
    ry = aw*qyy - ax*qyz + ay*qyw + az*qyx
    rz = aw*qyz + ax*qyy - ay*qyx + az*qyw
    rw = aw*qyw - ax*qyx - ay*qyy - az*qyz

    return [rx, ry, rz, rw]

def convert_quat(qx, qy, qz, qw):
    # MEUe5Importer::ConvertRotation: (X, Z, Y, -W)
    return [qx, qz, qy, -qw]

def extract_mesh_name(object_path: str) -> str:
    filename = object_path.rsplit("/", 1)[-1]
    if "." in filename:
        filename = filename.rsplit(".", 1)[0]
    return filename


# ── 컴포넌트 파싱 ────────────────────────────────────────────────────────

def parse_single(comp: dict, global_index: int, factor: float) -> dict | None:
    props = comp.get("Properties", {})
    if "StaticMesh" not in props:
        return None

    loc = props.get("RelativeLocation", {"X": 0.0, "Y": 0.0, "Z": 0.0})
    pos = convert_translation(loc["X"], loc["Y"], loc["Z"], factor)

    rot_raw = props.get("RelativeRotation")
    if rot_raw:
        quat = euler_to_quat(rot_raw["Pitch"], rot_raw["Yaw"], rot_raw["Roll"])
    else:
        quat = [0.0, 0.0, 0.0, 1.0]

    sc = props.get("RelativeScale3D", {"X": 1.0, "Y": 1.0, "Z": 1.0})
    scale = convert_scale(sc["X"], sc["Y"], sc["Z"], factor)

    name = extract_mesh_name(props["StaticMesh"]["ObjectPath"])
    return {
        "Id": f"{name}_{global_index}",
        "Position": pos,
        "Rotation": quat,
        "Scale": scale,
        "ModelPath": f"{name}.model",
    }

def parse_instanced(comp: dict, instance_index: int, global_index: int, factor: float) -> dict | None:
    instances = comp.get("PerInstanceSMData", [])
    if instance_index >= len(instances):
        return None
    td = instances[instance_index].get("TransformData")
    if not td:
        return None

    rot = td["Rotation"]
    tr  = td["Translation"]
    sc  = td["Scale3D"]

    pos   = convert_translation(tr["X"], tr["Y"], tr["Z"], factor)
    quat  = convert_quat(rot["X"], rot["Y"], rot["Z"], rot["W"])
    scale = convert_scale(sc["X"], sc["Y"], sc["Z"], factor)

    props = comp.get("Properties", {})
    if "StaticMesh" not in props:
        return None

    name = extract_mesh_name(props["StaticMesh"]["ObjectPath"])
    return {
        "Id": f"{name}_{global_index}",
        "Position": pos,
        "Rotation": quat,
        "Scale": scale,
        "ModelPath": f"{name}.model",
    }


# ── 메인 변환 ─────────────────────────────────────────────────────────────

def convert(origin_path: str, save_path: str, factor: float, model_folder: str = ""):
    with open(origin_path, "r", encoding="utf-8") as f:
        root = json.load(f)

    if not isinstance(root, list):
        print("오류: OriginJson 형식이 아닙니다 (배열이어야 합니다).")
        return

    map_name = os.path.splitext(os.path.basename(origin_path))[0]
    if not model_folder:
        model_folder = map_name

    static_meshes = []
    global_index = 0

    # ── 메인 JSON 처리 ──
    for comp in root:
        comp_type = comp.get("Type", "")

        if comp_type == "StaticMeshComponent":
            obj = parse_single(comp, global_index, factor)
            if obj:
                static_meshes.append(obj)
                global_index += 1

        elif comp_type == "InstancedStaticMeshComponent":
            instances = comp.get("PerInstanceSMData", [])
            for i in range(len(instances)):
                obj = parse_instanced(comp, i, global_index, factor)
                if obj:
                    static_meshes.append(obj)
                    global_index += 1

    main_count = len(static_meshes)

    # ── _Generated_ 폴더 처리 ──
    origin_dir = os.path.dirname(origin_path)
    generated_dir = os.path.join(origin_dir, map_name, "_Generated_")

    if os.path.isdir(generated_dir):
        gen_files = sorted(glob.glob(os.path.join(generated_dir, "*.json")))
        for gen_file in gen_files:
            with open(gen_file, "r", encoding="utf-8") as f:
                gen_root = json.load(f)
            if not isinstance(gen_root, list):
                continue
            for comp in gen_root:
                if comp.get("Type") != "StaticMeshComponent":
                    continue
                outer = comp.get("Outer", {})
                outer_name = outer.get("ObjectName", "") if isinstance(outer, dict) else ""
                if "StaticMeshActor'" not in outer_name:
                    continue
                obj = parse_single(comp, global_index, factor)
                if obj:
                    static_meshes.append(obj)
                    global_index += 1
        print(f"  Generated 파일: {len(gen_files)}개 처리 (+{len(static_meshes) - main_count}개 메시)")
    else:
        print("  Generated 폴더 없음, 메인 JSON만 처리")

    output = {
        "MapData": {
            "MapName": map_name,
            "ModelFolder": model_folder,
            "StaticMeshes": static_meshes,
            "SpawnPoints": [],
            "EncounterZones": [],
            "TriggerVolumes": [],
            "CameraZones": [],
            "PostProcessVolumes": [],
            "Lights": [],
        }
    }

    os.makedirs(os.path.dirname(save_path), exist_ok=True)
    with open(save_path, "w", encoding="utf-8") as f:
        json.dump(output, f, indent=4, ensure_ascii=False)

    print(f"완료: 총 {len(static_meshes)}개 메시 변환 (메인 {main_count}개 + Generated {len(static_meshes) - main_count}개)")
    print(f"  입력: {origin_path}")
    print(f"  출력: {save_path}")
    print(f"  스케일: x{factor}")
    print(f"  모델 폴더: {model_folder}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("filename", help="OriginJson 폴더 안의 파일명 (예: SmallLevel_SimonArea.json)")
    parser.add_argument("--factor", type=float, default=1.0, help="맵 전체 배율 (기본값: 1.0 = 실제 비율)")
    parser.add_argument("--folder", type=str, default="", help="모델 폴더 이름 (기본값: JSON 파일 이름과 동일)")
    args = parser.parse_args()

    script_dir   = os.path.dirname(os.path.abspath(__file__))
    origin_path  = os.path.join(script_dir, ORIGIN_DIR, args.filename)
    save_path    = os.path.join(script_dir, SAVE_DIR, args.filename)

    if not os.path.exists(origin_path):
        print(f"오류: 파일을 찾을 수 없습니다 → {origin_path}")
        return

    convert(origin_path, save_path, args.factor, args.folder)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Build a runtime-facing facial clip description from UE/FModel exports.

The first output format is JSON on purpose. It lets us verify curve/bone
matching before committing to an engine binary format.
"""

from __future__ import annotations

import argparse
import json
import math
import struct
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any


PSA_CHUNK_HEADER = struct.Struct("<20siii")
PSA_BONE_RECORD = struct.Struct("<64siii11f")
PSA_ANIMINFO_RECORD = struct.Struct("<64s64siiiifffiii")
PSA_ANIMKEY_RECORD = struct.Struct("<3f4ff")
PSA_SCALEKEY_RECORD = struct.Struct("<3ff")

MORPH_HEADER = struct.Struct("<IIII")
MORPH_MAGIC = 0x50524F4D  # 'MORP'
MORPH_VERSION = 1

ZERO_EPSILON = 1.0e-6

FACIAL_CLIP_MAGIC = 0x50434C46  # 'FLCP'
FACIAL_CLIP_VERSION = 1
FACIAL_CLIP_FLAG_ADDITIVE_LOCAL = 1 << 0


@dataclass
class PsaChunk:
    name: str
    data_size: int
    data_count: int
    offset: int


@dataclass
class PsaAnimInfo:
    name: str
    group: str
    total_bones: int
    key_count: int
    track_time: float
    anim_rate: float
    first_raw_frame: int
    num_raw_frames: int


def decode_c_string(raw: bytes) -> str:
    return raw.split(b"\0", 1)[0].decode("utf-8", "ignore")


def read_u32_string(data: bytes, offset: int) -> tuple[str, int]:
    if offset + 4 > len(data):
        raise ValueError("Unexpected end of morph string length")

    length = struct.unpack_from("<I", data, offset)[0]
    offset += 4
    end = offset + length
    if end > len(data):
        raise ValueError("Unexpected end of morph string body")

    return data[offset:end].decode("utf-8", "ignore"), end


def load_morph_targets(path: Path | None) -> set[str]:
    if path is None:
        return set()

    data = path.read_bytes()
    if len(data) < MORPH_HEADER.size:
        raise ValueError(f"Invalid morph file: {path}")

    magic, version, target_count, _mesh_count = MORPH_HEADER.unpack_from(data, 0)
    if magic != MORPH_MAGIC or version != MORPH_VERSION:
        raise ValueError(f"Unsupported morph header: {path}")

    offset = MORPH_HEADER.size
    _source_name, offset = read_u32_string(data, offset)

    targets: set[str] = set()
    for _ in range(target_count):
        name, offset = read_u32_string(data, offset)
        if name:
            targets.add(name)

    return targets


def load_model_bones(path: Path | None) -> dict[str, int]:
    if path is None:
        return {}

    root = json.loads(path.read_text(encoding="utf-8"))
    bones = root.get("bones", [])
    result: dict[str, int] = {}
    for bone in bones:
        name = bone.get("name")
        index = bone.get("index")
        if isinstance(name, str) and isinstance(index, int):
            result[name] = index

    return result


def load_anim_sequence(path: Path) -> dict[str, Any]:
    root = json.loads(path.read_text(encoding="utf-8"))
    if isinstance(root, list):
        for entry in root:
            if isinstance(entry, dict) and entry.get("Type") == "AnimSequence":
                return entry
        if root and isinstance(root[0], dict):
            return root[0]

    if isinstance(root, dict):
        return root

    raise ValueError(f"Unsupported AnimSequence JSON: {path}")


def read_curve_keys(curve_node: dict[str, Any]) -> list[dict[str, float]]:
    keys = curve_node.get("FloatCurve", {}).get("Keys", [])
    out: list[dict[str, float]] = []
    if not isinstance(keys, list):
        return out

    for key in keys:
        if not isinstance(key, dict):
            continue

        time = key.get("Time")
        value = key.get("Value")
        if isinstance(time, (int, float)) and isinstance(value, (int, float)):
            out.append({"time": float(time), "value": float(value)})

    return out


def read_curve_tracks(anim_node: dict[str, Any], morph_targets: set[str], epsilon: float) -> list[dict[str, Any]]:
    curve_data = anim_node.get("CompressedCurveData", {})
    float_curves = curve_data.get("FloatCurves", [])
    tracks: list[dict[str, Any]] = []

    if not isinstance(float_curves, list):
        return tracks

    for curve_node in float_curves:
        if not isinstance(curve_node, dict):
            continue

        name = curve_node.get("CurveName")
        if not isinstance(name, str) or not name:
            continue

        keys = read_curve_keys(curve_node)
        if not keys:
            continue

        max_abs = max(abs(key["value"]) for key in keys)
        if max_abs <= epsilon:
            continue

        tracks.append(
            {
                "name": name,
                "binding": "MorphTarget" if name in morph_targets else "UnboundCurve",
                "maxAbsValue": max_abs,
                "keys": keys,
            }
        )

    return tracks


def read_psa_chunks(path: Path) -> tuple[bytes, dict[str, PsaChunk]]:
    data = path.read_bytes()
    offset = 0
    chunks: dict[str, PsaChunk] = {}

    while offset + PSA_CHUNK_HEADER.size <= len(data):
        chunk_name_raw, _type_flag, data_size, data_count = PSA_CHUNK_HEADER.unpack_from(data, offset)
        name = decode_c_string(chunk_name_raw)
        offset += PSA_CHUNK_HEADER.size

        if not name:
            break

        data_bytes = data_size * data_count
        if data_bytes < 0 or offset + data_bytes > len(data):
            raise ValueError(f"Invalid PSA chunk size for {name}")

        chunks[name] = PsaChunk(name=name, data_size=data_size, data_count=data_count, offset=offset)
        offset += data_bytes

    return data, chunks


def read_psa_bones(data: bytes, chunk: PsaChunk) -> list[dict[str, Any]]:
    if chunk.data_size != PSA_BONE_RECORD.size:
        raise ValueError(f"Unsupported BONENAMES record size: {chunk.data_size}")

    bones: list[dict[str, Any]] = []
    for index in range(chunk.data_count):
        offset = chunk.offset + index * chunk.data_size
        name_raw, flags, num_children, parent_index, *pose_values = PSA_BONE_RECORD.unpack_from(data, offset)
        bones.append(
            {
                "index": index,
                "name": decode_c_string(name_raw),
                "flags": flags,
                "numChildren": num_children,
                "parentIndex": parent_index,
                "referencePoseRaw": list(pose_values),
            }
        )

    return bones


def read_psa_anim_info(data: bytes, chunk: PsaChunk) -> PsaAnimInfo:
    if chunk.data_count < 1:
        raise ValueError("PSA has no ANIMINFO records")

    if chunk.data_size != PSA_ANIMINFO_RECORD.size:
        raise ValueError(f"Unsupported ANIMINFO record size: {chunk.data_size}")

    values = PSA_ANIMINFO_RECORD.unpack_from(data, chunk.offset)
    name = decode_c_string(values[0])
    group = decode_c_string(values[1])
    total_bones = int(values[2])
    key_count = int(values[5])
    track_time = float(values[7])
    anim_rate = float(values[8])
    first_raw_frame = int(values[10])
    num_raw_frames = int(values[11])

    return PsaAnimInfo(
        name=name,
        group=group,
        total_bones=total_bones,
        key_count=key_count,
        track_time=track_time,
        anim_rate=anim_rate,
        first_raw_frame=first_raw_frame,
        num_raw_frames=num_raw_frames,
    )


def should_keep_bone(name: str, prefixes: list[str], include_all: bool) -> bool:
    if include_all:
        return True

    return any(name.startswith(prefix) for prefix in prefixes)


def nearly_same_key(a: dict[str, Any], b: dict[str, Any], epsilon: float) -> bool:
    for field in ("position", "rotation", "scale"):
        av = a[field]
        bv = b[field]
        if len(av) != len(bv):
            return False
        for x, y in zip(av, bv):
            if abs(float(x) - float(y)) > epsilon:
                return False

    return True


def prune_keys(keys: list[dict[str, Any]], epsilon: float) -> list[dict[str, Any]]:
    if len(keys) <= 2:
        return keys

    pruned = [keys[0]]
    for key in keys[1:-1]:
        if not nearly_same_key(pruned[-1], key, epsilon):
            pruned.append(key)

    if not nearly_same_key(pruned[-1], keys[-1], epsilon):
        pruned.append(keys[-1])
    elif len(pruned) == 1:
        pruned.append(keys[-1])

    return pruned


def read_psa_bone_tracks(
    path: Path,
    model_bones: dict[str, int],
    prefixes: list[str],
    include_all_bones: bool,
    prune_bone_keys: bool,
    key_epsilon: float,
) -> tuple[list[dict[str, Any]], dict[str, Any]]:
    data, chunks = read_psa_chunks(path)
    for required in ("BONENAMES", "ANIMINFO", "ANIMKEYS"):
        if required not in chunks:
            raise ValueError(f"PSA missing required chunk: {required}")

    bones = read_psa_bones(data, chunks["BONENAMES"])
    anim_info = read_psa_anim_info(data, chunks["ANIMINFO"])

    key_chunk = chunks["ANIMKEYS"]
    if key_chunk.data_size != PSA_ANIMKEY_RECORD.size:
        raise ValueError(f"Unsupported ANIMKEYS record size: {key_chunk.data_size}")

    scale_chunk = chunks.get("SCALEKEYS")
    has_scale = scale_chunk is not None and scale_chunk.data_size == PSA_SCALEKEY_RECORD.size

    total_bones = anim_info.total_bones
    raw_frames = anim_info.num_raw_frames
    expected_keys = total_bones * raw_frames
    if key_chunk.data_count < expected_keys:
        raise ValueError(
            f"PSA key count is smaller than expected: {key_chunk.data_count} < {expected_keys}"
        )

    frame_rate = anim_info.anim_rate if anim_info.anim_rate > ZERO_EPSILON else 30.0
    duration = raw_frames / frame_rate if frame_rate > ZERO_EPSILON else 0.0

    selected_bones = [
        bone
        for bone in bones[:total_bones]
        if should_keep_bone(str(bone["name"]), prefixes, include_all_bones)
    ]

    tracks: list[dict[str, Any]] = []
    for bone in selected_bones:
        bone_index = int(bone["index"])
        bone_name = str(bone["name"])
        keys: list[dict[str, Any]] = []

        for frame in range(raw_frames):
            key_index = frame * total_bones + bone_index
            key_offset = key_chunk.offset + key_index * key_chunk.data_size
            px, py, pz, qx, qy, qz, qw, _key_time = PSA_ANIMKEY_RECORD.unpack_from(data, key_offset)

            sx, sy, sz = 1.0, 1.0, 1.0
            if has_scale and scale_chunk is not None and scale_chunk.data_count > key_index:
                scale_offset = scale_chunk.offset + key_index * scale_chunk.data_size
                sx, sy, sz, _scale_time = PSA_SCALEKEY_RECORD.unpack_from(data, scale_offset)

            keys.append(
                {
                    "time": frame / frame_rate,
                    "position": [px, py, pz],
                    "rotation": [qx, qy, qz, qw],
                    "scale": [sx, sy, sz],
                }
            )

        if prune_bone_keys:
            keys = prune_keys(keys, key_epsilon)

        if len(keys) <= 2 and nearly_same_key(keys[0], keys[-1], key_epsilon):
            continue

        tracks.append(
            {
                "name": bone_name,
                "psaBoneIndex": bone_index,
                "modelBoneIndex": model_bones.get(bone_name, -1),
                "binding": "ModelBone" if bone_name in model_bones else "UnboundBone",
                "keys": keys,
            }
        )

    metadata = {
        "animName": anim_info.name,
        "animGroup": anim_info.group,
        "totalPsaBones": total_bones,
        "rawFrames": raw_frames,
        "frameRate": frame_rate,
        "durationSec": duration,
        "animKeyCount": key_chunk.data_count,
        "scaleKeyCount": scale_chunk.data_count if scale_chunk else 0,
    }
    return tracks, metadata


def default_output_path(json_path: Path) -> Path:
    return json_path.with_suffix(".facialclip.json")


def make_report(clip: dict[str, Any]) -> dict[str, Any]:
    morph_tracks = clip["morphCurveTracks"]
    bone_tracks = clip["boneTracks"]

    matched_morph = [track for track in morph_tracks if track["binding"] == "MorphTarget"]
    matched_bones = [track for track in bone_tracks if track["binding"] == "ModelBone"]

    return {
        "source": clip["source"],
        "durationSec": clip["durationSec"],
        "morphCurveTrackCount": len(morph_tracks),
        "matchedMorphCurveTrackCount": len(matched_morph),
        "unboundCurveTrackCount": len(morph_tracks) - len(matched_morph),
        "boneTrackCount": len(bone_tracks),
        "matchedBoneTrackCount": len(matched_bones),
        "unboundBoneTrackCount": len(bone_tracks) - len(matched_bones),
        "sampleMorphCurves": [track["name"] for track in matched_morph[:20]],
        "sampleBoneTracks": [track["name"] for track in matched_bones[:20]],
        "unboundBoneTracks": [track["name"] for track in bone_tracks if track["binding"] != "ModelBone"][:80],
    }


def write_binary_string(stream: Any, value: str) -> None:
    encoded = value.encode("utf-8")
    stream.write(struct.pack("<I", len(encoded)))
    stream.write(encoded)


def write_facialclip_binary(clip: dict[str, Any], output_path: Path) -> None:
    morph_tracks = [
        track for track in clip["morphCurveTracks"]
        if track["binding"] == "MorphTarget"
    ]
    bone_tracks = [
        track for track in clip["boneTracks"]
        if track["binding"] == "ModelBone"
    ]

    flags = 0
    if clip.get("additiveLocal", False):
        flags |= FACIAL_CLIP_FLAG_ADDITIVE_LOCAL

    with output_path.open("wb") as stream:
        stream.write(struct.pack(
            "<III f II",
            FACIAL_CLIP_MAGIC,
            FACIAL_CLIP_VERSION,
            flags,
            float(clip["durationSec"]),
            len(morph_tracks),
            len(bone_tracks),
        ))
        write_binary_string(stream, str(clip["source"]["name"]))

        for track in morph_tracks:
            write_binary_string(stream, str(track["name"]))
            keys = track["keys"]
            stream.write(struct.pack("<I", len(keys)))
            for key in keys:
                stream.write(struct.pack("<ff", float(key["time"]), float(key["value"])))

        for track in bone_tracks:
            write_binary_string(stream, str(track["name"]))
            keys = track["keys"]
            stream.write(struct.pack("<I", len(keys)))
            for key in keys:
                stream.write(struct.pack(
                    "<f 3f 4f 3f",
                    float(key["time"]),
                    float(key["position"][0]),
                    float(key["position"][1]),
                    float(key["position"][2]),
                    float(key["rotation"][0]),
                    float(key["rotation"][1]),
                    float(key["rotation"][2]),
                    float(key["rotation"][3]),
                    float(key["scale"][0]),
                    float(key["scale"][1]),
                    float(key["scale"][2]),
                ))


def build_facial_clip(args: argparse.Namespace) -> tuple[dict[str, Any], dict[str, Any]]:
    anim_node = load_anim_sequence(args.json)
    props = anim_node.get("Properties", {})
    json_duration = props.get("SequenceLength", anim_node.get("SequenceLength", 0.0))
    source_name = anim_node.get("Name", args.json.stem)
    additive_type = props.get("AdditiveAnimType", anim_node.get("AdditiveAnimType", ""))

    morph_targets = load_morph_targets(args.morph)
    model_bones = load_model_bones(args.model_meta)

    curve_tracks = read_curve_tracks(anim_node, morph_targets, args.curve_epsilon)
    bone_tracks, psa_metadata = read_psa_bone_tracks(
        args.psa,
        model_bones,
        args.bone_prefix,
        args.all_bones,
        not args.no_prune_bone_keys,
        args.key_epsilon,
    )

    duration = float(json_duration or 0.0)
    if duration <= ZERO_EPSILON:
        duration = float(psa_metadata["durationSec"])

    clip = {
        "format": "ExpeditionFacialClip",
        "version": 1,
        "source": {
            "name": source_name,
            "json": str(args.json),
            "psa": str(args.psa),
            "modelMeta": str(args.model_meta) if args.model_meta else "",
            "morph": str(args.morph) if args.morph else "",
        },
        "durationSec": duration,
        "additiveLocal": str(additive_type).endswith("AAT_LocalSpaceBase"),
        "psa": psa_metadata,
        "coordinateSpace": "actorx_psa_raw",
        "morphCurveTracks": curve_tracks,
        "boneTracks": bone_tracks,
    }

    return clip, make_report(clip)


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Build facialclip JSON from AnimSequence JSON + PSA.")
    parser.add_argument("--json", type=Path, required=True, help="FModel AnimSequence JSON path.")
    parser.add_argument("--psa", type=Path, required=True, help="ActorX PSA animation path.")
    parser.add_argument("--model-meta", type=Path, default=None, help="Optional .model.meta for model bone matching.")
    parser.add_argument("--morph", type=Path, default=None, help="Optional .morph for morph target matching.")
    parser.add_argument("--out", type=Path, default=None, help="Output .facialclip.json path.")
    parser.add_argument("--bin-out", type=Path, default=None, help="Output .facialclipbin path.")
    parser.add_argument("--report", type=Path, default=None, help="Output report JSON path.")
    parser.add_argument("--bone-prefix", action="append", default=["FACIAL_"], help="Bone prefix to extract.")
    parser.add_argument("--all-bones", action="store_true", help="Extract all PSA bones instead of prefix filtering.")
    parser.add_argument("--no-prune-bone-keys", action="store_true", help="Keep every PSA bone frame key.")
    parser.add_argument("--curve-epsilon", type=float, default=ZERO_EPSILON, help="Zero threshold for curve tracks.")
    parser.add_argument("--key-epsilon", type=float, default=1.0e-5, help="Duplicate threshold for bone key pruning.")
    parser.add_argument("--compact", action="store_true", help="Write compact JSON.")

    args = parser.parse_args(argv)

    for path_arg in ("json", "psa", "model_meta", "morph"):
        path = getattr(args, path_arg)
        if path is not None and not path.exists():
            parser.error(f"{path_arg.replace('_', '-')} path does not exist: {path}")

    if args.out is None:
        args.out = default_output_path(args.json)

    if args.bin_out is None:
        args.bin_out = args.out.with_suffix(".facialclipbin")

    if args.report is None:
        args.report = args.out.with_suffix(".report.json")

    return args


def main(argv: list[str]) -> int:
    args = parse_args(argv)

    clip, report = build_facial_clip(args)

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.bin_out.parent.mkdir(parents=True, exist_ok=True)
    args.report.parent.mkdir(parents=True, exist_ok=True)

    if args.compact:
        args.out.write_text(json.dumps(clip, ensure_ascii=False, separators=(",", ":")), encoding="utf-8")
    else:
        args.out.write_text(json.dumps(clip, ensure_ascii=False, indent=2), encoding="utf-8")

    write_facialclip_binary(clip, args.bin_out)
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")

    print(f"[FacialClip] wrote: {args.out}")
    print(f"[FacialClip] binary: {args.bin_out}")
    print(f"[FacialClip] report: {args.report}")
    print(f"[FacialClip] duration: {report['durationSec']:.6f}s")
    print(
        "[FacialClip] morph curves: "
        f"{report['matchedMorphCurveTrackCount']} matched / {report['morphCurveTrackCount']} total"
    )
    print(
        "[FacialClip] bone tracks: "
        f"{report['matchedBoneTrackCount']} matched / {report['boneTrackCount']} total"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))

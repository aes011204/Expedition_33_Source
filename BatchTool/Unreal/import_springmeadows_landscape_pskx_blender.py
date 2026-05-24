"""
Blender script for importing and placing SpringMeadows landscape PSKX tiles.

Usage:
1. Install/enable a PSK/PSKX importer add-on in Blender.
   This script tries common operators such as bpy.ops.psk.import_file.
2. Open Blender > Scripting > Text > Open this file > Run Script.
3. If the imported terrain is axis-flipped, change FLIP_Y or SWAP_XY below.
"""

from __future__ import annotations

import json
import re
from pathlib import Path

import bpy


LEVEL_JSON = Path(
    r"D:\Clair Obscur Expedition 33\Exports\Sandfall\Content\Levels\SpringMeadows\Level_SpringMeadows_Main_V2.json"
)
GENERATED_DIR = Path(
    r"D:\Clair Obscur Expedition 33\Exports\Sandfall\Content\Levels\SpringMeadows\Level_SpringMeadows_Main_V2\_Generated_"
)

CLEAR_SCENE = True
COLLECTION_NAME = "SpringMeadows_Landscape_PSKX"

# UE uses centimeters. Blender preview uses meters.
UE_CM_TO_M = 0.01

# If the PSK importer comes in mirrored, tweak only these first.
FLIP_Y = False
SWAP_XY = False

# Set this to a real path if you want the placed 16-tile scene exported at once.
EXPORT_SCENE_FBX_PATH = None

TILE_SIZE_QUADS = 126
LANDSCAPE_PROXY_RE = re.compile(r"LandscapeStreamingProxy_[^']+_1_(-?\d+)_(-?\d+)_0$")


def ue_vec_to_blender(v: dict[str, float]) -> tuple[float, float, float]:
    x = float(v.get("X", 0.0)) * UE_CM_TO_M
    y = float(v.get("Y", 0.0)) * UE_CM_TO_M
    z = float(v.get("Z", 0.0)) * UE_CM_TO_M

    if SWAP_XY:
        x, y = y, x
    if FLIP_Y:
        y = -y

    return x, y, z


def find_landscape_root(data: list[dict]) -> tuple[dict[str, float], dict[str, float]]:
    for obj in data:
        if obj.get("Type") != "SceneComponent" or obj.get("Name") != "RootComponent0":
            continue

        outer = obj.get("Outer", {})
        if "Landscape_Temp" not in outer.get("ObjectName", ""):
            continue

        props = obj.get("Properties", {})
        return props["RelativeLocation"], props["RelativeScale3D"]

    raise RuntimeError("Could not find Landscape_Temp.RootComponent0 in level json.")


def find_landscape_proxy_tiles(data: list[dict]) -> list[dict]:
    tiles = []

    for obj in data:
        props = obj.get("Properties")
        if not isinstance(props, dict):
            continue

        remapping = props.get("SubObjectsToCellRemapping")
        if not isinstance(remapping, list):
            continue

        for item in remapping:
            key = item.get("Key", "")
            package_hash = item.get("Value", "")
            match = LANDSCAPE_PROXY_RE.match(key)
            if not match:
                continue

            pskx_path = GENERATED_DIR / package_hash / "LandscapeNaniteMesh_0.pskx"
            if not pskx_path.exists():
                continue

            tiles.append(
                {
                    "grid_x": int(match.group(1)),
                    "grid_y": int(match.group(2)),
                    "hash": package_hash,
                    "pskx": pskx_path,
                }
            )

    if not tiles:
        raise RuntimeError("No LandscapeStreamingProxy tiles with LandscapeNaniteMesh_0.pskx were found.")

    tiles.sort(key=lambda tile: (tile["grid_y"], tile["grid_x"], tile["hash"]))
    return tiles


def clear_scene() -> None:
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete()


def get_or_create_collection(name: str) -> bpy.types.Collection:
    existing = bpy.data.collections.get(name)
    if existing:
        return existing

    collection = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(collection)
    return collection


def try_import_operator(operator_path: str, filepath: str):
    owner = bpy.ops
    for part in operator_path.split("."):
        owner = getattr(owner, part)

    return owner(filepath=filepath)


def import_pskx(path: Path) -> list[bpy.types.Object]:
    before = set(bpy.context.scene.objects)

    errors = []
    for operator_path in (
        "psk.import_file",
        "import_scene.psk",
        "import_mesh.psk",
        "import_scene.pskx",
        "import_mesh.pskx",
    ):
        try:
            try_import_operator(operator_path, str(path))
            break
        except Exception as exc:
            errors.append(f"{operator_path}: {exc}")
    else:
        raise RuntimeError(
            "No working PSK/PSKX import operator found. Enable a Blender PSK/PSKX add-on first.\n"
            "Tried:\n- " + "\n- ".join(errors)
        )

    after = set(bpy.context.scene.objects)
    imported = list(after - before)
    if not imported:
        raise RuntimeError(f"Importer finished but created no object: {path}")

    return imported


def move_objects_to_collection(objects: list[bpy.types.Object], collection: bpy.types.Collection) -> None:
    for obj in objects:
        if obj.name not in collection.objects:
            collection.objects.link(obj)

        for old_collection in list(obj.users_collection):
            if old_collection != collection:
                old_collection.objects.unlink(obj)


def place_tile_objects(
    objects: list[bpy.types.Object],
    collection: bpy.types.Collection,
    tile: dict,
    root_location: dict[str, float],
    landscape_scale: dict[str, float],
) -> None:
    scale_x = float(landscape_scale["X"]) * UE_CM_TO_M
    scale_y = float(landscape_scale["Y"]) * UE_CM_TO_M
    scale_z = float(landscape_scale["Z"]) * UE_CM_TO_M

    location_ue = {
        "X": float(root_location["X"]) + tile["grid_x"] * TILE_SIZE_QUADS * float(landscape_scale["X"]),
        "Y": float(root_location["Y"]) + tile["grid_y"] * TILE_SIZE_QUADS * float(landscape_scale["Y"]),
        "Z": float(root_location["Z"]),
    }

    location = ue_vec_to_blender(location_ue)
    if SWAP_XY:
        scale_x, scale_y = scale_y, scale_x

    for obj in objects:
        obj.name = f"SM_Landscape_{tile['grid_x']}_{tile['grid_y']}_{tile['hash']}"
        obj.location = location
        obj.scale = (scale_x, scale_y, scale_z)
        obj.rotation_euler = (0.0, 0.0, 0.0)
        obj["source_pskx"] = str(tile["pskx"])
        obj["landscape_grid"] = f"{tile['grid_x']},{tile['grid_y']}"

    move_objects_to_collection(objects, collection)


def add_preview_camera(tiles: list[dict], root_location: dict[str, float], landscape_scale: dict[str, float]) -> None:
    xs = [
        float(root_location["X"]) + tile["grid_x"] * TILE_SIZE_QUADS * float(landscape_scale["X"])
        for tile in tiles
    ]
    ys = [
        float(root_location["Y"]) + tile["grid_y"] * TILE_SIZE_QUADS * float(landscape_scale["Y"])
        for tile in tiles
    ]
    center = ue_vec_to_blender(
        {
            "X": (min(xs) + max(xs)) * 0.5,
            "Y": (min(ys) + max(ys)) * 0.5,
            "Z": float(root_location["Z"]),
        }
    )

    bpy.ops.object.light_add(type="SUN", location=(center[0], center[1], center[2] + 500.0))
    bpy.context.object.name = "SpringMeadows_Preview_Sun"

    bpy.ops.object.camera_add(location=(center[0], center[1] - 650.0, center[2] + 420.0), rotation=(1.05, 0.0, 0.0))
    bpy.context.scene.camera = bpy.context.object


def export_scene_fbx() -> None:
    if not EXPORT_SCENE_FBX_PATH:
        return

    export_path = Path(EXPORT_SCENE_FBX_PATH)
    export_path.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.fbx(filepath=str(export_path), use_selection=False, apply_unit_scale=True)
    print(f"Exported FBX: {export_path}")


def main() -> None:
    if CLEAR_SCENE:
        clear_scene()

    data = json.loads(LEVEL_JSON.read_text(encoding="utf-8"))
    root_location, landscape_scale = find_landscape_root(data)
    tiles = find_landscape_proxy_tiles(data)
    collection = get_or_create_collection(COLLECTION_NAME)

    print(f"Importing SpringMeadows landscape tiles: {len(tiles)}")
    for tile in tiles:
        objects = import_pskx(tile["pskx"])
        place_tile_objects(objects, collection, tile, root_location, landscape_scale)
        print(f"Placed ({tile['grid_x']}, {tile['grid_y']}) -> {tile['hash']}")

    add_preview_camera(tiles, root_location, landscape_scale)
    export_scene_fbx()
    print("SpringMeadows landscape PSKX import complete.")


if __name__ == "__main__":
    main()

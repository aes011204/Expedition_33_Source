r"""
Blender preview importer for SpringMeadows.

What it does:
- Imports SpringMeadows landscape PSKX tiles from `_Generated_`.
- Reads StaticMeshActor transforms from Level_SpringMeadows_Main_V2.json.
- Imports exported mesh files when they exist under the export root.
- Creates Empty placeholders for missing meshes so overall placement can still be previewed.

Expected export layout:
    C:\Users\moon\Desktop\Output\Exports\Sandfall\Content\...

Expected Blender add-ons:
- Unreal PSK (.psk/.pskx) for PSK/PSKX imports
- FBX/OBJ/GLTF importers are optional and used only if those files exist
"""

from __future__ import annotations

import json
import math
import re
from pathlib import Path

import bpy


EXPORT_ROOT = Path(r"C:\Users\moon\Desktop\Output\Exports\Sandfall\Content")
LEVEL_JSON = EXPORT_ROOT / "Levels" / "SpringMeadows" / "Level_SpringMeadows_Main_V2.json"
GENERATED_DIR = EXPORT_ROOT / "Levels" / "SpringMeadows" / "Level_SpringMeadows_Main_V2" / "_Generated_"

CLEAR_SCENE = True
IMPORT_LANDSCAPE = True
IMPORT_STATIC_MESHES = True
CREATE_PLACEHOLDERS_FOR_MISSING = True
LIMIT_STATIC_MESH_ACTORS = None

LANDSCAPE_COLLECTION = "SpringMeadows_Landscape"
STATIC_COLLECTION = "SpringMeadows_StaticMeshes"
MISSING_COLLECTION = "SpringMeadows_MissingMeshes"

UE_CM_TO_M = 0.01
TILE_SIZE_QUADS = 124

# Blender preview uses UE axis order by default: X/Y horizontal, Z up.
SWAP_XY = False
FLIP_Y = False

LANDSCAPE_PROXY_RE = re.compile(r"LandscapeStreamingProxy_[^']+_1_(-?\d+)_(-?\d+)_0$")
ACTOR_NAME_PATTERN = re.compile(r"\.([^\.']+)'$")


def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


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


def move_object_to_collection(obj: bpy.types.Object, collection: bpy.types.Collection) -> None:
    if obj.name not in collection.objects:
        collection.objects.link(obj)

    for old_collection in list(obj.users_collection):
        if old_collection != collection:
            old_collection.objects.unlink(obj)


def move_objects_to_collection(objects: list[bpy.types.Object], collection: bpy.types.Collection) -> None:
    for obj in objects:
        move_object_to_collection(obj, collection)


def ue_vec_to_blender(x: float, y: float, z: float) -> tuple[float, float, float]:
    x *= UE_CM_TO_M
    y *= UE_CM_TO_M
    z *= UE_CM_TO_M

    if SWAP_XY:
        x, y = y, x
    if FLIP_Y:
        y = -y

    return x, y, z


def read_ue_xyz(value: dict | None, default: tuple[float, float, float]) -> tuple[float, float, float]:
    if not isinstance(value, dict):
        return default
    return (
        float(value.get("X", default[0])),
        float(value.get("Y", default[1])),
        float(value.get("Z", default[2])),
    )


def read_ue_rot(value: dict | None) -> tuple[float, float, float]:
    if not isinstance(value, dict):
        return 0.0, 0.0, 0.0
    return (
        float(value.get("Pitch", 0.0)),
        float(value.get("Yaw", 0.0)),
        float(value.get("Roll", 0.0)),
    )


def read_ue_scale(value: dict | None) -> tuple[float, float, float]:
    if not isinstance(value, dict):
        return 1.0, 1.0, 1.0
    return (
        1.0 if float(value.get("X", 1.0)) == 0.0 else float(value.get("X", 1.0)),
        1.0 if float(value.get("Y", 1.0)) == 0.0 else float(value.get("Y", 1.0)),
        1.0 if float(value.get("Z", 1.0)) == 0.0 else float(value.get("Z", 1.0)),
    )


def ue_rot_to_blender_euler_degrees(pitch: float, yaw: float, roll: float) -> tuple[float, float, float]:
    # This follows the same preview-space idea we used elsewhere: use UE order,
    # then keep only a simple axis reorder for Blender's XYZ euler input.
    if SWAP_XY:
        return pitch, roll, yaw
    return pitch, roll, yaw


def degrees_to_radians(values: tuple[float, float, float]) -> tuple[float, float, float]:
    return tuple(math.radians(v) for v in values)


def get_short_name_from_object_name(object_name: str) -> str | None:
    if not object_name:
        return None
    match = ACTOR_NAME_PATTERN.search(object_name)
    if match:
        return match.group(1)
    return None


def get_mesh_basename_from_object_path(object_path: str) -> str | None:
    if not object_path:
        return None
    normalized = object_path.replace("\\", "/")
    leaf = normalized.split("/")[-1]
    if "." in leaf:
        return leaf.split(".")[0]
    return Path(leaf).stem


def try_import_operator(operator_path: str, filepath: str):
    owner = bpy.ops
    for part in operator_path.split("."):
        owner = getattr(owner, part)
    return owner(filepath=filepath)


def import_file(path: Path) -> list[bpy.types.Object]:
    before = set(bpy.context.scene.objects)
    suffix = path.suffix.lower()

    errors = []
    operator_candidates = {
        ".pskx": ("psk.import_file", "import_scene.psk", "import_mesh.pskx", "import_mesh.psk"),
        ".psk": ("psk.import_file", "import_scene.psk", "import_mesh.psk"),
        ".fbx": ("import_scene.fbx",),
        ".obj": ("wm.obj_import", "import_scene.obj"),
        ".glb": ("import_scene.gltf",),
        ".gltf": ("import_scene.gltf",),
    }

    candidates = operator_candidates.get(suffix)
    if not candidates:
        raise RuntimeError(f"Unsupported import suffix: {suffix}")

    for operator_path in candidates:
        try:
            try_import_operator(operator_path, str(path))
            break
        except Exception as exc:
            errors.append(f"{operator_path}: {exc}")
    else:
        raise RuntimeError(f"Import failed for {path}\n- " + "\n- ".join(errors))

    after = set(bpy.context.scene.objects)
    imported = list(after - before)
    if not imported:
        raise RuntimeError(f"Importer finished but created no object: {path}")
    return imported


def build_mesh_index(root: Path) -> dict[str, Path]:
    mesh_index: dict[str, Path] = {}
    supported_exts = {".pskx", ".psk", ".fbx", ".obj", ".glb", ".gltf"}

    for path in root.rglob("*"):
        if not path.is_file():
            continue
        if path.suffix.lower() not in supported_exts:
            continue

        stem_lower = path.stem.lower()
        mesh_index.setdefault(stem_lower, path)

    return mesh_index


def build_component_index(entries: list[dict]) -> dict[str, dict]:
    component_by_actor_name: dict[str, dict] = {}

    for entry in entries:
        if entry.get("Type") != "StaticMeshComponent":
            continue

        outer = entry.get("Outer", {})
        owner_actor_name = get_short_name_from_object_name(outer.get("ObjectName", ""))
        if owner_actor_name and owner_actor_name not in component_by_actor_name:
            component_by_actor_name[owner_actor_name] = entry

    return component_by_actor_name


def find_landscape_root(entries: list[dict]) -> tuple[dict[str, float], dict[str, float]]:
    for obj in entries:
        if obj.get("Type") != "SceneComponent" or obj.get("Name") != "RootComponent0":
            continue

        outer = obj.get("Outer", {})
        if "Landscape_Temp" not in outer.get("ObjectName", ""):
            continue

        props = obj.get("Properties", {})
        return props["RelativeLocation"], props["RelativeScale3D"]

    raise RuntimeError("Could not find SpringMeadows landscape root component.")


def find_landscape_proxy_tiles(entries: list[dict]) -> list[dict]:
    tiles: list[dict] = []

    for obj in entries:
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

    tiles.sort(key=lambda tile: (tile["grid_y"], tile["grid_x"], tile["hash"]))
    return tiles


def import_landscape(entries: list[dict]) -> None:
    root_location, landscape_scale = find_landscape_root(entries)
    tiles = find_landscape_proxy_tiles(entries)
    collection = get_or_create_collection(LANDSCAPE_COLLECTION)

    for tile in tiles:
        objects = import_file(tile["pskx"])

        location_ue = {
            "X": float(root_location["X"]) + tile["grid_x"] * TILE_SIZE_QUADS * float(landscape_scale["X"]),
            "Y": float(root_location["Y"]) + tile["grid_y"] * TILE_SIZE_QUADS * float(landscape_scale["Y"]),
            "Z": float(root_location["Z"]),
        }

        location = ue_vec_to_blender(location_ue["X"], location_ue["Y"], location_ue["Z"])
        scale = (
            float(landscape_scale["X"]) * UE_CM_TO_M,
            float(landscape_scale["Y"]) * UE_CM_TO_M,
            float(landscape_scale["Z"]) * UE_CM_TO_M,
        )
        if SWAP_XY:
            scale = (scale[1], scale[0], scale[2])

        for obj in objects:
            obj.name = f"SM_Landscape_{tile['grid_x']}_{tile['grid_y']}_{tile['hash']}"
            obj.location = location
            obj.scale = scale
            obj.rotation_euler = (0.0, 0.0, 0.0)
            obj["source_pskx"] = str(tile["pskx"])

        move_objects_to_collection(objects, collection)

    print(f"[Landscape] imported tiles: {len(tiles)}")


def create_missing_placeholder(
    actor_name: str,
    mesh_base_name: str,
    location: tuple[float, float, float],
    rotation_rad: tuple[float, float, float],
    scale: tuple[float, float, float],
    collection: bpy.types.Collection,
) -> None:
    bpy.ops.object.empty_add(type="PLAIN_AXES", location=location, rotation=rotation_rad)
    obj = bpy.context.object
    obj.name = f"MISSING_{actor_name}"
    obj.scale = scale
    obj["missing_mesh"] = mesh_base_name
    move_object_to_collection(obj, collection)


def import_static_meshes(entries: list[dict]) -> None:
    mesh_index = build_mesh_index(EXPORT_ROOT)
    component_by_actor_name = build_component_index(entries)
    collection = get_or_create_collection(STATIC_COLLECTION)
    missing_collection = get_or_create_collection(MISSING_COLLECTION)

    imported_count = 0
    missing_count = 0
    actor_count = 0

    for actor in entries:
        if actor.get("Type") != "StaticMeshActor":
            continue

        actor_name = actor.get("Name", "StaticMeshActor")
        component = component_by_actor_name.get(actor_name)
        if component is None:
            continue

        component_props = component.get("Properties", {})
        mesh_ref = component_props.get("StaticMesh", {})
        mesh_base_name = get_mesh_basename_from_object_path(mesh_ref.get("ObjectPath", ""))
        if not mesh_base_name:
            continue

        ue_location = read_ue_xyz(component_props.get("RelativeLocation"), (0.0, 0.0, 0.0))
        ue_rotation = read_ue_rot(component_props.get("RelativeRotation"))
        ue_scale = read_ue_scale(component_props.get("RelativeScale3D"))

        location = ue_vec_to_blender(*ue_location)
        rotation_rad = degrees_to_radians(ue_rot_to_blender_euler_degrees(*ue_rotation))
        scale = ue_scale
        if SWAP_XY:
            scale = (scale[1], scale[0], scale[2])

        mesh_path = mesh_index.get(mesh_base_name.lower())
        if mesh_path:
            try:
                objects = import_file(mesh_path)
                for obj in objects:
                    obj.name = actor_name
                    obj.location = location
                    obj.rotation_euler = rotation_rad
                    obj.scale = scale
                    obj["source_mesh"] = str(mesh_path)
                move_objects_to_collection(objects, collection)
                imported_count += 1
            except Exception as exc:
                print(f"[StaticMesh][ImportError] actor={actor_name} mesh={mesh_base_name} error={exc}")
                if CREATE_PLACEHOLDERS_FOR_MISSING:
                    create_missing_placeholder(actor_name, mesh_base_name, location, rotation_rad, scale, missing_collection)
                    missing_count += 1
        else:
            if CREATE_PLACEHOLDERS_FOR_MISSING:
                create_missing_placeholder(actor_name, mesh_base_name, location, rotation_rad, scale, missing_collection)
                missing_count += 1

        actor_count += 1
        if LIMIT_STATIC_MESH_ACTORS is not None and actor_count >= LIMIT_STATIC_MESH_ACTORS:
            break

    print(f"[StaticMesh] actors scanned: {actor_count}")
    print(f"[StaticMesh] imported meshes: {imported_count}")
    print(f"[StaticMesh] missing placeholders: {missing_count}")


def add_preview_camera() -> None:
    bpy.ops.object.light_add(type="SUN", location=(0.0, 0.0, 500.0))
    bpy.context.object.name = "SpringMeadows_Preview_Sun"

    bpy.ops.object.camera_add(location=(0.0, -700.0, 450.0), rotation=(1.05, 0.0, 0.0))
    bpy.context.scene.camera = bpy.context.object


def main() -> None:
    if CLEAR_SCENE:
        clear_scene()

    entries = load_json(LEVEL_JSON)

    if IMPORT_LANDSCAPE:
        import_landscape(entries)

    if IMPORT_STATIC_MESHES:
        import_static_meshes(entries)

    add_preview_camera()
    print("SpringMeadows Blender preview import complete.")


if __name__ == "__main__":
    main()

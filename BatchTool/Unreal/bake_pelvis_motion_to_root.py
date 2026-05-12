"""
Bake pelvis translation into root translation for selected AnimSequence assets.

Run inside the Unreal Editor Python console:
    exec(open(r"D:/GitDesktop/Expedition_158/BatchTool/Unreal/bake_pelvis_motion_to_root.py", encoding="utf-8").read())

This duplicates each selected animation with SUFFIX, then edits the duplicate.
"""

import unreal


ROOT_BONE_CANDIDATES = ("root", "Root")
PELVIS_BONE_CANDIDATES = ("pelvis", "Pelvis")
SUFFIX = "_RootMotion"

# Keep True for dash/jump style clips where vertical movement must travel with root.
# Set False if you only want ground-plane displacement moved to root.
BAKE_VERTICAL = True


def _log(message):
    unreal.log("[BakePelvisToRoot] {}".format(message))


def _warn(message):
    unreal.log_warning("[BakePelvisToRoot] {}".format(message))


def _asset_base_path(asset):
    path = asset.get_path_name()
    return path.split(".")[0]


def _anim_lib():
    if hasattr(unreal, "AnimationLibrary"):
        return unreal.AnimationLibrary
    if hasattr(unreal, "AnimationBlueprintLibrary"):
        return unreal.AnimationBlueprintLibrary
    raise RuntimeError("AnimationLibrary is not available. Enable the Animation Blueprint Library plugin/module.")


def _anim_controller(anim):
    if not hasattr(anim, "get_controller"):
        raise RuntimeError("{} has no get_controller(); this Unreal version cannot edit bone tracks through this script.".format(anim.get_name()))
    controller = anim.get_controller()
    if not controller:
        raise RuntimeError("{} returned no AnimDataController.".format(anim.get_name()))
    return controller


def _raw_track_names(anim):
    lib = _anim_lib()
    if hasattr(lib, "get_animation_track_names"):
        return [str(name) for name in lib.get_animation_track_names(anim)]
    if hasattr(lib, "get_raw_track_names"):
        return [str(name) for name in lib.get_raw_track_names(anim)]
    raise RuntimeError("AnimationLibrary has no track-name query function.")


def _find_track(anim, candidates):
    names = _raw_track_names(anim)
    lower_to_name = {name.lower(): name for name in names}
    for candidate in candidates:
        found = lower_to_name.get(candidate.lower())
        if found:
            return found
    return None


def _expanded(keys, count):
    keys = list(keys)
    if not keys:
        return []
    if len(keys) == count:
        return keys
    if len(keys) == 1:
        return [keys[0] for _ in range(count)]
    if len(keys) < count:
        return keys + [keys[-1] for _ in range(count - len(keys))]
    return keys[:count]


def _quat_rotate_vector(quat, vec):
    # Avoid relying on version-specific Python bindings for FQuat::RotateVector.
    qx, qy, qz, qw = quat.x, quat.y, quat.z, quat.w
    vx, vy, vz = vec.x, vec.y, vec.z

    # t = 2 * cross(q.xyz, v)
    tx = 2.0 * (qy * vz - qz * vy)
    ty = 2.0 * (qz * vx - qx * vz)
    tz = 2.0 * (qx * vy - qy * vx)

    # v' = v + qw * t + cross(q.xyz, t)
    rx = vx + qw * tx + (qy * tz - qz * ty)
    ry = vy + qw * ty + (qz * tx - qx * tz)
    rz = vz + qw * tz + (qx * ty - qy * tx)
    return unreal.Vector(rx, ry, rz)


def _scaled_vector(vec, scale):
    return unreal.Vector(vec.x * scale.x, vec.y * scale.y, vec.z * scale.z)


def _vector_sub(a, b):
    return unreal.Vector(a.x - b.x, a.y - b.y, a.z - b.z)


def _vector_add(a, b):
    return unreal.Vector(a.x + b.x, a.y + b.y, a.z + b.z)


def _vector_len(vec):
    return (vec.x * vec.x + vec.y * vec.y + vec.z * vec.z) ** 0.5


def _zero_vertical(vec):
    if BAKE_VERTICAL:
        return vec
    return unreal.Vector(vec.x, vec.y, 0.0)


def _get_track(anim, bone_name):
    lib = _anim_lib()
    return {
        "pos": list(lib.get_raw_track_position_data(anim, bone_name)),
        "rot": list(lib.get_raw_track_rotation_data(anim, bone_name)),
        "scale": list(lib.get_raw_track_scale_data(anim, bone_name)),
    }


def _set_track(anim, bone_name, pos, rot, scale):
    controller = _anim_controller(anim)
    return controller.set_bone_track_keys(bone_name, pos, rot, scale, False)


def _duplicate(anim):
    source_path = _asset_base_path(anim)
    target_path = source_path + SUFFIX
    if unreal.EditorAssetLibrary.does_asset_exist(target_path):
        target_path = unreal.AssetToolsHelpers.get_asset_tools().create_unique_asset_name(target_path, "")[0]

    if not unreal.EditorAssetLibrary.duplicate_asset(source_path, target_path):
        raise RuntimeError("duplicate failed: {} -> {}".format(source_path, target_path))

    duplicated = unreal.EditorAssetLibrary.load_asset(target_path)
    if not duplicated:
        raise RuntimeError("failed to load duplicated asset: {}".format(target_path))
    return duplicated


def bake_anim(anim):
    _anim_lib()
    root_name = _find_track(anim, ROOT_BONE_CANDIDATES)
    pelvis_name = _find_track(anim, PELVIS_BONE_CANDIDATES)

    if not root_name or not pelvis_name:
        _warn("{}: root/pelvis track missing. tracks={}".format(anim.get_name(), _raw_track_names(anim)[:20]))
        return None

    baked = _duplicate(anim)
    root = _get_track(baked, root_name)
    pelvis = _get_track(baked, pelvis_name)

    key_count = max(
        len(root["pos"]), len(root["rot"]), len(root["scale"]),
        len(pelvis["pos"]), len(pelvis["rot"]), len(pelvis["scale"]),
    )
    if key_count <= 1:
        _warn("{}: no animated pelvis/root translation keys found".format(anim.get_name()))
        return baked

    root_pos = _expanded(root["pos"], key_count)
    root_rot = _expanded(root["rot"], key_count)
    root_scale = _expanded(root["scale"], key_count)
    pelvis_pos = _expanded(pelvis["pos"], key_count)
    pelvis_rot = _expanded(pelvis["rot"], key_count)
    pelvis_scale = _expanded(pelvis["scale"], key_count)

    pelvis_ref = pelvis_pos[0]
    pelvis_last_delta = _zero_vertical(_vector_sub(pelvis_pos[-1], pelvis_ref))
    root_last_delta = _zero_vertical(_vector_sub(root_pos[-1], root_pos[0]))
    _log(
        "{}: root delta=({:.3f}, {:.3f}, {:.3f}) len={:.3f}, pelvis delta=({:.3f}, {:.3f}, {:.3f}) len={:.3f}".format(
            anim.get_name(),
            root_last_delta.x, root_last_delta.y, root_last_delta.z, _vector_len(root_last_delta),
            pelvis_last_delta.x, pelvis_last_delta.y, pelvis_last_delta.z, _vector_len(pelvis_last_delta),
        )
    )
    if _vector_len(pelvis_last_delta) < 0.001:
        _warn("{}: pelvis has almost no net translation; try selecting the pre-retarget/source animation instead.".format(anim.get_name()))

    new_root_pos = []
    new_pelvis_pos = []

    for index in range(key_count):
        local_delta = _zero_vertical(_vector_sub(pelvis_pos[index], pelvis_ref))
        component_delta = _quat_rotate_vector(root_rot[index], _scaled_vector(local_delta, root_scale[index]))
        new_root_pos.append(_vector_add(root_pos[index], component_delta))
        new_pelvis_pos.append(_vector_sub(pelvis_pos[index], local_delta))

    controller = _anim_controller(baked)
    controller.open_bracket("Bake pelvis motion to root", False)
    try:
        if not controller.set_bone_track_keys(root_name, new_root_pos, root_rot, root_scale, False):
            raise RuntimeError("failed to set root track keys: {}".format(root_name))
        if not controller.set_bone_track_keys(pelvis_name, new_pelvis_pos, pelvis_rot, pelvis_scale, False):
            raise RuntimeError("failed to set pelvis track keys: {}".format(pelvis_name))
    finally:
        controller.close_bracket(False)

    # Make the duplicate easier to inspect in the editor.
    try:
        baked.set_editor_property("enable_root_motion", True)
    except Exception:
        pass

    unreal.EditorAssetLibrary.save_loaded_asset(baked)
    _log("created {}".format(baked.get_path_name()))
    return baked


def main():
    selected = unreal.EditorUtilityLibrary.get_selected_assets()
    anims = [asset for asset in selected if isinstance(asset, unreal.AnimSequence)]
    if not anims:
        _warn("Select one or more AnimSequence assets first.")
        return

    with unreal.ScopedSlowTask(len(anims), "Baking pelvis motion to root") as task:
        task.make_dialog(True)
        for anim in anims:
            if task.should_cancel():
                break
            task.enter_progress_frame(1, anim.get_name())
            bake_anim(anim)


main()

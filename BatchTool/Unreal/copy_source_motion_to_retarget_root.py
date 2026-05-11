"""
Copy source animation displacement onto a retargeted animation's root track.

Use this when a retargeted clip has the correct pose but lost locomotion.

Selection order is important:
    1. Select the original/source AnimSequence that still moves.
    2. Ctrl-select the retargeted AnimSequence whose pose you want to keep.
    3. Run this script in Unreal Editor Python.

Run:
    exec(open(r"D:/GitDesktop/Expedition_158/BatchTool/Unreal/copy_source_motion_to_retarget_root.py", encoding="utf-8").read())
"""

import unreal


SOURCE_MOTION_BONE_CANDIDATES = ("root", "Root", "pelvis", "Pelvis")
TARGET_ROOT_BONE_CANDIDATES = ("root", "Root")
SUFFIX = "_SourceMotion"
OUTPUT_FOLDER_NAME = "Create"

# Keep True for jump/dash arcs. Set False to copy only ground-plane motion.
COPY_VERTICAL = True


def _log(message):
    unreal.log("[CopySourceMotionToRoot] {}".format(message))


def _warn(message):
    unreal.log_warning("[CopySourceMotionToRoot] {}".format(message))


def _anim_lib():
    if hasattr(unreal, "AnimationLibrary"):
        return unreal.AnimationLibrary
    if hasattr(unreal, "AnimationBlueprintLibrary"):
        return unreal.AnimationBlueprintLibrary
    raise RuntimeError("AnimationLibrary is not available.")


def _anim_libs():
    libs = []
    for name in ("AnimationLibrary", "AnimationBlueprintLibrary"):
        if hasattr(unreal, name):
            libs.append(getattr(unreal, name))
    return libs


def _asset_base_path(asset):
    return asset.get_path_name().split(".")[0]


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


def _get_track(anim, bone_name):
    lib = _anim_lib()
    return {
        "pos": list(lib.get_raw_track_position_data(anim, bone_name)),
        "rot": list(lib.get_raw_track_rotation_data(anim, bone_name)),
        "scale": list(lib.get_raw_track_scale_data(anim, bone_name)),
    }


def _controller(anim):
    if hasattr(anim, "get_controller"):
        controller = anim.get_controller()
        if controller:
            return controller

    try:
        controller = anim.get_editor_property("controller")
        if controller:
            return controller
    except Exception:
        pass

    if hasattr(anim, "controller"):
        controller = anim.controller
        if controller:
            return controller

    return None


def _set_bone_track_keys(anim, bone_name, pos, rot, scale):
    controller = _controller(anim)
    if controller:
        try:
            controller.set_bone_track_keys(bone_name, pos, rot, scale, False)
        except TypeError:
            controller.set_bone_track_keys(bone_name, pos, rot, scale)
        return True

    for lib in _anim_libs():
        if not hasattr(lib, "set_bone_track_keys"):
            continue
        try:
            lib.set_bone_track_keys(anim, bone_name, pos, rot, scale)
            return True
        except TypeError:
            try:
                lib.set_bone_track_keys(anim, bone_name, pos, rot, scale, False)
                return True
            except TypeError:
                pass

    raise RuntimeError(
        "{} has no get_controller(), and AnimationLibrary has no set_bone_track_keys(). "
        "This UE build can read animation data but does not expose Python write access for bone tracks.".format(anim.get_name())
    )


def _transform_translation(transform):
    for attr in ("translation", "location"):
        if hasattr(transform, attr):
            return getattr(transform, attr)
    if hasattr(transform, "get_translation"):
        return transform.get_translation()
    try:
        return transform.get_editor_property("translation")
    except Exception:
        pass
    try:
        return transform.get_editor_property("location")
    except Exception:
        pass
    raise RuntimeError("Could not read Transform translation from {}".format(transform))


def _bone_exists(anim, bone_name):
    lib = _anim_lib()
    if hasattr(lib, "does_bone_name_exist"):
        try:
            return bool(lib.does_bone_name_exist(anim, bone_name))
        except Exception:
            return False
    return True


def _sample_bone_positions(anim, bone_name):
    lib = _anim_lib()
    if not hasattr(lib, "get_bone_pose_for_frame"):
        return []
    if not _bone_exists(anim, bone_name):
        return []

    if hasattr(lib, "get_num_keys"):
        frame_count = int(lib.get_num_keys(anim))
    elif hasattr(lib, "get_num_frames"):
        frame_count = int(lib.get_num_frames(anim)) + 1
    else:
        frame_count = 0

    if frame_count <= 1:
        return []

    positions = []
    for frame in range(frame_count):
        try:
            pose = lib.get_bone_pose_for_frame(anim, bone_name, frame, False)
        except Exception:
            return []
        positions.append(_transform_translation(pose))
    return positions


def _duplicate(anim):
    source_path = _asset_base_path(anim)
    source_dir, source_name = source_path.rsplit("/", 1)
    output_dir = "{}/{}".format(source_dir, OUTPUT_FOLDER_NAME)
    if not unreal.EditorAssetLibrary.does_directory_exist(output_dir):
        unreal.EditorAssetLibrary.make_directory(output_dir)

    target_path = "{}/{}{}".format(output_dir, source_name, SUFFIX)
    if unreal.EditorAssetLibrary.does_asset_exist(target_path):
        target_path = unreal.AssetToolsHelpers.get_asset_tools().create_unique_asset_name(target_path, "")[0]

    if not unreal.EditorAssetLibrary.duplicate_asset(source_path, target_path):
        raise RuntimeError("duplicate failed: {} -> {}".format(source_path, target_path))

    duplicated = unreal.EditorAssetLibrary.load_asset(target_path)
    if not duplicated:
        raise RuntimeError("failed to load duplicated asset: {}".format(target_path))
    return duplicated


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


def _lerp(a, b, alpha):
    return a + (b - a) * alpha


def _vector_lerp(a, b, alpha):
    return unreal.Vector(_lerp(a.x, b.x, alpha), _lerp(a.y, b.y, alpha), _lerp(a.z, b.z, alpha))


def _vector_sub(a, b):
    return unreal.Vector(a.x - b.x, a.y - b.y, a.z - b.z)


def _vector_add(a, b):
    return unreal.Vector(a.x + b.x, a.y + b.y, a.z + b.z)


def _vector_len(vec):
    return (vec.x * vec.x + vec.y * vec.y + vec.z * vec.z) ** 0.5


def _maybe_zero_vertical(vec):
    if COPY_VERTICAL:
        return vec
    return unreal.Vector(vec.x, vec.y, 0.0)


def _resample_vectors(keys, target_count):
    keys = list(keys)
    if target_count <= 0 or not keys:
        return []
    if len(keys) == target_count:
        return keys
    if len(keys) == 1 or target_count == 1:
        return [keys[0] for _ in range(target_count)]

    result = []
    source_last = len(keys) - 1
    target_last = target_count - 1
    for index in range(target_count):
        source_pos = (index / target_last) * source_last
        left = int(source_pos)
        right = min(left + 1, source_last)
        alpha = source_pos - left
        result.append(_vector_lerp(keys[left], keys[right], alpha))
    return result


def _choose_motion_track(source_anim):
    root_name = _find_track(source_anim, ("root", "Root"))
    pelvis_name = _find_track(source_anim, ("pelvis", "Pelvis"))
    candidates = []
    names = []
    for name in (root_name, pelvis_name):
        if not name:
            continue
        names.append(name)
    for fallback_name in ("root", "Root", "pelvis", "Pelvis"):
        if fallback_name not in names:
            names.append(fallback_name)

    for name in names:
        pos = []
        if name in _raw_track_names(source_anim):
            pos = _get_track(source_anim, name)["pos"]
        if len(pos) <= 1:
            pos = _sample_bone_positions(source_anim, name)
        if len(pos) > 1:
            delta = _maybe_zero_vertical(_vector_sub(pos[-1], pos[0]))
            candidates.append((name, _vector_len(delta), pos))
    if not candidates:
        _warn("{} raw tracks: {}".format(source_anim.get_name(), ", ".join(_raw_track_names(source_anim)[:80])))
        raise RuntimeError("{} has no usable root/pelvis motion tracks or sampled poses.".format(source_anim.get_name()))
    candidates.sort(key=lambda item: item[1], reverse=True)
    return candidates[0]


def copy_motion(source_anim, target_anim):
    motion_name, motion_len, source_motion = _choose_motion_track(source_anim)
    target_root_name = _find_track(target_anim, TARGET_ROOT_BONE_CANDIDATES)
    if not target_root_name:
        raise RuntimeError("{} has no root track.".format(target_anim.get_name()))

    baked = _duplicate(target_anim)
    target_root = _get_track(baked, target_root_name)

    key_count = max(len(target_root["pos"]), len(target_root["rot"]), len(target_root["scale"]))
    if key_count <= 1:
        key_count = len(source_motion)
    if key_count <= 1:
        raise RuntimeError("{} has too few keys to edit.".format(target_anim.get_name()))

    root_pos = _expanded(target_root["pos"], key_count)
    root_rot = _expanded(target_root["rot"], key_count)
    root_scale = _expanded(target_root["scale"], key_count)
    if not root_pos:
        root_pos = [unreal.Vector(0.0, 0.0, 0.0) for _ in range(key_count)]
    if not root_rot:
        root_rot = [unreal.Quat(0.0, 0.0, 0.0, 1.0) for _ in range(key_count)]
    if not root_scale:
        root_scale = [unreal.Vector(1.0, 1.0, 1.0) for _ in range(key_count)]
    source_pos = _resample_vectors(source_motion, key_count)
    source_ref = source_pos[0]

    new_root_pos = []
    for index in range(key_count):
        delta = _maybe_zero_vertical(_vector_sub(source_pos[index], source_ref))
        new_root_pos.append(_vector_add(root_pos[0], delta))

    last_delta = _maybe_zero_vertical(_vector_sub(source_pos[-1], source_ref))
    _log(
        "source={} motion_track={} delta=({:.3f}, {:.3f}, {:.3f}) len={:.3f} -> target={}".format(
            source_anim.get_name(),
            motion_name,
            last_delta.x,
            last_delta.y,
            last_delta.z,
            motion_len,
            baked.get_name(),
        )
    )

    controller = _controller(baked)
    if controller:
        controller.open_bracket("Copy source motion to root", False)
    try:
        if not _set_bone_track_keys(baked, target_root_name, new_root_pos, root_rot, root_scale):
            raise RuntimeError("failed to set root track keys: {}".format(target_root_name))
    finally:
        if controller:
            controller.close_bracket(False)

    try:
        baked.set_editor_property("enable_root_motion", True)
    except Exception:
        pass

    unreal.EditorAssetLibrary.save_loaded_asset(baked)
    _log("created {}".format(baked.get_path_name()))
    return baked


def main():
    selected = [asset for asset in unreal.EditorUtilityLibrary.get_selected_assets() if isinstance(asset, unreal.AnimSequence)]
    if len(selected) != 2:
        _warn("Select exactly 2 AnimSequence assets: source-moving animation first, retargeted-pose animation second.")
        _warn("Current AnimSequence selection count: {}".format(len(selected)))
        return

    source_anim, target_anim = selected[0], selected[1]
    copy_motion(source_anim, target_anim)


main()

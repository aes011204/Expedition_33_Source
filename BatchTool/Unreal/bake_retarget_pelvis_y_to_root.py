"""
Bake evaluated pelvis Y motion into root Y for retargeted AnimSequence assets.

Use this when an IK-retargeted clip looks like:
    root stays near the start, pelvis/body moves forward on Y.

Preferred selection:
    1. Select the original/source AnimSequence whose root Y motion is correct.
    2. Ctrl-select one or more retargeted AnimSequences whose pose you want to keep.

The source root Y curve is copied to the retargeted root, and the source pelvis
Y curve is copied to the retargeted pelvis. This matches clips where the source
root travels forward while source pelvis stays near a small gait offset.

Fallback selection is intentionally not supported here; this script needs the
source animation because guessing pelvis correction from the retarget result can
double-apply movement.

Run inside Unreal Editor Python:
    exec(open(r"D:/GitDesktop/Expedition_158/BatchTool/Unreal/bake_retarget_pelvis_y_to_root.py", encoding="utf-8").read())
"""

import unreal


ROOT_BONE_CANDIDATES = ("root", "Root")
PELVIS_BONE_CANDIDATES = ("pelvis", "Pelvis")
OUTPUT_FOLDER_NAME = "Create"
SUFFIX = "_RootYBake"

# Keep source pelvis local Y exactly. This is the mode needed when the source
# pelvis oscillates around small values like -1..-4 while root carries travel.
COPY_SOURCE_PELVIS_ABSOLUTE_Y = True


def _log(message):
    unreal.log("[BakeRetargetPelvisYToRoot] {}".format(message))


def _warn(message):
    unreal.log_warning("[BakeRetargetPelvisYToRoot] {}".format(message))


def _anim_lib():
    if hasattr(unreal, "AnimationLibrary"):
        return unreal.AnimationLibrary
    if hasattr(unreal, "AnimationBlueprintLibrary"):
        return unreal.AnimationBlueprintLibrary
    raise RuntimeError("AnimationLibrary is not available.")


def _raw_track_names(anim):
    lib = _anim_lib()
    if hasattr(lib, "get_animation_track_names"):
        return [str(name) for name in lib.get_animation_track_names(anim)]
    if hasattr(lib, "get_raw_track_names"):
        return [str(name) for name in lib.get_raw_track_names(anim)]
    return []


def _bone_exists(anim, bone_name):
    lib = _anim_lib()
    if hasattr(lib, "does_bone_name_exist"):
        try:
            return bool(lib.does_bone_name_exist(anim, bone_name))
        except Exception:
            return False
    return True


def _find_bone(anim, candidates):
    tracks = {name.lower(): name for name in _raw_track_names(anim)}
    for candidate in candidates:
        found = tracks.get(candidate.lower())
        if found:
            return found
    for candidate in candidates:
        if _bone_exists(anim, candidate):
            return candidate
    return None


def _find_track(anim, candidates):
    tracks = {name.lower(): name for name in _raw_track_names(anim)}
    for candidate in candidates:
        found = tracks.get(candidate.lower())
        if found:
            return found
    return None


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


def _ensure_bone_track(anim, bone_name):
    controller = _controller(anim)
    if not controller:
        return None

    if bone_name not in _raw_track_names(anim) and hasattr(controller, "add_bone_track"):
        added = None
        try:
            added = controller.add_bone_track(bone_name, False)
        except TypeError:
            added = controller.add_bone_track(bone_name)
        except Exception:
            added = None
        _log("{} add_bone_track({}) -> {}".format(anim.get_name(), bone_name, added))
    return controller


def _set_bone_track_keys(anim, bone_name, pos, rot, scale):
    controller = _ensure_bone_track(anim, bone_name)
    if controller:
        try:
            result = controller.set_bone_track_keys(bone_name, pos, rot, scale, False)
        except TypeError:
            result = controller.set_bone_track_keys(bone_name, pos, rot, scale)
        _log("{} set_bone_track_keys({}) -> {}".format(anim.get_name(), bone_name, result))
        if result is False:
            raise RuntimeError("{} failed to set {} track keys.".format(anim.get_name(), bone_name))
        return True

    for lib_name in ("AnimationLibrary", "AnimationBlueprintLibrary"):
        if not hasattr(unreal, lib_name):
            continue
        lib = getattr(unreal, lib_name)
        if not hasattr(lib, "set_bone_track_keys"):
            continue
        try:
            result = lib.set_bone_track_keys(anim, bone_name, pos, rot, scale)
            _log("{} {}.set_bone_track_keys({}) -> {}".format(anim.get_name(), lib_name, bone_name, result))
            if result is False:
                raise RuntimeError("{} failed to set {} track keys.".format(anim.get_name(), bone_name))
            return True
        except TypeError:
            try:
                result = lib.set_bone_track_keys(anim, bone_name, pos, rot, scale, False)
                _log("{} {}.set_bone_track_keys({}) -> {}".format(anim.get_name(), lib_name, bone_name, result))
                if result is False:
                    raise RuntimeError("{} failed to set {} track keys.".format(anim.get_name(), bone_name))
                return True
            except TypeError:
                pass

    raise RuntimeError("{} cannot write bone track keys in this UE Python build.".format(anim.get_name()))


def _asset_base_path(asset):
    return asset.get_path_name().split(".")[0]


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


def _vector_sub(a, b):
    return unreal.Vector(a.x - b.x, a.y - b.y, a.z - b.z)


def _vector_len(vec):
    return (vec.x * vec.x + vec.y * vec.y + vec.z * vec.z) ** 0.5


def _transform_rotation(transform):
    if hasattr(transform, "rotation"):
        return transform.rotation
    if hasattr(transform, "get_rotation"):
        return transform.get_rotation()
    try:
        return transform.get_editor_property("rotation")
    except Exception:
        return unreal.Quat(0.0, 0.0, 0.0, 1.0)


def _transform_scale(transform):
    for attr in ("scale3d", "scale"):
        if hasattr(transform, attr):
            return getattr(transform, attr)
    if hasattr(transform, "get_scale3d"):
        return transform.get_scale3d()
    try:
        return transform.get_editor_property("scale3d")
    except Exception:
        pass
    try:
        return transform.get_editor_property("scale")
    except Exception:
        pass
    return unreal.Vector(1.0, 1.0, 1.0)


def _frame_count(anim):
    lib = _anim_lib()
    if hasattr(lib, "get_num_keys"):
        return int(lib.get_num_keys(anim))
    if hasattr(lib, "get_num_frames"):
        return int(lib.get_num_frames(anim)) + 1
    raise RuntimeError("AnimationLibrary has no frame/key count function.")


def _raw_position_track(anim, candidates):
    track_name = _find_track(anim, candidates)
    if not track_name:
        return None
    lib = _anim_lib()
    if not hasattr(lib, "get_raw_track_position_data"):
        return None
    try:
        values = list(lib.get_raw_track_position_data(anim, track_name))
    except Exception:
        return None
    return values if values else None


def _lerp(a, b, alpha):
    return a + (b - a) * alpha


def _resample_floats(values, target_count):
    values = list(values)
    if not values or target_count <= 0:
        return []
    if len(values) == target_count:
        return values
    if len(values) == 1 or target_count == 1:
        return [values[0] for _ in range(target_count)]

    result = []
    source_last = len(values) - 1
    target_last = target_count - 1
    for index in range(target_count):
        source_pos = (index / target_last) * source_last
        left = int(source_pos)
        right = min(left + 1, source_last)
        alpha = source_pos - left
        result.append(_lerp(values[left], values[right], alpha))
    return result


def _sample_track(anim, bone_name):
    lib = _anim_lib()
    if not hasattr(lib, "get_bone_pose_for_frame"):
        raise RuntimeError("AnimationLibrary has no get_bone_pose_for_frame().")

    count = _frame_count(anim)
    if count <= 1:
        raise RuntimeError("{} has too few keys.".format(anim.get_name()))

    pos = []
    rot = []
    scale = []
    for frame in range(count):
        pose = lib.get_bone_pose_for_frame(anim, bone_name, frame, False)
        pos.append(_transform_translation(pose))
        rot.append(_transform_rotation(pose))
        scale.append(_transform_scale(pose))
    return {"pos": pos, "rot": rot, "scale": scale}


def _source_y_curves(source_anim):
    root_name = _find_bone(source_anim, ROOT_BONE_CANDIDATES)
    pelvis_name = _find_bone(source_anim, PELVIS_BONE_CANDIDATES)
    if not root_name or not pelvis_name:
        raise RuntimeError("{}: source root/pelvis bone not found".format(source_anim.get_name()))

    root_raw = _raw_position_track(source_anim, ROOT_BONE_CANDIDATES)
    pelvis_raw = _raw_position_track(source_anim, PELVIS_BONE_CANDIDATES)
    root_pos = root_raw if root_raw else _sample_track(source_anim, root_name)["pos"]
    pelvis_pos = pelvis_raw if pelvis_raw else _sample_track(source_anim, pelvis_name)["pos"]

    root_ref_y = root_pos[0].y
    root_curve = [pos.y - root_ref_y for pos in root_pos]
    pelvis_curve = [pos.y for pos in pelvis_pos]
    pelvis_min = min(pelvis_curve) if pelvis_curve else 0.0
    pelvis_max = max(pelvis_curve) if pelvis_curve else 0.0
    _log(
        "{} source root y delta {:.3f}, source pelvis y range {:.3f}..{:.3f}".format(
            source_anim.get_name(),
            root_curve[-1] if root_curve else 0.0,
            pelvis_min,
            pelvis_max,
        )
    )
    return root_curve, pelvis_curve


def bake_anim(anim, source_root_y, source_pelvis_y):
    root_name = _find_bone(anim, ROOT_BONE_CANDIDATES)
    pelvis_name = _find_bone(anim, PELVIS_BONE_CANDIDATES)
    if not root_name or not pelvis_name:
        _warn("{}: root/pelvis bone not found".format(anim.get_name()))
        return None

    baked = _duplicate(anim)
    root = _sample_track(baked, root_name)
    pelvis = _sample_track(baked, pelvis_name)

    root_ref = root["pos"][0]
    pelvis_ref = pelvis["pos"][0]
    root_y_curve = _resample_floats(source_root_y, len(root["pos"]))
    pelvis_y_curve = _resample_floats(source_pelvis_y, len(root["pos"]))

    new_root_pos = []
    new_pelvis_pos = []
    for index in range(len(root["pos"])):
        root_delta_y = root_y_curve[index]
        new_root_pos.append(unreal.Vector(root_ref.x, root_ref.y + root_delta_y, root_ref.z))
        if COPY_SOURCE_PELVIS_ABSOLUTE_Y:
            pelvis_y = pelvis_y_curve[index]
        else:
            pelvis_y = pelvis_ref.y + (pelvis_y_curve[index] - pelvis_y_curve[0])
        new_pelvis_pos.append(unreal.Vector(pelvis["pos"][index].x, pelvis_y, pelvis["pos"][index].z))

    last_root_delta = new_root_pos[-1].y - new_root_pos[0].y
    last_pelvis_delta = new_pelvis_pos[-1].y - new_pelvis_pos[0].y
    _log(
        "{}: source root y delta {:.3f} -> root y delta {:.3f}, source pelvis y range {:.3f}..{:.3f}, pelvis y delta after {:.3f}".format(
            anim.get_name(),
            root_y_curve[-1] if root_y_curve else 0.0,
            last_root_delta,
            min(pelvis_y_curve) if pelvis_y_curve else 0.0,
            max(pelvis_y_curve) if pelvis_y_curve else 0.0,
            last_pelvis_delta,
        )
    )

    controller = _controller(baked)
    if controller:
        controller.open_bracket("Bake evaluated pelvis Y to root Y", False)
    try:
        _set_bone_track_keys(baked, root_name, new_root_pos, root["rot"], root["scale"])
        _set_bone_track_keys(baked, pelvis_name, new_pelvis_pos, pelvis["rot"], pelvis["scale"])
    finally:
        if controller:
            controller.close_bracket(False)

    # Keep visual inspection simple: do not force root-motion extraction on.
    try:
        baked.set_editor_property("enable_root_motion", False)
    except Exception:
        pass

    unreal.EditorAssetLibrary.save_loaded_asset(baked)

    verify_root = _sample_track(baked, root_name)
    verify_pelvis = _sample_track(baked, pelvis_name)
    verify_root_delta = _vector_sub(verify_root["pos"][-1], verify_root["pos"][0])
    verify_pelvis_delta = _vector_sub(verify_pelvis["pos"][-1], verify_pelvis["pos"][0])
    _log(
        "{} verify: root delta=({:.3f}, {:.3f}, {:.3f}), pelvis delta=({:.3f}, {:.3f}, {:.3f}), pelvis len={:.3f}".format(
            baked.get_name(),
            verify_root_delta.x,
            verify_root_delta.y,
            verify_root_delta.z,
            verify_pelvis_delta.x,
            verify_pelvis_delta.y,
            verify_pelvis_delta.z,
            _vector_len(verify_pelvis_delta),
        )
    )
    if abs(verify_pelvis_delta.y) > 1.0:
        _warn(
            "{}: pelvis Y is still moving after bake. Root was written, but pelvis correction did not take effect.".format(
                baked.get_name()
            )
        )

    _log("created {}".format(baked.get_path_name()))
    return baked


def main():
    selected = [asset for asset in unreal.EditorUtilityLibrary.get_selected_assets() if isinstance(asset, unreal.AnimSequence)]
    if len(selected) < 2:
        _warn("Select source AnimSequence first, then one or more retargeted AnimSequences.")
        return

    source_anim = selected[0]
    source_root_y, source_pelvis_y = _source_y_curves(source_anim)
    targets = selected[1:]

    with unreal.ScopedSlowTask(len(targets), "Bake retarget/source Y to root Y") as task:
        task.make_dialog(True)
        for anim in targets:
            if task.should_cancel():
                break
            task.enter_progress_frame(1, anim.get_name())
            bake_anim(anim, source_root_y, source_pelvis_y)


main()

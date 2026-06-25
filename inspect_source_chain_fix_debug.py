"""
Inspect source-chain-fixed animations and print raw/evaluated first-frame rotations
for the left arm chain so we can confirm whether the source fix actually applied.

Usage:
- Set ASSET_PATH to one anim in /Game/Retarget/.../Animation/_SourceChainFixed
- Run in Unreal Editor Python
"""

import unreal


ASSET_PATH = "/Game/Retarget/Lune/Animation/_SourceChainFixed/REPLACE_ME"
BONES_TO_CHECK = [
	("clavicle_l", "Clavicle_l", "Clavicle_L", "clavicle_L"),
	("upperarm_l", "UpperArm_L", "Upperarm_l", "upperarm_L"),
]


def _log(msg):
	unreal.log("[InspectSourceChainFix] {}".format(msg))


def _anim_lib():
	if hasattr(unreal, "AnimationLibrary"):
		return unreal.AnimationLibrary
	if hasattr(unreal, "AnimationBlueprintLibrary"):
		return unreal.AnimationBlueprintLibrary
	raise RuntimeError("AnimationLibrary is not available.")


def _math_lib():
	for lib_name in ("MathLibrary", "KismetMathLibrary"):
		if hasattr(unreal, lib_name):
			return getattr(unreal, lib_name)
	return None


def _raw_track_names(anim):
	lib = _anim_lib()
	if hasattr(lib, "get_animation_track_names"):
		return [str(name) for name in lib.get_animation_track_names(anim)]
	if hasattr(lib, "get_raw_track_names"):
		return [str(name) for name in lib.get_raw_track_names(anim)]
	return []


def _find_track(anim, candidates):
	tracks = {name.lower(): name for name in _raw_track_names(anim)}
	for candidate in candidates:
		found = tracks.get(candidate.lower())
		if found:
			return found
	return None


def _raw_rotation_track(anim, candidates):
	track_name = _find_track(anim, candidates)
	if not track_name:
		return None, None
	lib = _anim_lib()
	if not hasattr(lib, "get_raw_track_rotation_data"):
		return track_name, None
	try:
		values = list(lib.get_raw_track_rotation_data(anim, track_name))
	except Exception:
		values = None
	return track_name, values


def _quat_to_rotator(quat):
	if hasattr(quat, "rotator"):
		return quat.rotator()
	math_lib = _math_lib()
	if math_lib:
		for fn_name in ("quat_rotator", "conv_quat_to_rotator"):
			if hasattr(math_lib, fn_name):
				return getattr(math_lib, fn_name)(quat)
	raise RuntimeError("Cannot convert quaternion to rotator.")


def _transform_rotation(transform):
	if hasattr(transform, "rotation"):
		return transform.rotation
	if hasattr(transform, "get_rotation"):
		return transform.get_rotation()
	try:
		return transform.get_editor_property("rotation")
	except Exception:
		return unreal.Quat(0.0, 0.0, 0.0, 1.0)


def main():
	anim = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
	if not anim:
		raise RuntimeError("Failed to load {}".format(ASSET_PATH))

	lib = _anim_lib()
	_log("Inspecting {}".format(ASSET_PATH))
	_log("Raw tracks: {}".format(", ".join(_raw_track_names(anim))))

	for candidates in BONES_TO_CHECK:
		track_name, raw_track = _raw_rotation_track(anim, candidates)
		if not track_name:
			_log("Bone candidates {} -> raw track not found".format(candidates))
			continue

		_log("Bone {} -> track '{}'".format(candidates[0], track_name))

		if raw_track:
			raw_rot = _quat_to_rotator(raw_track[0])
			_log(
				"  raw frame0 XYZ(display-ish) = ({:.3f}, {:.3f}, {:.3f})".format(
					getattr(raw_rot, "roll", 0.0),
					getattr(raw_rot, "pitch", 0.0),
					getattr(raw_rot, "yaw", 0.0),
				)
			)
		else:
			_log("  raw rotation track empty")

		try:
			pose = lib.get_bone_pose_for_frame(anim, track_name, 0, False)
			eval_rot = _quat_to_rotator(_transform_rotation(pose))
			_log(
				"  eval frame0 XYZ = ({:.3f}, {:.3f}, {:.3f})".format(
					getattr(eval_rot, "roll", 0.0),
					getattr(eval_rot, "pitch", 0.0),
					getattr(eval_rot, "yaw", 0.0),
				)
			)
		except Exception as exc:
			_log("  eval read failed: {}".format(exc))


if __name__ == "__main__":
	main()

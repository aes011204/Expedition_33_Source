#pragma once

enum class PreviewShotPanel
{
    CharacterStatus,
    SkillTree,
    CharacterProgression,
    ChangeWeapon,
    ChangeSkill,
    Pictos,
    Wardrobe,
};

enum class TargetCameraMode { NORMAL, AIM, BATTLE_ENTER, SOULLIKE, };
enum class BattleCameraMode
{ WIDE,
  BATTLEINTRO,
  ENEMYFOCUS,  //공격 대상 선택시 줌
  ENEMYATTACK, //적 공격
  PLAYERFOCUS, //플레이어 공격 선택시 줌
  PLAYERATTACK,
  SELECTITEM,
  SELECTSKILL,
  PARRY,
  AIM,
  SELECTPLAYER

  // 순서 수정 금지
};

enum class BattleCameraTransitionEasing
{
    Linear,
    EaseInOut,
    EaseIn,
    EaseOut,
    EaseInStrong,
    EaseOutStrong,
    EaseInOutStrong,
    EaseInCubic,
    EaseOutCubic,
    EaseInExpo,
    EaseOutExpo,
    EaseOutBack
};

enum class SoulLikeCameraFocusMode
{
    Player,
    PlayerOpponentMidpoint,
    Opponent
};

struct PreviewShotPreset
{
    Vec3 lookOffset;
    float followSpeed;
    float distance;
};

struct CommonCameraPreset
{
    float        fovy = XMConvertToRadians(45.f);
    float        nearPlane = 0.1f;
    float        farPlane = 500.f;

    float        mouseSensor = 0.3f;
    float        mouseSensitivity = 0.1f;
};

struct DialogueCameraDriftProfile
{
    bool enabled = false;

    float lateralAmplitude = 0.06f;
    float verticalAmplitude = 0.03f;
    float depthAmplitude = 0.02f;
    float rollAmplitude = 0.01f;

    float lateralFrequency = 0.65f;
    float verticalFrequency = 0.92f;
    float depthFrequency = 0.48f;
    float rollFrequency = 0.55f;
};

struct DialogueCameraPreset
{
    CommonCameraPreset camera;

    Vec3         localCameraOffset = Vec3(0.7f, 1.55f, 1.2f);
    Vec3         localLookTargetOffset = Vec3(0.f, 1.35f, 0.f);

    float        rollOffset = 0.f;
    DialogueCameraDriftProfile drift{};
};

inline bool Evaluate_DialoguePresetPoseNpcAnchor(
    const Vec3& anchorPos,
    Vec3 anchorForward,
    const DialogueCameraPreset& preset,
    Vec3& outCameraPos,
    Vec3& outLookTarget,
    float* outFovY = nullptr,
    float* outRollOffset = nullptr)
{
    anchorForward.y = 0.f;
    if (anchorForward.LengthSquared() <= 0.0001f)
        anchorForward = Vec3::Look;
    else
        anchorForward.Normalize();

    Vec3 rightDir = Vec3::Up.Cross(anchorForward);
    if (rightDir.LengthSquared() <= 0.0001f)
        rightDir = Vec3::Right;
    else
        rightDir.Normalize();

    outCameraPos =
        anchorPos +
        rightDir * preset.localCameraOffset.x +
        Vec3::Up * preset.localCameraOffset.y +
        anchorForward * preset.localCameraOffset.z;

    outLookTarget =
        anchorPos +
        rightDir * preset.localLookTargetOffset.x +
        Vec3::Up * preset.localLookTargetOffset.y +
        anchorForward * preset.localLookTargetOffset.z;

    if (outFovY != nullptr)
        *outFovY = preset.camera.fovy;

    if (outRollOffset != nullptr)
        *outRollOffset = preset.rollOffset;

    return true;
}

struct TargetCameraPreset
{
    CommonCameraPreset camera;
    bool         snapOnEnter = false;
    SoulLikeCameraFocusMode focusMode = SoulLikeCameraFocusMode::Player;

    float        followSpeed = 8.f;
    float        targetFollowSpeed = 4.f; // 플레이어의 위치를 카메라가 따라가는 속도
    float        verticalFollowSpeed = 10.f; // 플레이어의 높이를 카메라가 따라가는 속도
    BattleCameraTransitionEasing targetFollowEasing = BattleCameraTransitionEasing::EaseOut; // 수평 추적 곡선

    float        distance = 2.51f;

    float        sideOffset = 0.f;
    float        heightOffset = 0.65f;
    float        aimOffset = 0.5f;

    float        pitchLookScale = 0.5f;

    float        pitchMin = -60.f;
    float        pitchMax = 60.f;

    float        yawOffset = 0.f;
    float        pitchOffset = 0.f;
    float        rollOffset = 0.f;

    float        lookHeight = 1.13f;
    float        pivotHeight = 0.4f;

    bool         allowMouseRotation = true;

    /* Camera Collision Test */
    bool         doCollisionTest = true;
    float        collisionProbeRadius = 0.2f;
    float        collisionSkin = 0.03f;

};

struct BattleCameraDriftProfile
{
    bool enabled = false;

    float lateralAmplitude = 1.6f;
    float verticalAmplitude = 0.8f;
    float depthAmplitude = 0.35f;
    float rollAmplitude = 0.035f;

    float lateralFrequency = 0.65f;
    float verticalFrequency = 0.92f;
    float depthFrequency = 0.48f;
    float rollFrequency = 0.55f;
};

struct BattleCameraPreset
{
    CommonCameraPreset camera;
    bool         snapOnEnter = false;
    BattleCameraTransitionEasing transitionEasing = BattleCameraTransitionEasing::EaseInOut;
    float        transitionDuration = 0.35f;
    float        followSpeed;
    float        distance;

    float        sideOffset;
    float        heightOffset;
    float        aimOffset;

    float        pitchLookScale;

    float        pitchMin;
    float        pitchMax;

    float        yawOffset = 0.f;
    float        pitchOffset = 0.f;
    float        rollOffset = 0.f;

    float        lookHeight;
    float        pivotHeight;
};

struct SkillPresentationHiddenHero
{
    Shared<GameObject> actor{};
    bool wasVisible = false;
};

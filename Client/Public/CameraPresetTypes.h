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

enum class TargetCameraMode { NORMAL, AIM, BATTLE_ENTER };
enum class BattleCameraMode
{ WIDE,
  BATTLEINTRO,
  ENEMYFOCUS, //공격 대상 선택시 줌
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

struct TargetCameraPreset
{
    CommonCameraPreset camera;
    bool         snapOnEnter = false;

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

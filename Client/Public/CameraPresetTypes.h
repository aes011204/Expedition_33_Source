#pragma once

enum class PreviewShotPanel
{
    CharacterStatus,
    SkillTree
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
  SELECTSKILL
};

enum class BattleCameraTransitionEasing
{
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
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

    bool         allowMouseRotation = true;

    /* Camera Collision Test */
    bool         doCollisionTest = true;
    float        collisionProbeRadius = 0.2f;
    float        collisionSkin = 0.03f;

};


struct BattleCameraPreset
{
    CommonCameraPreset camera;
    bool         snapOnEnter = false;
    BattleCameraTransitionEasing transitionEasing = BattleCameraTransitionEasing::EaseInOut;

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

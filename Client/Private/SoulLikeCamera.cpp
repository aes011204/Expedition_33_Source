#include "SoulLikeCamera.h"

#include "CameraPresetIO.h"
#include "Collision_Define.h"
#include "GameInstance.h"
#include "PhysX_Types.h"

namespace
{
    float Evaluate_SoulLikeFollowT(BattleCameraTransitionEasing easing, float t)
    {
        t = clamp(t, 0.f, 1.f);
        constexpr float kBackOvershoot = 1.70158f;

        switch (easing)
        {
        case BattleCameraTransitionEasing::Linear:
            return t;

        case BattleCameraTransitionEasing::EaseIn:
            return t * t;

        case BattleCameraTransitionEasing::EaseOut:
            return 1.f - (1.f - t) * (1.f - t);

        case BattleCameraTransitionEasing::EaseInStrong:
            return t * t * t;

        case BattleCameraTransitionEasing::EaseOutStrong:
            return 1.f - powf(1.f - t, 3.f);

        case BattleCameraTransitionEasing::EaseInOutStrong:
            return t < 0.5f
                ? 4.f * t * t * t
                : 1.f - powf(-2.f * t + 2.f, 3.f) * 0.5f;

        case BattleCameraTransitionEasing::EaseInCubic:
            return t * t * t;

        case BattleCameraTransitionEasing::EaseOutCubic:
            return 1.f - powf(1.f - t, 3.f);

        case BattleCameraTransitionEasing::EaseInExpo:
            return t <= 0.f ? 0.f : powf(2.f, 10.f * t - 10.f);

        case BattleCameraTransitionEasing::EaseOutExpo:
            return t >= 1.f ? 1.f : 1.f - powf(2.f, -10.f * t);

        case BattleCameraTransitionEasing::EaseOutBack:
        {
            const float x = t - 1.f;
            return 1.f + (kBackOvershoot + 1.f) * x * x * x + kBackOvershoot * x * x;
        }

        case BattleCameraTransitionEasing::EaseInOut:
        default:
            return t * t * (3.f - 2.f * t);
        }
    }
}

NS_BEGIN(Client)

IMPLEMENT_REFLECTION(SoulLikeCamera)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "SoulLikeCamera";
    info.category = "Camera";

    return true;
}

SoulLikeCamera::SoulLikeCamera(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Camera(device, context)
{
}

SoulLikeCamera::SoulLikeCamera(const SoulLikeCamera& prototype)
    : Camera(prototype)
{
}

HRESULT SoulLikeCamera::Initialize_Prototype()
{
    CHECK_FAILED(Camera::Initialize_Prototype(), E_FAIL);

    return S_OK;
}

HRESULT SoulLikeCamera::Initialize(void* arg)
{
    CHECK_NULL(arg, E_FAIL);

    const auto desc = static_cast<SOULLIKE_CAMERA_DESC*>(arg);
    Build_DefaultPreset(*desc);

    CHECK_FAILED(Camera::Initialize(arg), E_FAIL);

    CameraPresetIO::LoadTargetPresetFromFile(TargetCameraMode::SOULLIKE, _targetSettings);
    _baseSettings = _targetSettings;
    _currentSettings = _targetSettings;

    Set_FovY(_currentSettings.camera.fovy);
    Set_Near(_currentSettings.camera.nearPlane);
    Set_Far(_currentSettings.camera.farPlane);

    _yaw = 0.f;
    _pitch = 0.f;

    _collisionQueryAccum = 0.f;
    _hasCollisionQueryCache = false;
    _lastCollisionSweepStart = Vec3::Zero;
    _lastCollisionDesiredPos = Vec3::Zero;
    _lastCollisionResolvedPos = Vec3::Zero;

    Set_Name(L"SoulLike Camera");

    return S_OK;
}

void SoulLikeCamera::BeginPlay()
{
    Camera::BeginPlay();

    GAME->Add_Camera(GetSharedPtr<Camera>());
}

void SoulLikeCamera::OnActivated()
{
    Shared<TransformCom> targetTransformCom;
    if (Try_GetTargetTransform(targetTransformCom))
    {
        const Vec3 targetPos = targetTransformCom->Get_WorldPosition();
        _prevTargetPos = targetPos;
        _smoothedTargetPos = targetPos;
    }

    Update_FromCurrentSettings(0.f);
    Apply_View();
}

void SoulLikeCamera::Priority_Update(float timeDelta)
{
    Camera::Priority_Update(timeDelta);
}

void SoulLikeCamera::Update(float timeDelta)
{
    Camera::Update(timeDelta);

    if (!GAME->Is_ActiveCamera(GetSharedPtr<Camera>()))
        return;

    if (_targetSettings.allowMouseRotation && !GAME->Is_WorldGameplayInputBlocked())
    {
        const Vec2 mouseDelta = GAME->Get_MouseDelta();

        _yaw += mouseDelta.x * _currentSettings.camera.mouseSensor;
        _pitch += mouseDelta.y * _currentSettings.camera.mouseSensor * _currentSettings.camera.mouseSensitivity;
        _pitch = clamp(_pitch, _currentSettings.pitchMin, _currentSettings.pitchMax);
    }

    Interpolate_CurrentSettings(timeDelta);
    Update_FromCurrentSettings(timeDelta);
}

void SoulLikeCamera::Late_Update(float timeDelta)
{
    Camera::Late_Update(timeDelta);
}

HRESULT SoulLikeCamera::Render()
{
    return Camera::Render();
}

void SoulLikeCamera::SetTargetTransform(Shared<TransformCom> targetTransform)
{
    _targetTransformCom = targetTransform;

    if (targetTransform)
    {
        const Vec3 targetPos = targetTransform->Get_WorldPosition();
        _prevTargetPos = targetPos;
        _smoothedTargetPos = targetPos;
    }

    _collisionQueryAccum = 0.f;
    _hasCollisionQueryCache = false;
    _collisionDistance = -1.f;
}

bool SoulLikeCamera::Try_GetFlatMoveBasis(Vec3& outForward, Vec3& outRight) const
{
    const float finalYaw = _yaw + _currentSettings.yawOffset;
    const float yawRad = XMConvertToRadians(finalYaw);

    outForward.x = sinf(yawRad);
    outForward.y = 0.f;
    outForward.z = cosf(yawRad);
    outForward.Normalize();

    outRight.x = cosf(yawRad);
    outRight.y = 0.f;
    outRight.z = -sinf(yawRad);
    outRight.Normalize();

    return true;
}

const TargetCameraPreset& SoulLikeCamera::Get_Preset() const
{
    return _targetSettings;
}

void SoulLikeCamera::Set_Preset(const TargetCameraPreset& preset)
{
    _baseSettings = preset;
    _targetSettings = preset;
    _activePresetKey.clear();
}

void SoulLikeCamera::Apply_PreviewPreset(const TargetCameraPreset& preset)
{
    _baseSettings = preset;
    _currentSettings = preset;
    _targetSettings = preset;
    _activePresetKey.clear();

    Set_FovY(_currentSettings.camera.fovy);
    Set_Near(_currentSettings.camera.nearPlane);
    Set_Far(_currentSettings.camera.farPlane);

    _collisionQueryAccum = 0.f;
    _hasCollisionQueryCache = false;
    _collisionDistance = -1.f;
}

bool SoulLikeCamera::Apply_PresetOverride(const string& presetKey)
{
    if (presetKey.empty())
        return false;

    TargetCameraPreset overridePreset = _baseSettings;
    if (!CameraPresetIO::LoadSoulLikePresetFromFile(presetKey, overridePreset))
    {
        Restore_BasePreset();
        return false;
    }

    _targetSettings = overridePreset;
    _activePresetKey = presetKey;

    if (_targetSettings.snapOnEnter)
        _currentSettings = _targetSettings;

    Set_FovY(_currentSettings.camera.fovy);
    Set_Near(_currentSettings.camera.nearPlane);
    Set_Far(_currentSettings.camera.farPlane);

    _collisionQueryAccum = 0.f;
    _hasCollisionQueryCache = false;
    _collisionDistance = -1.f;

    return true;
}

bool SoulLikeCamera::Apply_SkillPreset(const string& skillKey)
{
    if (skillKey.empty())
        return false;

    return Apply_PresetOverride("PlayerSkill_" + skillKey);
}

void SoulLikeCamera::Restore_BasePreset()
{
    if (_activePresetKey.empty())
        return;

    _activePresetKey.clear();
    _targetSettings = _baseSettings;

    if (_targetSettings.snapOnEnter)
        _currentSettings = _targetSettings;

    Set_FovY(_currentSettings.camera.fovy);
    Set_Near(_currentSettings.camera.nearPlane);
    Set_Far(_currentSettings.camera.farPlane);

    _collisionQueryAccum = 0.f;
    _hasCollisionQueryCache = false;
    _collisionDistance = -1.f;
}

void SoulLikeCamera::Build_DefaultPreset(const SOULLIKE_CAMERA_DESC& desc)
{
    _targetSettings.followSpeed = desc.followSpeed;
    _targetSettings.targetFollowSpeed = desc.targetFollowSpeed;
    _targetSettings.verticalFollowSpeed = desc.verticalFollowSpeed;
    _targetSettings.distance = desc.distance;
    _targetSettings.sideOffset = desc.sideOffset;
    _targetSettings.heightOffset = desc.heightOffset;
    _targetSettings.aimOffset = desc.aimOffset;
    _targetSettings.pitchLookScale = desc.pitchLookScale;
    _targetSettings.pitchMin = desc.pitchMin;
    _targetSettings.pitchMax = desc.pitchMax;
    _targetSettings.yawOffset = desc.yawOffset;
    _targetSettings.pitchOffset = desc.pitchOffset;
    _targetSettings.rollOffset = desc.rollOffset;
    _targetSettings.lookHeight = desc.lookHeight;
    _targetSettings.pivotHeight = desc.pivotHeight;
    _targetSettings.allowMouseRotation = desc.allowMouseRotation;
    _targetSettings.doCollisionTest = desc.doCollisionTest;
    _targetSettings.collisionProbeRadius = desc.collisionProbeRadius;
    _targetSettings.collisionSkin = desc.collisionSkin;
    _targetSettings.camera.fovy = desc.fovy;
    _targetSettings.camera.nearPlane = desc.nearPlane;
    _targetSettings.camera.farPlane = desc.farPlane;
    _targetSettings.camera.mouseSensor = desc.mouseSensor;
    _targetSettings.camera.mouseSensitivity = desc.mouseSensitivity;

    _baseSettings = _targetSettings;
    _currentSettings = _targetSettings;
}

void SoulLikeCamera::Interpolate_CurrentSettings(float timeDelta)
{
    const float t = clamp(timeDelta * _targetSettings.followSpeed, 0.f, 1.f);

    _currentSettings.aimOffset = lerp(_currentSettings.aimOffset, _targetSettings.aimOffset, t);
    _currentSettings.distance = lerp(_currentSettings.distance, _targetSettings.distance, t);
    _currentSettings.heightOffset = lerp(_currentSettings.heightOffset, _targetSettings.heightOffset, t);
    _currentSettings.lookHeight = lerp(_currentSettings.lookHeight, _targetSettings.lookHeight, t);
    _currentSettings.pitchLookScale = lerp(_currentSettings.pitchLookScale, _targetSettings.pitchLookScale, t);
    _currentSettings.pivotHeight = lerp(_currentSettings.pivotHeight, _targetSettings.pivotHeight, t);
    _currentSettings.sideOffset = lerp(_currentSettings.sideOffset, _targetSettings.sideOffset, t);
    _currentSettings.targetFollowSpeed = lerp(_currentSettings.targetFollowSpeed, _targetSettings.targetFollowSpeed, t);
    _currentSettings.verticalFollowSpeed = lerp(_currentSettings.verticalFollowSpeed, _targetSettings.verticalFollowSpeed, t);
    _currentSettings.pitchOffset = lerp(_currentSettings.pitchOffset, _targetSettings.pitchOffset, t);
    _currentSettings.yawOffset = lerp(_currentSettings.yawOffset, _targetSettings.yawOffset, t);
    _currentSettings.rollOffset = lerp(_currentSettings.rollOffset, _targetSettings.rollOffset, t);
    _currentSettings.camera.fovy = lerp(_currentSettings.camera.fovy, _targetSettings.camera.fovy, t);
    _currentSettings.camera.nearPlane = lerp(_currentSettings.camera.nearPlane, _targetSettings.camera.nearPlane, t);
    _currentSettings.camera.farPlane = lerp(_currentSettings.camera.farPlane, _targetSettings.camera.farPlane, t);
    _currentSettings.targetFollowEasing = _targetSettings.targetFollowEasing;

    Set_FovY(_currentSettings.camera.fovy);
    Set_Near(_currentSettings.camera.nearPlane);
    Set_Far(_currentSettings.camera.farPlane);
}

bool SoulLikeCamera::Try_GetTargetTransform(Shared<TransformCom>& outTargetTransformCom) const
{
    if (_targetTransformCom.expired())
        return false;

    outTargetTransformCom = _targetTransformCom.lock();
    return outTargetTransformCom != nullptr;
}

void SoulLikeCamera::Update_SmoothedTargetPosition(const Vec3& targetPos, float timeDelta)
{
    constexpr float kTargetStillXZFrameDeltaSq = 0.000025f;
    constexpr float kHorizontalSettleDistanceSq = 0.0004f;
    constexpr float kTargetStillYFrameDelta = 0.005f;
    constexpr float kVerticalSettleDistance = 0.035f;

    const Vec3 targetDelta = targetPos - _prevTargetPos;
    const float horizontalFollowT = Evaluate_SoulLikeFollowT(
        _currentSettings.targetFollowEasing,
        timeDelta * _currentSettings.targetFollowSpeed);

    const Vec3 currentHorizontal{ _smoothedTargetPos.x, 0.f, _smoothedTargetPos.z };
    const Vec3 targetHorizontal{ targetPos.x, 0.f, targetPos.z };
    const float horizontalDeltaSq = (targetHorizontal - currentHorizontal).LengthSquared();

    if (horizontalDeltaSq <= 0.000001f ||
        (targetDelta.x * targetDelta.x + targetDelta.z * targetDelta.z <= kTargetStillXZFrameDeltaSq &&
            horizontalDeltaSq <= kHorizontalSettleDistanceSq))
    {
        _smoothedTargetPos.x = targetPos.x;
        _smoothedTargetPos.z = targetPos.z;
    }
    else
    {
        _smoothedTargetPos.x = lerp(_smoothedTargetPos.x, targetPos.x, horizontalFollowT);
        _smoothedTargetPos.z = lerp(_smoothedTargetPos.z, targetPos.z, horizontalFollowT);
    }

    const float yDelta = fabsf(targetPos.y - _smoothedTargetPos.y);
    const float targetYFrameDelta = fabsf(targetDelta.y);

    if (yDelta <= 0.0001f ||
        (targetYFrameDelta <= kTargetStillYFrameDelta && yDelta <= kVerticalSettleDistance))
    {
        _smoothedTargetPos.y = targetPos.y;
    }
    else
    {
        _smoothedTargetPos.y = Math::Lerp_Damp(
            _smoothedTargetPos.y,
            targetPos.y,
            _currentSettings.verticalFollowSpeed,
            timeDelta);
    }
}

void SoulLikeCamera::Update_FromCurrentSettings(float timeDelta)
{
    Shared<TransformCom> targetTransformCom;
    if (!Try_GetTargetTransform(targetTransformCom))
        return;

    const Vec3 targetPos = targetTransformCom->Get_WorldPosition();

    const float finalYaw = _yaw + _currentSettings.yawOffset;
    const float finalPitch = _pitch + _currentSettings.pitchOffset;

    const float yawRad = XMConvertToRadians(finalYaw);
    const float pitchRad = XMConvertToRadians(finalPitch);

    Vec3 backDir;
    backDir.x = -sinf(yawRad);
    backDir.y = 0.f;
    backDir.z = -cosf(yawRad);
    backDir.Normalize();

    Vec3 rightDir;
    rightDir.x = cosf(yawRad);
    rightDir.y = 0.f;
    rightDir.z = -sinf(yawRad);
    rightDir.Normalize();

    const Vec3 baseOffset = backDir * _currentSettings.distance;

    const Matrix pitchRotation = XMMatrixRotationAxis(rightDir, pitchRad);
    const Vec3 orbitOffset = XMVector3TransformNormal(baseOffset, pitchRotation);

    const Vec3 prevSmoothedTargetPos = _smoothedTargetPos;
    Update_SmoothedTargetPosition(targetPos, timeDelta);
    const bool smoothedTargetMoved =
        (_smoothedTargetPos - prevSmoothedTargetPos).LengthSquared() > 0.000025f;

    const Vec3 pivotPos = _smoothedTargetPos + Vec3::Up * _currentSettings.pivotHeight;
    const Vec3 framingPos = pivotPos + rightDir * _currentSettings.aimOffset;
    Vec3 desiredPos = framingPos + orbitOffset + Vec3::Up * _currentSettings.heightOffset + rightDir * _currentSettings.sideOffset;
    const Vec3 lookTarget = framingPos + Vec3::Up * _currentSettings.lookHeight;

    const Vec3 collisionStart = lookTarget;
    Vec3 newPos = desiredPos;

    if (_currentSettings.doCollisionTest)
    {
        const float sweepStartDeltaSq = (collisionStart - _lastCollisionSweepStart).LengthSquared();
        const float desiredPosDeltaSq = (desiredPos - _lastCollisionDesiredPos).LengthSquared();

        const bool movedEnoughForImmediateQuery =
            sweepStartDeltaSq > 0.0004f ||
            desiredPosDeltaSq > 0.0004f ||
            smoothedTargetMoved;

        if (!_hasCollisionQueryCache || movedEnoughForImmediateQuery)
        {
            _collisionQueryAccum = 0.f;

            newPos = Resolve_Collision(collisionStart, desiredPos, timeDelta);

            _hasCollisionQueryCache = true;
            _lastCollisionSweepStart = collisionStart;
            _lastCollisionDesiredPos = desiredPos;
            _lastCollisionResolvedPos = newPos;
        }
        else
        {
            _collisionQueryAccum += timeDelta;

            if (_collisionQueryAccum >= _collisionQueryInterval)
            {
                _collisionQueryAccum = 0.f;

                newPos = Resolve_Collision(collisionStart, desiredPos, timeDelta);

                _lastCollisionSweepStart = collisionStart;
                _lastCollisionDesiredPos = desiredPos;
                _lastCollisionResolvedPos = newPos;
            }
            else
            {
                newPos = _lastCollisionResolvedPos;
            }
        }
    }

    _transformCom->Set_WorldPosition(newPos);
    _transformCom->LookAt(lookTarget);
    _transformCom->Add_LocalRotationQuaternion(Quat::CreateFromAxisAngle(Vec3::Forward, _currentSettings.rollOffset));

    Update_TransformMatrices();
    GAME->UpdateFrustum(*GAME->Get_Transform(D3DTS::View), *GAME->Get_Transform(D3DTS::Proj));

    _prevTargetPos = targetPos;
}

Vec3 SoulLikeCamera::Resolve_Collision(const Vec3& sweepStart, const Vec3& desiredPos, float timeDelta)
{
    if (!_currentSettings.doCollisionTest)
        return desiredPos;

    Vec3 sweep = desiredPos - sweepStart;
    const float desiredDistance = sweep.Length();

    if (desiredDistance <= 0.0001f)
        return desiredPos;

    sweep /= desiredDistance;

    float targetDistance = desiredDistance;

    PhysXQueryHit hit{};
    const bool blocked = GAME->Sweep_Sphere(
        sweepStart, sweep, desiredDistance, _currentSettings.collisionProbeRadius, Collision_Preset::Camera, hit);

    if (blocked)
        targetDistance = max(0.f, hit.distance - _currentSettings.collisionSkin);

    if (_collisionDistance < 0.f)
        _collisionDistance = targetDistance;

    const float distanceDelta = targetDistance - _collisionDistance;
    if (fabsf(distanceDelta) <= _collisionDeadZone)
        return sweepStart + sweep * _collisionDistance;

    const bool shouldPullIn = distanceDelta < 0.f;
    const float lerpSpeed = shouldPullIn ? _collisionPullSpeed : _collisionReturnSpeed;

    _collisionDistance = Math::Lerp_Damp(
        _collisionDistance,
        targetDistance,
        lerpSpeed,
        timeDelta);

    return sweepStart + sweep * _collisionDistance;
}

Shared<SoulLikeCamera> SoulLikeCamera::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<SoulLikeCamera>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Create : SoulLikeCamera");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> SoulLikeCamera::Clone(void* arg)
{
    auto instance = make_shared<SoulLikeCamera>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        MSG_BOX("Failed to Clone : SoulLikeCamera");
        return nullptr;
    }

    return instance;
}

void SoulLikeCamera::Free()
{
    Camera::Free();
}

NS_END

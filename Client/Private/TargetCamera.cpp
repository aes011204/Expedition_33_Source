#include "TargetCamera.h"
#include "CameraPresetIO.h"
#include "ClientInstance.h"
#include "GameInstance.h"
#include "Collision_Define.h"
#include "PhysX_Types.h"
#include "CameraPresetTypes.h"
#include "ClientScreenFxManager.h"

namespace
{
float Evaluate_TargetFollowT(BattleCameraTransitionEasing easing, float t)
{
    t = clamp(t, 0.f, 1.f);

    switch (easing)
    {
    case BattleCameraTransitionEasing::Linear:
        return t;

    case BattleCameraTransitionEasing::EaseIn:
        return t * t;

    case BattleCameraTransitionEasing::EaseOut:
        return 1.f - (1.f - t) * (1.f - t);

    case BattleCameraTransitionEasing::EaseInOut:
        return t * t * (3.f - 2.f * t);

    default:
        return t;
    }
}
}

TargetCamera::TargetCamera(const ComPtr<Device>& device, const ComPtr<Context>& context) : Camera{ device, context }
{
}

TargetCamera::TargetCamera(const TargetCamera& prototype) : Camera{ prototype }
{
}

HRESULT TargetCamera::Initialize_Prototype()
{
    if (FAILED(__super::Initialize_Prototype()))
        return E_FAIL;

    return S_OK;
}

HRESULT TargetCamera::Initialize(void* arg)
{
    if (nullptr == arg)
        return E_FAIL;

    const auto desc = static_cast<TARGETCAMERA_DESC*>(arg);

    _currentSettings.followSpeed = desc->followSpeed;
    _currentSettings.targetFollowSpeed = desc->targetFollowSpeed;
    _currentSettings.verticalFollowSpeed = desc->verticalFollowSpeed;
    _currentSettings.camera.mouseSensor = desc->mouseSensor;
    _currentSettings.distance = desc->distance;
    _currentSettings.sideOffset = desc->sideOffset;
    _currentSettings.heightOffset = desc->heightOffset;
    _currentSettings.aimOffset = desc->aimOffset;
    _currentSettings.pitchLookScale = desc->pitchLookScale;
    _currentSettings.pitchMin = desc->pitchMin;
    _currentSettings.pitchMax = desc->pitchMax;
    _currentSettings.lookHeight = desc->lookHeight;
    _currentSettings.pivotHeight = desc->pivotHeight;
    _currentSettings.camera.mouseSensitivity = desc->mouseSensitivity;
    _currentSettings.yawOffset = desc->yawOffset;
    _currentSettings.pitchOffset = desc->pitchOffset;
    _currentSettings.rollOffset = desc->rollOffset;

    _yaw = 0.f;
    _pitch = 0.f;

    if (FAILED(__super::Initialize(arg)))
        return E_FAIL;

    Update_TransformMatrices();

    Set_Name(L"Target Camera");

    CameraPresetIO::LoadTargetPresetFromFile(TargetCameraMode::NORMAL, _normalPreset);
    CameraPresetIO::LoadTargetPresetFromFile(TargetCameraMode::AIM, _aimPreset);
    CameraPresetIO::LoadTargetPresetFromFile(TargetCameraMode::BATTLE_ENTER, _battleEnterPreset);

    _currentState = TargetCameraMode::NORMAL;
    _currentSettings = _normalPreset;
    _targetSettings = _currentSettings;

    _collisionQueryAccum = 0.f;
    _hasCollisionQueryCache = false;
    _lastCollisionSweepStart = Vec3::Zero;
    _lastCollisionDesiredPos = Vec3::Zero;
    _lastCollisionResolvedPos = Vec3::Zero;

    return S_OK;
}

void TargetCamera::BeginPlay()
{
    Camera::BeginPlay();

    GAME->Add_Camera(GetSharedPtr<Camera>());
    CLIENT->Request_GamePlay();

    Refresh_TargetTransform();

    _prevTargetPos = _targetTransformCom.lock()->Get_WorldPosition();
    _smoothedTargetPos = _prevTargetPos;
}

void TargetCamera::OnActivated()
{
    Refresh_TargetTransform();

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

void TargetCamera::Priority_Update(float timeDelta)
{
    __super::Priority_Update(timeDelta);

    if (GAME->Is_WorldGameplayInputBlocked())
    {
        if (_currentState == TargetCameraMode::AIM && !CLIENT->Is_TargetCameraEditMode())
            Change_Mode(TargetCameraMode::NORMAL);

        return;
    }

    if (GAME->KeyDown(KEY_TYPE::RBUTTON))
    {
        Change_Mode(TargetCameraMode::AIM);
    }

    if (_currentState == TargetCameraMode::AIM && GAME->KeyUp(KEY_TYPE::RBUTTON))
    {
        Change_Mode(TargetCameraMode::NORMAL);
    }
}

void TargetCamera::Update(float timeDelta)
{
    __super::Update(timeDelta);

    if (!GAME->Is_ActiveCamera(GetSharedPtr<Camera>()))
        return;

    if (GAME->Is_ActiveCamera(GetSharedPtr<Camera>()) &&
        _targetSettings.allowMouseRotation &&
        !GAME->Is_WorldGameplayInputBlocked())
    {
        const Vec2 mouseDelta = GAME->Get_MouseDelta();

        _yaw += mouseDelta.x * _currentSettings.camera.mouseSensor;
        _pitch += mouseDelta.y * _currentSettings.camera.mouseSensor * _currentSettings.camera.mouseSensitivity;
        _pitch = std::clamp(_pitch, _currentSettings.pitchMin, _currentSettings.pitchMax);

    }

    Interpolate_CurrentSettings(timeDelta);

    switch (_currentState)
    {
    case TargetCameraMode::NORMAL:
        Update_NormalState(timeDelta);
        break;

    case TargetCameraMode::AIM:
        Update_AimState(timeDelta);
        break;

    case TargetCameraMode::BATTLE_ENTER:
        Update_BattleEnterState(timeDelta);
        break;
    }


}

void TargetCamera::Late_Update(float timeDelta)
{
    __super::Late_Update(timeDelta);

}

HRESULT TargetCamera::Render()
{
    if (FAILED(__super::Render()))
        return E_FAIL;

    return S_OK;
}

void TargetCamera::Interpolate_CurrentSettings(float timeDelta)
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

bool TargetCamera::Try_GetTargetTransform(Shared<TransformCom>& outTargetTransformCom) const
{
    if (_targetTransformCom.expired())
    {
        MSG_BOX("Target Transform == nullptr");
        return false;
    }

    outTargetTransformCom = _targetTransformCom.lock();
    return outTargetTransformCom != nullptr;
}

void TargetCamera::Update_SmoothedTargetPosition(const Vec3& targetPos, float timeDelta)
{
    constexpr float kTargetStillXZFrameDeltaSq = 0.000025f;
    constexpr float kHorizontalSettleDistanceSq = 0.0004f;
    constexpr float kTargetStillYFrameDelta = 0.005f;
    constexpr float kVerticalSettleDistance = 0.035f;

    const Vec3 targetDelta = targetPos - _prevTargetPos;
    const float horizontalFollowT = Evaluate_TargetFollowT(
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

void TargetCamera::Update_FromCurrentSettings(float timeDelta)
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
    Vec3 framingPos = pivotPos + rightDir * _currentSettings.aimOffset;
    Vec3 desiredPos = framingPos + orbitOffset + Vec3::Up * _currentSettings.heightOffset + rightDir * _currentSettings.sideOffset;
    Vec3 lookTarget = framingPos + Vec3::Up * _currentSettings.lookHeight;

    _currentLookTarget = lookTarget;

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

void TargetCamera::Refresh_TargetTransform()
{
    auto player = CLIENT->Find_Player(GAME->Current_LevelIndex());
    if (player && player->Get_Transform())
        _targetTransformCom = player->Get_Transform();
}

Vec3 TargetCamera::Get_FreeAimQueryOrigin() const
{
    CHECK_NULL(_transformCom, Vec3::Zero);

    return _transformCom->Get_WorldPosition();
}

Vec3 TargetCamera::Get_FreeAimQueryDirection() const
{
    CHECK_NULL(_transformCom, Vec3::Look);

    Vec3 direction = _transformCom->Get_WorldForward();

    if (direction.LengthSquared() <= 0.0001f)
        return Vec3::Look;

    direction.Normalize();
    return direction;
}

void TargetCamera::Update_NormalState(float timeDelta)
{
    Update_FromCurrentSettings(timeDelta);
}

void TargetCamera::Update_AimState(float timeDelta)
{
    Update_FromCurrentSettings(timeDelta);
}

void TargetCamera::Update_BattleEnterState(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    float unscaledDelta = GAME->Get_UnscaledTimeDelta();
    _battleEnterAcc += unscaledDelta;

    if (_battleEnterAcc >= _battleEnterDuration)
    {
        Change_Mode(TargetCameraMode::NORMAL);
        _battleEnterAcc = 0.f;
        return;
    }

    Update_FromCurrentSettings(unscaledDelta);
}

Vec3 TargetCamera::Resolve_Collision(const Vec3& sweepStart, const Vec3& desiredPos, float timeDelta)
{
    if (!_currentSettings.doCollisionTest)
        return desiredPos;

    // 카메라가 있어야할 뒤쪽 위치 - 플레이어 / 타겟 pivot
    Vec3 sweep = desiredPos - sweepStart;
    const float desiredDistance = sweep.Length();

    if (desiredDistance <= 0.0001f)
        return desiredPos;

    sweep /= desiredDistance;

    float targetDistance = desiredDistance;

    // 카메라 기준점에서 목표 위치까지 sphere sweep을 쏴서 벽/지형 가림 여부 확인
    PhysXQueryHit hit{};

    // 막혔는지 판단
    const bool blocked = GAME->Sweep_Sphere(
        sweepStart, sweep, desiredDistance, _currentSettings.collisionProbeRadius, Collision_Preset::Camera, hit);

    if (blocked)
    {
        targetDistance = max(0.f, hit.distance - _currentSettings.collisionSkin);
    }

    if (_collisionDistance < 0.f)
    {
        _collisionDistance = targetDistance;
    }

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

void TargetCamera::Change_Mode(TargetCameraMode mode)
{
    if (_currentState == mode)
        return;

    _currentState = mode;
    _collisionQueryAccum = 0.f;
    _hasCollisionQueryCache = false;
    _collisionDistance = -1.f;

    switch (_currentState)
    {
    case TargetCameraMode::NORMAL:
        _targetSettings = _normalPreset;
        _currentSettings.followSpeed = _normalPreset.followSpeed;
        break;

    case TargetCameraMode::AIM:
        _targetSettings = _aimPreset;
        _currentSettings.followSpeed = _aimPreset.followSpeed;
        break;

    case TargetCameraMode::BATTLE_ENTER:
        _targetSettings = _battleEnterPreset;
        _currentSettings.followSpeed = _battleEnterPreset.followSpeed;
        break;
    }

    if (_targetSettings.snapOnEnter)
    {
        _currentSettings = _targetSettings;

        Shared<TransformCom> targetTransformCom;
        if (Try_GetTargetTransform(targetTransformCom))
        {
            const Vec3 targetPos = targetTransformCom->Get_WorldPosition();
            _prevTargetPos = targetPos;
            _smoothedTargetPos = targetPos;
        }
    }
}

void TargetCamera::Change_Target()
{

}

bool TargetCamera::Try_GetFlatMoveBasis(Vec3& outForward, Vec3& outRight) const
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

void TargetCamera::SetTargetTransform(Shared<TransformCom> targetTransform)
{
    _targetTransformCom = targetTransform;

    // 전환 직후 이전 target 위치가 남아 있으면 카메라 보간이 튈 수 있어서
    if (targetTransform)
        _prevTargetPos = targetTransform->Get_WorldPosition();

    _collisionQueryAccum = 0.f;
    _hasCollisionQueryCache = false;
    _collisionDistance = -1.f;
}

const TargetCameraPreset& TargetCamera::Get_Preset(TargetCameraMode mode) const
{
    switch (mode)
    {
    case TargetCameraMode::NORMAL:
        return _normalPreset;

    case TargetCameraMode::AIM:
        return _aimPreset;

    case TargetCameraMode::BATTLE_ENTER:
        return _battleEnterPreset;
    }

    LOG_DEBUG("Unknown Camera State");
    return _normalPreset;
}

void TargetCamera::Set_Preset(TargetCameraMode mode, const TargetCameraPreset& preset)
{
    switch (mode)
    {
    case TargetCameraMode::NORMAL:
        _normalPreset = preset;
        break;

    case TargetCameraMode::AIM:
        _aimPreset = preset;
        break;

    case TargetCameraMode::BATTLE_ENTER:
        _battleEnterPreset = preset;

    default:
        LOG_DEBUG("Unknown Camera State");
        break;
    }
}

void TargetCamera::Apply_PreviewPreset(const TargetCameraPreset& preset)
{
    _currentSettings = preset;
    _targetSettings = preset;
    Set_FovY(_currentSettings.camera.fovy);
    Set_Near(_currentSettings.camera.nearPlane);
    Set_Far(_currentSettings.camera.farPlane);
}

Shared<TargetCamera> TargetCamera::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<TargetCamera>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Create : TargetCamera");
        return nullptr;
    }
    return instance;
}

Shared<GameObject> TargetCamera::Clone(void* arg)
{
    auto instance = make_shared<TargetCamera>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        MSG_BOX("Failed to Clone : TargetCamera");
        return nullptr;
    }
    return instance;
}

void TargetCamera::Free()
{
    __super::Free();
}

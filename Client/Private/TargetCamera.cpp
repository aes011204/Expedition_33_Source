#include "TargetCamera.h"
#include "CameraPresetIO.h"
#include "ClientInstance.h"
#include "GameInstance.h"

TargetCamera::TargetCamera(const ComPtr<Device>& device, const ComPtr<Context>& context) :  Camera {device, context}
{
}

TargetCamera::TargetCamera(const TargetCamera& prototype) : Camera {prototype}
{
}

HRESULT TargetCamera::Initialize_Prototype()
{
    if(FAILED(__super::Initialize_Prototype()))
        return E_FAIL;

    return S_OK;
}

HRESULT TargetCamera::Initialize(void* arg)
{
    if (nullptr == arg)
        return E_FAIL;

    const auto desc = static_cast<TARGETCAMERA_DESC*>(arg);

    _currentSettings.followSpeed = desc->followSpeed;
    _currentSettings.mouseSensor = desc->mouseSensor;
    _currentSettings.distance = desc->distance;
    _currentSettings.sideOffset = desc->sideOffset;
    _currentSettings.heightOffset = desc->heightOffset;
    _currentSettings.aimOffset = desc->aimOffset;
    _currentSettings.pitchLookScale = desc->pitchLookScale;
    _currentSettings.pitchMin = desc->pitchMin;
    _currentSettings.pitchMax = desc->pitchMax;
    _currentSettings.lookHeight = desc->lookHeight;
    _currentSettings.pivotHeight = desc->pivotHeight;
    _currentSettings.mouseSensitivity = desc->mouseSensitivity;
    _currentSettings.yawOffset = desc->yawOffset;
    _currentSettings.pitchOffset = desc->pivotHeight;
    _currentSettings.rollOffset = desc->rollOffset;
    
    _yaw = 0.f;
    _pitch = 0.f;

    if(FAILED(__super::Initialize(arg)))
        return E_FAIL;

    Update_TransformMatrices();

    Set_Name(L"Target Camera");

    CameraPresetIO::LoadPresetFromFile(CAMERA_STATE::NORMAL, _normalPreset);
    CameraPresetIO::LoadPresetFromFile(CAMERA_STATE::AIM, _aimPreset);
    CameraPresetIO::LoadPresetFromFile(CAMERA_STATE::BATTLE_ENTER, _battleEnterPreset);

    _currentState = CAMERA_STATE::NORMAL;
    _currentSettings = _normalPreset;
    _targetSettings = _currentSettings;
    return S_OK;
}

void TargetCamera::BeginPlay()
{
    Camera::BeginPlay();

    GAME->Add_Camera(GetSharedPtr<Camera>());
    GAME->Change_ActiveCamera(GetSharedPtr<Camera>());
    _targetTransformCom =  CLIENT->Find_Player(GAME->Current_LevelIndex())->Get_Transform();
    _prevTargetPos = _targetTransformCom.lock()->Get_WorldPosition();
    _smoothedTargetPos = _prevTargetPos;
}

void TargetCamera::Priority_Update(float timeDelta)
{
    __super::Priority_Update(timeDelta);

    if(GAME->KeyDown(KEY_TYPE::RBUTTON))
    {
        Change_State(CAMERA_STATE::AIM);
    }

    if(_currentState == CAMERA_STATE::AIM && GAME->KeyUp(KEY_TYPE::RBUTTON))
    {
        Change_State(CAMERA_STATE::NORMAL);
    }

    if(GAME->KeyDown(KEY_TYPE::CTRL))
    {
        Change_State(CAMERA_STATE::BATTLE_ENTER);
    }

}

void TargetCamera::Update(float timeDelta)
{
    __super::Update(timeDelta);

    if(!GAME->Is_ActiveCamera(GetSharedPtr<Camera>()))
         return;

    if(GAME->Is_ActiveCamera(GetSharedPtr<Camera>())&&_targetSettings.allowMouseRotation)
    {
        const Vec2 mouseDelta = GAME->Get_MouseDelta();

        _yaw += mouseDelta.x * _currentSettings.mouseSensor;
        _pitch += mouseDelta.y * _currentSettings.mouseSensor * _currentSettings.mouseSensitivity;
        _pitch = std::clamp(_pitch, _currentSettings.pitchMin, _currentSettings.pitchMax);
     
    }

    Interpolate_CurrentSettings(timeDelta);

    switch(_currentState)
    {
    case CAMERA_STATE::NORMAL:
        Update_NormalState(timeDelta);
        break;

    case CAMERA_STATE::AIM:
        Update_AimState(timeDelta);
        break;

    case CAMERA_STATE::BATTLE_ENTER:
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
   if(FAILED(__super::Render()))
       return E_FAIL;

    return S_OK;
}

void TargetCamera::Interpolate_CurrentSettings(float timeDelta)
{
    const float t = clamp(timeDelta * _targetSettings.followSpeed,0.f,1.f );

    _currentSettings.aimOffset = lerp(_currentSettings.aimOffset,_targetSettings.aimOffset,t);
    _currentSettings.distance = lerp(_currentSettings.distance,_targetSettings.distance,t);
    _currentSettings.heightOffset = lerp(_currentSettings.heightOffset,_targetSettings.heightOffset,t);
    _currentSettings.lookHeight = lerp(_currentSettings.lookHeight,_targetSettings.lookHeight,t);
    _currentSettings.pitchLookScale = lerp(_currentSettings.pitchLookScale,_targetSettings.pitchLookScale,t);
    _currentSettings.pivotHeight = lerp(_currentSettings.pivotHeight,_targetSettings.pivotHeight,t);
    _currentSettings.sideOffset = lerp(_currentSettings.sideOffset,_targetSettings.sideOffset,t);
    _currentSettings.pitchOffset = lerp(_currentSettings.pitchOffset, _targetSettings.pitchOffset, t);
    _currentSettings.yawOffset = lerp(_currentSettings.yawOffset,_targetSettings.yawOffset,t);
    _currentSettings.rollOffset = lerp(_currentSettings.rollOffset,_targetSettings.rollOffset,t);
    _currentSettings.fovy = lerp(_currentSettings.fovy,_targetSettings.fovy,t);
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

void TargetCamera::Update_FromCurrentSettings(float timeDelta)
{
    Shared<TransformCom> targetTransformCom;
    if (!Try_GetTargetTransform(targetTransformCom))
        return;

    const Vec3 targetPos = targetTransformCom->Get_WorldPosition();

    const bool isTargetMoving = (targetPos - _prevTargetPos).LengthSquared() > 0.0001f;
    float followSpeed = isTargetMoving ? _currentSettings.followSpeed : _currentSettings.followSpeed * 0.5f;

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

    //Vec3 delta = targetPos - _smoothedTargetPos;
    //
    //if(delta.LengthSquared() < 0.0004f)
    //  _smoothedTargetPos = targetPos;
    //
    //_smoothedTargetPos = Math::Lerp_Damp(_smoothedTargetPos, targetPos, followSpeed, timeDelta);

    _smoothedTargetPos = targetPos;

    const Vec3 pivotPos = _smoothedTargetPos + Vec3::Up * _currentSettings.pivotHeight;
    Vec3 framingPos = pivotPos  + rightDir * _currentSettings.aimOffset;
    Vec3 desiredPos = framingPos + orbitOffset + Vec3::Up * _currentSettings.heightOffset + rightDir * _currentSettings.sideOffset;
    Vec3 lookTarget = framingPos + Vec3::Up * _currentSettings.lookHeight;

    Vec3 newPos = desiredPos;
    
    _transformCom->Set_WorldPosition(newPos);
    _transformCom->LookAt(lookTarget);
    _transformCom->Add_LocalRotation(Quat::CreateFromAxisAngle(Vec3::Forward, _currentSettings.rollOffset));

    Update_TransformMatrices();
    GAME->UpdateFrustum(*GAME->Get_Transform(D3DTS::View), *GAME->Get_Transform(D3DTS::Proj));

    _prevTargetPos = targetPos;

    Vec3 pos = _transformCom->Get_WorldPosition();
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
    _battleEnterAcc += timeDelta;

    if(_battleEnterAcc >= _battleEnterDuration)
    {
        //TODO 유림 : 배틀 집입 연출 끝나면 배틀 카메라로! 일단 지금은 다시 normal 카메라로 돌리기
        Change_State(CAMERA_STATE::NORMAL);
        _battleEnterAcc = 0.f;
        return;
    }

    Update_FromCurrentSettings(timeDelta);
}

void TargetCamera::Change_State(CAMERA_STATE state)
{
    if(_currentState == state)
        return;

    _currentState = state;

    switch(_currentState)
    {
    case CAMERA_STATE::NORMAL:
        _targetSettings = _normalPreset;
        _currentSettings.followSpeed = _normalPreset.followSpeed;
        break;

    case CAMERA_STATE::AIM:
        _targetSettings = _aimPreset;
        _currentSettings.followSpeed = _aimPreset.followSpeed;
        break;

    case CAMERA_STATE::BATTLE_ENTER:
        _targetSettings = _battleEnterPreset;
        _currentSettings.followSpeed = _battleEnterPreset.followSpeed;
        break;
    }
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
}

const TargetCamera::TARGETCAMERA_DESC& TargetCamera::Get_Preset(CAMERA_STATE state) const
{
    switch (state)
    {
    case CAMERA_STATE::NORMAL:
        return _normalPreset;

    case CAMERA_STATE::AIM:
        return _aimPreset;

    case CAMERA_STATE::BATTLE_ENTER:
        return _battleEnterPreset;
    }

    LOG_DEBUG("Unknown Camera State");
    return _normalPreset;
}

void TargetCamera::Set_Preset(CAMERA_STATE state, const TARGETCAMERA_DESC& preset)
{
    switch (state)
    {
    case CAMERA_STATE::NORMAL:
         _normalPreset = preset;
        break;

    case CAMERA_STATE::AIM:
        _aimPreset = preset;
        break;

    case CAMERA_STATE::BATTLE_ENTER:
        _battleEnterPreset = preset;

    default:
        LOG_DEBUG("Unknown Camera State");
        break;
    }
}

void TargetCamera::Apply_PreviewPreset(const TARGETCAMERA_DESC& preset)
{
    _currentSettings = preset;
    _targetSettings = preset;
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

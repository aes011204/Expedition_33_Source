#include "TargetCamera.h"
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

    _followSpeed = desc->followSpeed;
    _offset = desc->offset;
    _mouseSensor = desc->mouseSensor;

    if(FAILED(__super::Initialize(arg)))
        return E_FAIL;

    Update_TransformMatrices();

    Set_Name(L"Target Camera");

    return S_OK;
}

void TargetCamera::Priority_Update(float timeDelta)
{
    __super::Priority_Update(timeDelta);

    if(_firstUpdate)
    {
        GAME->Add_Camera(GetSharedPtr<Camera>());

        _targetTransformCom =  CLIENT->Find_Player(GAME->Current_LevelIndex())->Get_Transform();

        _firstUpdate = false;
    }

    if(!GAME->Is_ActiveCamera(GetSharedPtr<Camera>()))
        return;

    if(_targetTransformCom.expired())
    {
        MSG_BOX("Target Transform == nullptr");
        return;
    }

   auto targetTransformCom  = _targetTransformCom.lock();

    if(nullptr == targetTransformCom)
        return;

    Vec3 targetPos = targetTransformCom->Get_WorldPosition();

    Vec3 desiredPos = { targetPos.x + _offset.x , targetPos.y + _offset.y, targetPos.z + _offset.z };

    Vec3 currentPos = _transformCom->Get_WorldPosition();

    Vec3 newPos =  Math::Lerp_Damp(currentPos, desiredPos, _followSpeed, timeDelta);

    const Vec2 mouseDelta = GAME->Get_MouseDelta();

    _yaw += mouseDelta.x * _mouseSensor;
    _pitch += mouseDelta.y * _mouseSensor;

    if(0.f != mouseDelta.x || 0.f != mouseDelta.y)
        _transformCom->Set_RotationEuler(Vec3(_pitch, _yaw, 0.f));

    _transformCom->Set_WorldPosition(newPos);

    Update_TransformMatrices();

    Vec3 pos =  _transformCom->Get_WorldPosition();
    Vec3 look = _transformCom->Get_WorldForward();

    if(GAME->KeyDown(KEY_TYPE::F11))
    {
        LOG_DEBUG("Target Camera Settings : yaw = {}, pitch = {}, x = {}, y = {}, z = {}, look.x = {}, look.y = {}, look.z = {}", _yaw, _pitch, pos.x, pos.y, pos.z, look.x, look.y, look.z);
    }
}

void TargetCamera::Update(float timeDelta)
{
    __super::Update(timeDelta);
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

void TargetCamera::SetTargetTransform(Shared<TransformCom> targetTransform)
{
    //타겟을 설정해줘야함

    _targetTransformCom = targetTransform;
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

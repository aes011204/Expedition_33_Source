#include "FreeCamera.h"
#include "GameInstance.h"

FreeCamera::FreeCamera(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Camera{ device, context }
{
}

FreeCamera::FreeCamera(const FreeCamera& prototype)
    : Camera{ prototype }
{
}

void FreeCamera::BeginPlay()
{
    Camera::BeginPlay();
    GAME->Add_Camera(GetSharedPtr<Camera>());
    GAME->Change_ActiveCamera(GetSharedPtr<Camera>());
}

HRESULT FreeCamera::Initialize_Prototype()
{
    return S_OK;
}

HRESULT FreeCamera::Initialize(void* arg)
{
    const auto desc = static_cast<FREECAMERA_DESC*>(arg);

    _mouseSensor = desc->mouseSensor;

    if (FAILED(__super::Initialize(arg)))
        return E_FAIL;

    Set_Name(L"Free Camera");

    return S_OK;
}

void FreeCamera::Priority_Update(float timeDelta)
{
    __super::Priority_Update(timeDelta);

    if(!GAME->Is_ActiveCamera(GetSharedPtr<Camera>()))
        return;

    if(GAME->KeyDown(KEY_TYPE::F1))
    {
        _lock = !_lock;
    }

    if(_lock)
        return;

    if (GAME->KeyPress(KEY_TYPE::W))
        _transformCom->Go_Straight(timeDelta);

    if (GAME->KeyPress(KEY_TYPE::S))
        _transformCom->Go_Backward(timeDelta);

    if (GAME->KeyPress(KEY_TYPE::A))
        _transformCom->Go_Left(timeDelta);

    if (GAME->KeyPress(KEY_TYPE::D))
        _transformCom->Go_Right(timeDelta);

    if (GAME->KeyPress(KEY_TYPE::E))
        _transformCom->Go_Up(timeDelta);

    if (GAME->KeyPress(KEY_TYPE::Q))
        _transformCom->Go_Down(timeDelta);

    const Vec2 mouseDelta = GAME->Get_MouseDelta();

    _yaw += mouseDelta.x * _mouseSensor;
    _pitch += mouseDelta.y * _mouseSensor;

    if (0.f != mouseDelta.x || 0.f != mouseDelta.y )
             _transformCom->Set_RotationEuler(Vec3(_pitch, _yaw, 0.f));

    __super::Update_TransformMatrices();

    Vec3 pos = _transformCom->Get_WorldPosition();
    Vec3 look = _transformCom->Get_WorldForward();

    if(GAME->KeyDown(KEY_TYPE::F11))
    {
        LOG_DEBUG("Target Camera Settings : yaw = {}, pitch = {}, x = {}, y = {}, z = {}, look.x = {}, look.y = {}, look.z = {}", _yaw, _pitch, pos.x, pos.y, pos.z, look.x, look.y, look.z);
    }
}

void FreeCamera::Update(float timeDelta)
{
}

void FreeCamera::Late_Update(float timeDelta)
{
}

//void FreeCamera::ApplyPreset(const FreeCameraPreset& preset)
//{
//    _transformCom->Set_WorldPosition(preset.position);
//    _transformCom->Set_RotationEuler(preset.rotation);
//    _transformCom->Set_SpeedPerSec(preset.moveSpeed);
//
//    _mouseSensor = preset.mouseSensor;
//    _fovy = preset.fovy;
//    _near = preset.nearPlane;
//    _far = preset.farPlane;
//
//    Update_TransformMatrices();
//}

HRESULT FreeCamera::Render()
{
    return S_OK;
}

Shared<FreeCamera> FreeCamera::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<FreeCamera>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Create : FreeCamera");
        return nullptr;
    }
    return instance;
}

Shared<GameObject> FreeCamera::Clone(void* arg)
{
    auto instance = make_shared<FreeCamera>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        MSG_BOX("Failed to Clone : FreeCamera");
        return nullptr;
    }
    return instance;
}

void FreeCamera::Free()
{
    __super::Free();
}

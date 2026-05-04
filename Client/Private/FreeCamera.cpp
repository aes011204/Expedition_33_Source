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

    return S_OK;
}

void FreeCamera::Priority_Update(float timeDelta)
{
    __super::Priority_Update(timeDelta);

    if(_firstUpdate)
    {
        GAME->Add_Camera(GetSharedPtr<Camera>());
        // 이거 여기에서 호출하면, GameLauncher에서도, Free Camera로 시작돼서, Target Camera 쪽으로 변경합니다
        //GAME->Change_ActiveCamera(GetSharedPtr<Camera>());

        _firstUpdate = false;
    }

    if(!GAME->Is_ActiveCamera(GetSharedPtr<Camera>()))
        return;

    // 활성 자유카메라는 입력이 없어도 현재 transform 기준으로 view matrix를 바로 반영해야,
    // F8 전환이나 Scene 더블클릭 포커스 이동이 그 프레임에 즉시 보인다.
    __super::Update_TransformMatrices();

    // 우클릭 눌렀을 때 움직일 수 있게
    if (!GAME->Is_FreeCameraInputEnabled())
        return;

    if (!GAME->KeyPress(KEY_TYPE::RBUTTON))
        return;

    //TODO movespeed 만들고 그거 기준으로 이동할 수 있도록

    //if(GAME->KeyDown(KEY_TYPE::F1))
    //{
    //    _lock = !_lock;
    //}
    //
    //if(_lock)
    //    return;

    const float cameraDelta = max(timeDelta, GAME->Get_FreeCameraTimeDelta());

    if (GAME->KeyPress(KEY_TYPE::W))
        _transformCom->Go_Straight(cameraDelta);

    if (GAME->KeyPress(KEY_TYPE::S))
        _transformCom->Go_Backward(cameraDelta);

    if (GAME->KeyPress(KEY_TYPE::A))
        _transformCom->Go_Left(cameraDelta);

    if (GAME->KeyPress(KEY_TYPE::D))
        _transformCom->Go_Right(cameraDelta);

    if (GAME->KeyPress(KEY_TYPE::E))
        _transformCom->Go_Up(cameraDelta);

    if (GAME->KeyPress(KEY_TYPE::Q))
        _transformCom->Go_Down(cameraDelta);

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

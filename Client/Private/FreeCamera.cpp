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

    if (!GAME->Is_ActiveCamera(GetSharedPtr<Camera>()))
        return;

    const bool canControl =
        !GAME->Is_WorldGameplayInputBlocked() &&
        GAME->Is_FreeCameraInputEnabled() &&
        GAME->KeyPress(KEY_TYPE::RBUTTON);

    const float wheelDelta = GAME->Get_MouseWheel();

    if (canControl)
    {
        if (GAME->KeyPress(KEY_TYPE::CTRL) && wheelDelta != 0.f)
        {
            _moveSpeed = static_cast<int>(_transformCom->Get_SpeedPerSec());

            const int step = 2;

            if (wheelDelta > 0.f)
                _moveSpeed += step;
            else
                _moveSpeed -= step;

            _moveSpeed = clamp(_moveSpeed, 0, 100);

            _transformCom->Set_SpeedPerSec(static_cast<float>(_moveSpeed));
        }

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

        if (0.f != mouseDelta.x || 0.f != mouseDelta.y)
            _transformCom->Set_RotationEuler(Vec3(_pitch, _yaw, 0.f));
    }
    __super::Update_TransformMatrices();
    GAME->UpdateFrustum(*GAME->Get_Transform(D3DTS::View), *GAME->Get_Transform(D3DTS::Proj));
}

void FreeCamera::Update(float timeDelta)
{
}

void FreeCamera::Late_Update(float timeDelta)
{
}

HRESULT FreeCamera::Render()
{
    return S_OK;
}

void FreeCamera::Update_TransformMatrices()
{
    return __super::Update_TransformMatrices();
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

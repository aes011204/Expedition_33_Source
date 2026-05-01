#include "FreeCamera.h"
#include "GameInstance.h"

FreeCamera::FreeCamera(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Camera{ device, context }
{
}

FreeCamera::FreeCamera(const FreeCamera& prototype)
    : Camera{ prototype }
    , _mouseSensor{ prototype._mouseSensor }
{
}

HRESULT FreeCamera::Initialize_Prototype()
{
    return S_OK;
}

HRESULT FreeCamera::Initialize(void* arg)
{
    const auto pDesc = static_cast<FREECAMERA_DESC*>(arg);
    _mouseSensor = pDesc->mouseSensor;

    if (FAILED(__super::Initialize(arg)))
        return E_FAIL;

    return S_OK;
}

void FreeCamera::Priority_Update(float timeDelta)
{

    Matrix before = _transformCom->Get_WorldMatrix();

    if (GAME->KeyPress(KEY_TYPE::W))
        _transformCom->Go_Straight(timeDelta);

    if (GAME->KeyPress(KEY_TYPE::S))
        _transformCom->Go_Backward(timeDelta);

    if (GAME->KeyPress(KEY_TYPE::A))
        _transformCom->Go_Left(timeDelta);

    if (GAME->KeyPress(KEY_TYPE::D))
        _transformCom->Go_Right(timeDelta);

    Matrix after = _transformCom->Get_WorldMatrix();

    const Vec2 mouseDelta = GAME->Get_MouseDelta();

    if (0.f != mouseDelta.x)
        _transformCom->Turn(Vec3::Up, timeDelta * mouseDelta.x * _mouseSensor);

    if (0.f != mouseDelta.y)
        _transformCom->Turn(_transformCom->Get_State(State::Right), timeDelta * mouseDelta.y * _mouseSensor);

    __super::Update_TransformMatrices();
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

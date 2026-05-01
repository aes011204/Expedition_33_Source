#include "FreeCamera.h"
#include "GameInstance.h"

FreeCamera::FreeCamera(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Camera{ device, context }
{
}

FreeCamera::FreeCamera(const FreeCamera& prototype)
    : Camera{ prototype }
    , m_fMouseSensor{ prototype.m_fMouseSensor }
{
}

HRESULT FreeCamera::Initialize_Prototype()
{
    return S_OK;
}

HRESULT FreeCamera::Initialize(void* pArg)
{
    const auto pDesc = static_cast<FREECAMERA_DESC*>(pArg);
    m_fMouseSensor = pDesc->fMouseSensor;

    if (FAILED(__super::Initialize(pArg)))
        return E_FAIL;

    return S_OK;
}

void FreeCamera::Priority_Update(float fTimeDelta)
{
    if (GAME->KeyPress(KEY_TYPE::W))
        _transformCom->Go_Straight(fTimeDelta);

    if (GAME->KeyPress(KEY_TYPE::S))
        _transformCom->Go_Backward(fTimeDelta);

    if (GAME->KeyPress(KEY_TYPE::A))
        _transformCom->Go_Left(fTimeDelta);

    if (GAME->KeyPress(KEY_TYPE::D))
        _transformCom->Go_Right(fTimeDelta);

    const Vec2 vMouseDelta = GAME->Get_MouseDelta();

    if (0.f != vMouseDelta.x)
        _transformCom->Turn(Vec3::Up, fTimeDelta * vMouseDelta.x * m_fMouseSensor);

    if (0.f != vMouseDelta.y)
        _transformCom->Turn(_transformCom->Get_State(State::Right), fTimeDelta * vMouseDelta.y * m_fMouseSensor);

    __super::Update_TransformMatrices();
}

void FreeCamera::Update(float fTimeDelta)
{
}

void FreeCamera::Late_Update(float fTimeDelta)
{
}

HRESULT FreeCamera::Render()
{
    return S_OK;
}

Shared<FreeCamera> FreeCamera::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto pInstance = make_shared<FreeCamera>(device, context);

    if (FAILED(pInstance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : FreeCamera");
        return nullptr;
    }
    return pInstance;
}

Shared<GameObject> FreeCamera::Clone(void* pArg)
{
    auto pInstance = make_shared<FreeCamera>(*this);

    if (FAILED(pInstance->Initialize(pArg)))
    {
        MSG_BOX("Failed to Cloned : FreeCamera");
        return nullptr;
    }
    return pInstance;
}

void FreeCamera::Free()
{
    __super::Free();
}

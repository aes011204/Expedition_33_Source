#include "CinematicCamera.h"
#include "GameInstance.h"

CinematicCamera::CinematicCamera(const ComPtr<Device>& device, const ComPtr<Context>& context) : Camera { device, context}
{
}

CinematicCamera::CinematicCamera(const CinematicCamera& prototype) : Camera { prototype }
{
}

void CinematicCamera::BeginPlay()
{
    Camera::BeginPlay();
}

HRESULT CinematicCamera::Initialize_Prototype()
{
    return S_OK;
}

HRESULT CinematicCamera::Initialize(void* arg)
{
    const auto desc = static_cast<CINEMATICCAMERA_DESC*>(arg);

    if (FAILED(__super::Initialize(arg)))
        return E_FAIL;

    Set_Name(L"Cinematic Camera");

    return S_OK;
}

void CinematicCamera::OnActivated()
{
    __super::OnActivated();
}

void CinematicCamera::Priority_Update(float timeDelta)
{
    __super::Priority_Update(timeDelta);
}

void CinematicCamera::Update(float timeDelta)
{
    __super::Update(timeDelta);
}

void CinematicCamera::Late_Update(float timeDelta)
{
    __super::Late_Update(timeDelta);
}

HRESULT CinematicCamera::Render()
{
    return S_OK;
}

void CinematicCamera::Apply_CinematicPose(const CAMERAPOSE& camPose)
{
    if(_transformCom == nullptr)
        return;

    _transformCom->Set_Position(camPose.position);
    _transformCom->Set_RotationEuler(camPose.rotation);

    Set_FovY(camPose.fovY);
    Set_Near(camPose.nearPlane);
    Set_Far(camPose.farPlane);

    if (GAME->Is_ActiveCamera(GetSharedPtr<Camera>()))
    {
        Update_TransformMatrices();
        GAME->UpdateFrustum(*GAME->Get_Transform(D3DTS::View), *GAME->Get_Transform(D3DTS::Proj));
    }
}

Shared<CinematicCamera> CinematicCamera::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<CinematicCamera>(device, context);

    if(FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Create : Cinematic Camera");
        return nullptr;
    }
    return instance;
}

Shared<GameObject> CinematicCamera::Clone(void* arg)
{
    auto instance = make_shared<CinematicCamera>(*this);

    if(FAILED(instance->Initialize(arg)))
    {
        MSG_BOX("Failed to Clone : Cinematic Camera");
        return nullptr;
    }
    return instance;
}

void CinematicCamera::Free()
{
    __super::Free();
}

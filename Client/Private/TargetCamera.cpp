#include "TargetCamera.h"

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
    if(FAILED(__super::Initialize(arg)))
        return E_FAIL;

    return S_OK;
}


void TargetCamera::Priority_Update(float timeDelta)
{
    __super::Priority_Update(timeDelta);
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
    return __super::Render();
}

Shared<TargetCamera> TargetCamera::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
}

Shared<GameObject> TargetCamera::Clone(void* arg)
{
}

void TargetCamera::Free()
{
    Camera::Free();
}

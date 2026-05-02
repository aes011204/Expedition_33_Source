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
    if (nullptr == arg)
        return E_FAIL;

    if(FAILED(__super::Initialize(arg)))
        return E_FAIL;

    Update_TransformMatrices();

    return S_OK;
}

void TargetCamera::Priority_Update(float timeDelta)
{
    __super::Priority_Update(timeDelta);

   auto targetTransformCom  = _targetTransformCom.lock();

    if(nullptr == targetTransformCom)
        return;

    Vec3 targetPos = targetTransformCom->Get_WorldPosition();

    _transformCom->Set_Position(targetPos);

    Update_TransformMatrices();
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

    if (FAILED(instance->Initialize_Prototype()))
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

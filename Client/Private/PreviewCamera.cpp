#include "PreviewCamera.h"
#include "GameInstance.h"

PreviewCamera::PreviewCamera(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Camera{ device, context }
{
}

PreviewCamera::PreviewCamera(const PreviewCamera& prototype)
    : Camera{ prototype }
{
}

void PreviewCamera::BeginPlay()
{
    Camera::BeginPlay();

    // 이거 추가하면, 카메라쪽 로직 꼬이려나 잘 모르겠네;
    GAME->Add_Camera(GetSharedPtr<Camera>());
}

HRESULT PreviewCamera::Initialize_Prototype()
{
    return S_OK;
}

HRESULT PreviewCamera::Initialize(void* arg)
{
    CHECK_FAILED(Camera::Initialize(arg), E_FAIL);

    const auto* desc = static_cast<Preview_CameraDesc*>(arg);
    if (desc != nullptr && _transformCom != nullptr)
    {
        const Vec3 at(desc->at.x, desc->at.y, desc->at.z);
        _transformCom->LookAt(at);
    }

    Set_Name(L"Preview Camera");
    return S_OK;
}

void PreviewCamera::Update(float timeDelta)
{
}

void PreviewCamera::Apply_View()
{
    Update_TransformMatrices();
}

Shared<PreviewCamera> PreviewCamera::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<PreviewCamera>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> PreviewCamera::Clone(void* arg)
{
    auto instance = make_shared<PreviewCamera>(*this);

    if (FAILED(instance->Initialize(arg)))
        return nullptr;

    return instance;
}

void PreviewCamera::Free()
{
    Camera::Free();
}

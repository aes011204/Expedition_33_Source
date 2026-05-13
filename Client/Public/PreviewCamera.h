#pragma once

#include "Client_Defines.h"
#include "Camera.h"

NS_BEGIN(Client)

// UI에서 사용할 카메라 -> 일단 임시임
class PreviewCamera final : public Camera
{
    GENERATED_GAMEOBJECT(PreviewCamera, ObjectType::PreviewCamera)

public:
    using Preview_CameraDesc = Camera::CAMERA_DESC;

public:
    PreviewCamera(const ComPtr<Device>& device, const ComPtr<Context>& context);
    PreviewCamera(const PreviewCamera& prototype);
    ~PreviewCamera() override = default;

public:
    void    BeginPlay() override;
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    Update(float timeDelta) override;

    void    Apply_View();

public:
    static Shared<PreviewCamera> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END

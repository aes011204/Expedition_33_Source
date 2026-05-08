#pragma once

#include "Camera.h"

NS_BEGIN(MainEditor)

class Editor_Camera_Free final : public Engine::Camera
{
public:
    using EDITOR_CAMERA_DESC = Engine::Camera::CAMERA_DESC;

public:
    Editor_Camera_Free(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Editor_Camera_Free(const Editor_Camera_Free& prototype);
    virtual ~Editor_Camera_Free() = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void Priority_Update(float timeDelta) override;

    Matrix Get_ViewMatrix() const;
    void Apply_EditorDesc(const EDITOR_CAMERA_DESC& desc);

private:
    void Sync_RotationFromAngles();

public:
    static Shared<Editor_Camera_Free> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END

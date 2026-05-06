#pragma once
#include "Camera.h"

NS_BEGIN(ModelEditor)

class ModelEditorCamera final : public Camera
{
public:
    struct ModelEditorCameraDesc : public CAMERA_DESC
    {
    };

public:
    ModelEditorCamera(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ~ModelEditorCamera() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    Priority_Update(float timeDelta) override;
    void    Update(float timeDelta) override;
    void    Late_Update(float timeDelta) override;

private:
    uint32 _presetIndex{ 0 };
    bool  _firstFrame{ true };  
    bool  _isRightMouseDown{ false };
    float _speed{ 3.f };

public:
    static Shared<ModelEditorCamera> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END

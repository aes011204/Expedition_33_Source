#pragma once
#include "MapEditorDefines.h"
#include "Camera.h"

NS_BEGIN(MapEditor)

class MEBridgeCamera final : public Camera
{
public:
    MEBridgeCamera(const ComPtr<Device>& device, const ComPtr<Context>& context)
        : Camera(device, context) {
    }

    virtual HRESULT Initialize(void* arg) override
    {
        if (FAILED(GameObject::Initialize(arg))) return E_FAIL;
        _far = 1000.f;
        return S_OK;
    }

    virtual Shared<GameObject> Clone(void* arg) override { return nullptr; }

    static Shared<MEBridgeCamera> Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
    {
        auto instance = make_shared<MEBridgeCamera>(device, context);
        if (FAILED(instance->Initialize(nullptr)))
            return nullptr;
        return instance;
    }
};

NS_END

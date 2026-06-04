#pragma once
#include "VIBuffer.h"

NS_BEGIN(Engine)

class ENGINE_DLL VIBuffer_Plane final : public VIBuffer
{
public:
    struct DESC {
        uint32 countX = 64;
        uint32 countZ = 64;
        float  sizeX = 1.f;
        float  sizeZ = 1.f;
    };

public:
    VIBuffer_Plane(const ComPtr<Device>& device, const ComPtr<Context>& context);
    VIBuffer_Plane(const VIBuffer_Plane& prototype);
    virtual ~VIBuffer_Plane() = default;

public:
    virtual HRESULT Initialize_Prototype(const DESC& desc);
    virtual HRESULT Initialize(void* arg) override;

public:
    static Shared<VIBuffer_Plane> Create(const ComPtr<Device>& device, const ComPtr<Context>& context, const DESC& desc);
    virtual Shared<Component> Clone(void* arg) override;
    virtual void Free() override;
};

NS_END

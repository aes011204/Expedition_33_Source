#pragma once
#include "VIBufferCom_Instance.h"
#include "Vertex_Types.h"

NS_BEGIN(Engine)

struct MeshInstanceInitDesc
{
    const VTXNORTEX* vertices = {};
    uint32 vertexCount = {};
    const uint16* indices = {};
    uint32 indexCount = {};

    const MESHINSTANCEDATA* instances = {};
    uint32 instanceCount = {};
};



class ENGINE_DLL VIBuffer_MeshInstance final : public VIBufferCom_Instance
{
public:
    VIBuffer_MeshInstance(const ComPtr<Device>& device, const ComPtr<Context>& context);
    VIBuffer_MeshInstance(const VIBuffer_MeshInstance& prototype);
    virtual ~VIBuffer_MeshInstance() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;

private:
    uint32 _instanceCapacity = {};

public:
    HRESULT UpdateInstances(const MESHINSTANCEDATA* instances, uint32 count);

public:
    static Shared<VIBuffer_MeshInstance> Create(const ComPtr<Device>& device,
        const ComPtr<Context>& context);
    Shared<Component> Clone(void* arg) override;
    void Free() override;
};

NS_END

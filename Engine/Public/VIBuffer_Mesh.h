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



class ENGINE_DLL VIBuffer_Mesh final// : public VIBufferCom_Instance
{
//public:
//    VIBuffer_Mesh(const ComPtr<Device>& device, const ComPtr<Context>& context);
//    VIBuffer_Mesh(const VIBuffer_Mesh& prototype);
//    virtual ~VIBuffer_Mesh() = default;
//
//public:
//    HRESULT Initialize_Prototype() override;
//    HRESULT Initialize(void* arg) override;
//
//public:
//    HRESULT UpdateInstances(const MESHINSTANCEDATA* instances, uint32 count);
//
//public:
//    static Shared<VIBuffer_Mesh> Create(const ComPtr<Device>& device,
//        const ComPtr<Context>& context);
//    Shared<Component> Clone(void* arg) override;
//    void Free() override;
};

NS_END

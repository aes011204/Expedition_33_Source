#pragma once
#include "VIBuffer.h"

NS_BEGIN(Engine)

class ENGINE_DLL VIBufferCom_Instance abstract : public VIBuffer
{
public:
    VIBufferCom_Instance(const ComPtr<Device>& device, const ComPtr<Context>& context);
    VIBufferCom_Instance(const VIBufferCom_Instance& prototype);
    ~VIBufferCom_Instance() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    HRESULT Bind_Resources() override;
    HRESULT Render() override;

protected: //## Data::GPU
    ComPtr<Buffer> _instanceVB{}; // 인스턴스별 정점 버퍼.

protected: //## Data::InstanceDraw
    uint32 _instanceStride{};      // 인스턴스 데이터 1개의 바이트 크기.
    uint32 _instanceCount{};       // 제출할 인스턴스 수.
    uint32 _verticesPerInstance{}; // non-indexed instanced draw에서 인스턴스당 정점 수.

public:
    Shared<Component> Clone(void* arg) override = 0;
    void Free() override;
};

NS_END

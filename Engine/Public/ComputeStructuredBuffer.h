#pragma once
#include "Base.h"

NS_BEGIN(Engine)

/**
 * @class ComputeStructuredBuffer
 * @brief compute shader 입출력에 사용할 structured buffer wrapper.
 * @details
 * - compute shader 입출력 buffer를 만들고 데이터 갱신/복사를 제공한다.
 * - shader compile, slot binding, Dispatch는 ComputeShaderCom이 담당한다.
 */
class ENGINE_DLL ComputeStructuredBuffer final : public Base
{
public:
    ComputeStructuredBuffer(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ~ComputeStructuredBuffer() override;

public:
    // structured buffer와 그 SRV/UAV를 함께 만든다.
    HRESULT Initialize(
        uint32 elementSize,
        uint32 elementCount,
        const void* initialData = nullptr,
        uint32 extraBindFlags = 0);

public: //## Behavior::DataTransfer
    // buffer 앞쪽 elementCount개를 새 데이터로 갱신한다.
    HRESULT Update_Data(const void* data, uint32 elementCount);
    // 같은 크기/형식의 GPU resource에서 현재 buffer로 복사한다.
    HRESULT Copy_From(ID3D11Resource* source);

public: //## Accessors
    Buffer* Get_Buffer() const { return _buffer.Get(); }        // 원본 D3D buffer 접근.
    ShaderResourceView* Get_SRV() const { return _srv.Get(); }  // compute/render shader 읽기 view.
    UnorderedAccessView* Get_UAV() const { return _uav.Get(); } // compute shader 쓰기 view.

    uint32 Get_ElementSize() const { return _elementSize; }   // element 하나의 byte 크기.
    uint32 Get_ElementCount() const { return _elementCount; } // buffer element 개수.

private: //## Data::Device
    ComPtr<Device> _device{};   // buffer/view 생성에 사용하는 device.
    ComPtr<Context> _context{}; // update/copy에 사용하는 immediate context.

private: //## Data::Resource
    ComPtr<Buffer> _buffer{};           // structured GPU buffer.
    ComPtr<ShaderResourceView> _srv{};  // buffer read view.
    ComPtr<UnorderedAccessView> _uav{}; // buffer write view.

private: //## Data::Layout
    uint32 _elementSize{ 0 };  // D3D11 StructureByteStride와 동일한 값.
    uint32 _elementCount{ 0 }; // SRV/UAV NumElements와 동일한 값.
    uint32 _extraBindFlags { 0 };

private: //## Helper::Setup
    HRESULT Ready_Views(); // 현재 buffer desc 기준으로 SRV/UAV를 만든다.

public:
    static Shared<ComputeStructuredBuffer> Create(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context,
        uint32 elementSize,
        uint32 elementCount,
        const void* initialData = nullptr,
        uint32 extraBindFlags = 0);
    void Free() override;
};

NS_END

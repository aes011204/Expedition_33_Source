#pragma once
#include "Component.h"

NS_BEGIN(Engine)

/**
 * @class ComputeShaderCom
 * @brief DX11 compute shader 실행을 담당하는 컴포넌트
 * @details
 * - compute-only hlsl을 cs_5_0으로 직접 컴파일하고, SRV/UAV/constant buffer slot 바인딩 후 Dispatch를 수행한다.
 * - buffer 생성과 lifetime은 소유하지 않는다.
 * - StructuredBuffer 계열 resource는 ComputeStructuredBuffer 같은 별도 wrapper가 담당한다.
 */
class ENGINE_DLL ComputeShaderCom final : public Component
{
public:
    ComputeShaderCom(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ComputeShaderCom(const ComputeShaderCom& prototype);
    ~ComputeShaderCom() override;

public:
    // compute-only hlsl 파일과 entry point로 cs_5_0 shader를 만든다.
    HRESULT Initialize_Prototype(const wchar_t* shaderFilePath, const char* entryPoint);
    HRESULT Initialize(void* arg) override;

public: //## Behavior::Binding
    // CS SRV slot에 읽기 resource를 바인딩하고, 이후 Clear_Bindings 대상 slot으로 기록한다.
    HRESULT Bind_SRV(uint32 slot, ShaderResourceView* srv);
    // CS UAV slot에 쓰기 resource를 바인딩하고, 이후 Clear_Bindings 대상 slot으로 기록한다.
    HRESULT Bind_UAV(uint32 slot, UnorderedAccessView* uav);
    // CS constant buffer slot에 buffer를 바인딩하고, 이후 Clear_Bindings 대상 slot으로 기록한다.
    HRESULT Bind_ConstantBuffer(uint32 slot, Buffer* buffer);
    // compute shader를 바인딩해 Dispatch를 실행한 뒤, 이 객체가 바인딩한 CS slot만 정리한다.
    HRESULT Dispatch(uint32 groupX, uint32 groupY, uint32 groupZ);
    // 이 객체가 기록한 CS SRV/UAV/CB slot과 CS shader binding을 해제한다.
    void Clear_Bindings();

private: //## Constant::D3DSlot
    // Direct3D 11이 정의한 CS resource binding slot 한계값.
    static constexpr uint32 srvSlotCount{ D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT };
    static constexpr uint32 uavSlotCount{ D3D11_PS_CS_UAV_REGISTER_COUNT };
    static constexpr uint32 constantBufferSlotCount{ D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT };

private: //## Data::Shader
    ComPtr<ComputeShader> _computeShader{}; // prototype이 공유하는 실제 compute shader 객체.

private: //## Data::TrackedBindings
    vector<uint32> _boundSRVSlots{};            // Clear_Bindings에서 해제할 SRV slot 목록.
    vector<uint32> _boundUAVSlots{};            // Clear_Bindings에서 해제할 UAV slot 목록.
    vector<uint32> _boundConstantBufferSlots{}; // Clear_Bindings에서 해제할 CB slot 목록.

private: //## Helper::Binding
    static void Track_Slot(vector<uint32>& slots, uint32 slot); // 중복 없이 slot 목록에 기록한다.

public:
    static Shared<ComputeShaderCom> Create(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context,
        const wchar_t* shaderFilePath,
        const char* entryPoint);
    Shared<Component> Clone(void* arg) override;
    void Free() override;
};

NS_END

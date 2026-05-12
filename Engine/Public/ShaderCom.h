#pragma once

#include "Component.h"
#include "Material_Types.h"

NS_BEGIN(Engine)
class Material;
struct MaterialRenderState;

class ENGINE_DLL ShaderCom final : public Component
{
public:
    ShaderCom(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ShaderCom(const ShaderCom& prototype);
    virtual ~ShaderCom() = default;

public:
    virtual HRESULT Initialize_Prototype(const wchar_t* shaderFilePath, const D3D11_INPUT_ELEMENT_DESC* elements, uint32 numElements, ShaderType shader);
    virtual HRESULT Initialize(void* arg);

public:
    HRESULT Begin(uint32 passIndex);
    HRESULT Bind_SRV(const char* constantName, ID3D11ShaderResourceView* srv);
    HRESULT Bind_SRVs(const char* constantName, ID3D11ShaderResourceView** ppSRVs, uint32 numSRVs);
    HRESULT Bind_Matrix(const char* constantName, const XMFLOAT4X4* matrix);
    HRESULT Bind_Matrices(const char* constantName, const XMFLOAT4X4* matrices, uint32 numMatrices);
    HRESULT Bind_RawValue(const char* constantName, const void* data, uint32 length);

    HRESULT Bind_MaterialTextures(const Material& material);
    HRESULT Bind_ObjectCB(const Matrix& worldMatrix);

    ShaderType  Get_Type() const { return _type; }
private :
    HRESULT Update_CBuffer(ConstantBuffer cbType, const void* pData, size_t size);

public:
    template<typename Desc>
    HRESULT Bind_CBufferData(const Desc& desc)
    {
        static_assert(sizeof(Desc) % 16 == 0,
            "ConstantBuffer data size must be 16-byte aligned.");

        return Update_CBuffer(Desc::CBType, &desc, sizeof(Desc));
    }

private:
    ComPtr<ID3DX11Effect> _effect = {};
    uint32 _numPasses = 0;

    vector<ComPtr<ID3D11InputLayout>> _inputLayouts;
    ShaderType                          _type;

    // 핸들
    ComPtr<ID3DX11EffectConstantBuffer>             _CB[ETOI(ConstantBuffer::END)]{};
    ComPtr<ID3DX11EffectShaderResourceVariable>     _materialTextures{};

    // GPU 주소
    ComPtr<ID3D11Buffer>                            _allocatedCB[ETOI(ConstantBuffer::END)]{};

public:
    static Shared<ShaderCom> Create(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context,
        const wchar_t* shaderFilePath,
        const D3D11_INPUT_ELEMENT_DESC* elements,
        uint32 numElements,
        ShaderType shade);

    virtual Shared<Component> Clone(void* arg) override;
    virtual void Free() override;
};

NS_END

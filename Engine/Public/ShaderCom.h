#pragma once

#include "Component.h"

NS_BEGIN(Engine)

class ENGINE_DLL ShaderCom final : public Component
{
public:
    ShaderCom(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ShaderCom(const ShaderCom& prototype);
    virtual ~ShaderCom() = default;

public:
    virtual HRESULT Initialize_Prototype(const wchar_t* shaderFilePath, const D3D11_INPUT_ELEMENT_DESC* elements, uint32 numElements);
    virtual HRESULT Initialize(void* arg);

public:
    HRESULT Begin(uint32 passIndex);
    HRESULT Bind_SRV(const char* constantName, ID3D11ShaderResourceView* srv);
    HRESULT Bind_SRVs(const char* constantName, ID3D11ShaderResourceView** ppSRVs, uint32 numSRVs);
    HRESULT Bind_Matrix(const char* constantName, const XMFLOAT4X4* matrix);
    HRESULT Bind_Matrices(const char* constantName, const XMFLOAT4X4* matrices, uint32 numMatrices);
    HRESULT Bind_RawValue(const char* constantName, const void* data, uint32 length);

private:
    ComPtr<ID3DX11Effect> _effect = {};
    uint32 _numPasses = 0;

    vector<ComPtr<ID3D11InputLayout>> _inputLayouts;

public:
    static Shared<ShaderCom> Create(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context,
        const wchar_t* shaderFilePath,
        const D3D11_INPUT_ELEMENT_DESC* elements,
        uint32 numElements);

    virtual Shared<Component> Clone(void* arg) override;
    virtual void Free() override;
};

NS_END

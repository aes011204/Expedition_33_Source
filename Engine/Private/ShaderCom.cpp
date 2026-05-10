#include "ShaderCom.h"
#include "Material.h"

ShaderCom::ShaderCom(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Component{ device, context }
{
}

ShaderCom::ShaderCom(const ShaderCom& prototype)
    : Component{ prototype }
    , _effect{ prototype._effect }
    , _numPasses{ prototype._numPasses }
    , _inputLayouts{ prototype._inputLayouts }
{
    for (uint32 i = 0; i < ETOI(ConstantBuffer::END); ++i)
        _CB[i] = prototype._CB[i];

    _materialTextures = prototype._materialTextures;
}

HRESULT ShaderCom::Initialize_Prototype(const wchar_t* shaderFilePath, const D3D11_INPUT_ELEMENT_DESC* elements, uint32 numElements)
{
    uint32 hlslFlag = 0;

#ifdef _DEBUG
    hlslFlag |= D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_DEBUG;
#else
    hlslFlag |= D3DCOMPILE_OPTIMIZATION_LEVEL1;
#endif

    if (FAILED(D3DX11CompileEffectFromFile(
        shaderFilePath,
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        hlslFlag,
        0,
        _device.Get(),
        _effect.GetAddressOf(),
        nullptr)))
    {
        return E_FAIL;
    }

    ID3DX11EffectTechnique* technique = _effect->GetTechniqueByIndex(0);
    if (nullptr == technique)
        return E_FAIL;

    D3DX11_TECHNIQUE_DESC techniqueDesc{};
    technique->GetDesc(&techniqueDesc);

    _numPasses = techniqueDesc.Passes;
    _inputLayouts.clear();
    _inputLayouts.reserve(_numPasses);

    for (uint32 i = 0; i < _numPasses; ++i)
    {
        ID3DX11EffectPass* pass = technique->GetPassByIndex(i);
        if (nullptr == pass)
            return E_FAIL;

        D3DX11_PASS_DESC passDesc{};
        pass->GetDesc(&passDesc);

        ComPtr<ID3D11InputLayout> inputLayout;

        if (FAILED(_device->CreateInputLayout(
            elements,
            numElements,
            passDesc.pIAInputSignature,
            passDesc.IAInputSignatureSize,
            inputLayout.GetAddressOf())))
        {
            return E_FAIL;
        }

        _inputLayouts.emplace_back(std::move(inputLayout));
    }

    // TODO CB 확장에 따른 추가 !!  캐싱 ==========================================
    _CB[ETOI(ConstantBuffer::Material)] =
        _effect->GetConstantBufferByName("MaterialBuffer");

    _materialTextures = _effect->GetVariableByName("g_MaterialTextures")->AsShaderResource();

    return S_OK;
}

HRESULT ShaderCom::Initialize(void* arg)
{
    return S_OK;
}

HRESULT ShaderCom::Begin(uint32 passIndex)
{
    if (passIndex >= _numPasses || !_inputLayouts[passIndex])
        return E_FAIL;

    _effect->GetTechniqueByIndex(0)->GetPassByIndex(passIndex)->Apply(0, _context.Get());
    _context->IASetInputLayout(_inputLayouts[passIndex].Get());

    return S_OK;
}

HRESULT ShaderCom::Bind_SRV(const char* constantName, ID3D11ShaderResourceView* srv)
{
    ID3DX11EffectVariable* variable = _effect->GetVariableByName(constantName);
    if (nullptr == variable)
    {
        MSG_BOX("Failed to throw value to shader");
        return E_FAIL;
    }

    ID3DX11EffectShaderResourceVariable* srVariable = variable->AsShaderResource();
    if (nullptr == srVariable)
    {
        MSG_BOX("ShaderCom types do not match");
        return E_FAIL;
    }

    return srVariable->SetResource(srv);
}

HRESULT ShaderCom::Bind_SRVs(const char* constantName, ID3D11ShaderResourceView** ppSRVs, uint32 numSRVs)
{
    ID3DX11EffectVariable* variable = _effect->GetVariableByName(constantName);
    if (nullptr == variable)
    {
        MSG_BOX("Failed to throw value to shader");
        return E_FAIL;
    }

    ID3DX11EffectShaderResourceVariable* srVariable = variable->AsShaderResource();
    if (nullptr == srVariable)
    {
        MSG_BOX("ShaderCom types do not match");
        return E_FAIL;
    }

    return srVariable->SetResourceArray(ppSRVs, 0, numSRVs);
}

HRESULT ShaderCom::Bind_Matrix(const char* constantName, const XMFLOAT4X4* matrix)
{
    ID3DX11EffectVariable* variable = _effect->GetVariableByName(constantName);
    if (nullptr == variable)
    {
        MSG_BOX("Failed to throw value to shader");
        return E_FAIL;
    }

    ID3DX11EffectMatrixVariable* matrixVariable = variable->AsMatrix();
    if (nullptr == matrixVariable)
    {
        MSG_BOX("ShaderCom types do not match");
        return E_FAIL;
    }

    return matrixVariable->SetMatrix(reinterpret_cast<const float*>(matrix));
}

HRESULT ShaderCom::Bind_Matrices(const char* constantName, const XMFLOAT4X4* matrices, uint32 numMatrices)
{
    ID3DX11EffectVariable* variable = _effect->GetVariableByName(constantName);
    if (nullptr == variable)
    {
        MSG_BOX("Failed to throw value to shader");
        return E_FAIL;
    }

    ID3DX11EffectMatrixVariable* matrixVariable = variable->AsMatrix();
    if (nullptr == matrixVariable)
    {
        MSG_BOX("ShaderCom types do not match");
        return E_FAIL;
    }

    return matrixVariable->SetMatrixArray(reinterpret_cast<const float*>(matrices), 0, numMatrices);
}

HRESULT ShaderCom::Bind_RawValue(const char* constantName, const void* data, uint32 length)
{
    ID3DX11EffectVariable* variable = _effect->GetVariableByName(constantName);
    if (nullptr == variable)
    {
        MSG_BOX("Failed to throw value to shader");
        return E_FAIL;
    }

    return variable->SetRawValue(data, 0, length);
}

HRESULT ShaderCom::Update_CBuffer(ConstantBuffer cbType, const void* pData, size_t size)
{
    if (pData == nullptr || size == 0)
        return E_INVALIDARG;

    const uint32 cbIndex = ETOI(cbType);
    if (cbIndex >= ETOI(ConstantBuffer::END))
        return E_FAIL;

    if (_CB[cbIndex] == nullptr)
        return E_FAIL;

    if (_allocatedCB[cbIndex] == nullptr)
    {
        D3D11_BUFFER_DESC bufferDesc{};
        bufferDesc.ByteWidth = static_cast<UINT>(size);
        bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        bufferDesc.MiscFlags = 0;
        bufferDesc.StructureByteStride = 0;

        if (FAILED(_device->CreateBuffer(&bufferDesc, nullptr, _allocatedCB[cbIndex].GetAddressOf())))
            return E_FAIL;
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(_context->Map(_allocatedCB[cbIndex].Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return E_FAIL;

    memcpy(mapped.pData, pData, size);
    _context->Unmap(_allocatedCB[cbIndex].Get(), 0);

    return _CB[cbIndex]->SetConstantBuffer(_allocatedCB[cbIndex].Get());
}

HRESULT ShaderCom::Bind_MaterialTextures(const Material& material)
{
    if (_materialTextures == nullptr)
        return E_FAIL;

    ID3D11ShaderResourceView* srvs[ETOI(MaterialTextureSlot::END)]{};

    for (uint32 i = 0; i < ETOI(MaterialTextureSlot::END); ++i)
    {
        const Material::TextureSlot* textureSlot =
            material.Find_TextureSlot(static_cast<MaterialTextureSlot>(i), 0);

        if (textureSlot != nullptr && textureSlot->shaderResourceView != nullptr)
            srvs[i] = textureSlot->shaderResourceView.Get();
    }

    return _materialTextures->SetResourceArray(srvs, 0, ETOI(MaterialTextureSlot::END));
}

Shared<ShaderCom> ShaderCom::Create(
    const ComPtr<Device>& device,
    const ComPtr<Context>& context,
    const wchar_t* shaderFilePath,
    const D3D11_INPUT_ELEMENT_DESC* elements,
    uint32 numElements)
{
    auto instance = make_shared<ShaderCom>(device, context);

    if (FAILED(instance->Initialize_Prototype(shaderFilePath, elements, numElements)))
    {
        LOG_WARN("Failed to create ShaderCom: path='{}'", String::ToString(shaderFilePath));
        return nullptr;
    }

    return instance;
}

Shared<Component> ShaderCom::Clone(void* arg)
{
    auto instance = make_shared<ShaderCom>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : ShaderCom");
        return nullptr;
    }

    return instance;
}

void ShaderCom::Free()
{
    __super::Free();

    _inputLayouts.clear();
    _effect.Reset();
    _numPasses = 0;
}

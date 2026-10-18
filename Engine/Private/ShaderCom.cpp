#include "ShaderCom.h"
#include "Material.h"
#include "GameInstance.h"
#include "ShaderCom_Controller.h"

ShaderCom::ShaderCom(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Component{ device, context }
{
}

ShaderCom::ShaderCom(const ShaderCom& prototype)
    : Component{ prototype }
    , _effect{ prototype._effect }
    , _numPasses{ prototype._numPasses }
    , _inputLayouts{ prototype._inputLayouts }
    , _type{ prototype._type }
    , _shaderFilePath{ prototype._shaderFilePath }
    , _inputElements{ prototype._inputElements }
    , _numInputElements{ prototype._numInputElements }
{
    for (uint32 i = 0; i < ETOI(ConstantBuffer::END); ++i)
        _CB[i] = prototype._CB[i];

    _materialTextures = prototype._materialTextures;
    _drawFeatureTextures = prototype._drawFeatureTextures;

    for (uint32 i = 0; i < _countof(_featureSRVs); ++i)
        _featureSRVs[i] = nullptr;
}

void ShaderCom::Reset_EffectState()
{
    _effectVariableCache.clear();
    _matrixVariableCache.clear();
    _shaderResourceVariableCache.clear();

    _inputLayouts.clear();
    _numPasses = 0;

    for (uint32 i = 0; i < ETOI(ConstantBuffer::END); ++i)
    {
        if (_CB[i])
            _CB[i].Reset();
        if (_allocatedCB[i])
            _allocatedCB[i].Reset();
    }

    _materialTextures.Reset();
    _drawFeatureTextures.Reset();

    for (uint32 i = 0; i < _countof(_featureSRVs); ++i)
        _featureSRVs[i] = nullptr;

    _effect.Reset();
}

void ShaderCom::Copy_EffectState(const ShaderCom& source)
{
    _effect = source._effect;
    _numPasses = source._numPasses;
    _inputLayouts = source._inputLayouts;
    _type = source._type;
    _shaderFilePath = source._shaderFilePath;
    _inputElements = source._inputElements;
    _numInputElements = source._numInputElements;

    for (uint32 i = 0; i < ETOI(ConstantBuffer::END); ++i)
        _CB[i] = source._CB[i];

    _materialTextures = source._materialTextures;
    _drawFeatureTextures = source._drawFeatureTextures;

    _effectVariableCache = source._effectVariableCache;
    _matrixVariableCache = source._matrixVariableCache;
    _shaderResourceVariableCache = source._shaderResourceVariableCache;

    for (uint32 i = 0; i < _countof(_featureSRVs); ++i)
        _featureSRVs[i] = nullptr;
}

HRESULT ShaderCom::Initialize_Prototype(const wchar_t* shaderFilePath, const D3D11_INPUT_ELEMENT_DESC* elements, uint32 numElements, ShaderType shader, bool registerWithController)
{
    uint32 hlslFlag = 0;

#ifdef _DEBUG
    hlslFlag |= D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_DEBUG;
#else
    hlslFlag |= D3DCOMPILE_OPTIMIZATION_LEVEL1;
#endif

    ComPtr<ID3DBlob> errorBlob = nullptr;

    if (FAILED(D3DX11CompileEffectFromFile(
        shaderFilePath,
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        hlslFlag,
        0,
        _device.Get(),
        _effect.GetAddressOf(),
        errorBlob.GetAddressOf())))
    {
        if (errorBlob)
            OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        return E_FAIL;
    }

    _shaderFilePath = shaderFilePath != nullptr ? shaderFilePath : L"";
    _inputElements.clear();
    _numInputElements = numElements;
    if (elements != nullptr && numElements > 0)
        _inputElements.assign(elements, elements + numElements);

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
    
    // Effect 변수 핸들 캐싱
    for (uint32 i = 0; i < ETOI(ConstantBuffer::END); ++i)
    {
        auto buffer = _effect->GetConstantBufferByName(ShaderCB::Names[i].data());
        if (buffer != nullptr && buffer->IsValid())
            _CB[i] = buffer;
    }

    _materialTextures = Get_ShaderResourceVariable(ShaderSRV::MaterialTextures.data());
    _drawFeatureTextures = Get_ShaderResourceVariable(ShaderSRV::DrawFeatureTextures.data());

    _type = shader;

    if (registerWithController && GAME != nullptr)
    {
        if (auto* controller = GAME->Get_ShaderComController())
            controller->Register_Shader(GetSharedPtr<ShaderCom>());
    }

    return S_OK;
}

HRESULT ShaderCom::Initialize(void* arg)
{
    if (GAME != nullptr)
    {
        if (auto* controller = GAME->Get_ShaderComController())
            controller->Register_Shader(GetSharedPtr<ShaderCom>());
    }

    return S_OK;
}

HRESULT ShaderCom::Recreate(const ShaderCom& source)
{
    if (source._effect == nullptr)
        return E_FAIL;

    Reset_EffectState();
    Copy_EffectState(source);
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
    ID3DX11EffectShaderResourceVariable* srVariable = Get_ShaderResourceVariable(constantName);
    if (srVariable == nullptr)
        return E_FAIL;

    return srVariable->SetResource(srv);
}

HRESULT ShaderCom::Bind_SRVs(const char* constantName, ID3D11ShaderResourceView** ppSRVs, uint32 numSRVs)
{
     if (numSRVs == 0)
        return S_OK;

    if (ppSRVs == nullptr)
        return E_INVALIDARG;

    ID3DX11EffectShaderResourceVariable* srVariable = Get_ShaderResourceVariable(constantName);
    if (srVariable == nullptr)
        return E_FAIL;

    return srVariable->SetResourceArray(ppSRVs, 0, numSRVs);
}

HRESULT ShaderCom::Bind_Matrix(const char* constantName, const XMFLOAT4X4* matrix)
{
    if (matrix == nullptr)
        return E_INVALIDARG;

    ID3DX11EffectMatrixVariable* matrixVariable = Get_MatrixVariable(constantName);
    if (matrixVariable == nullptr)
        return E_FAIL;

    return matrixVariable->SetMatrix(reinterpret_cast<const float*>(matrix));
}

HRESULT ShaderCom::Bind_Matrices(const char* constantName, const XMFLOAT4X4* matrices, uint32 numMatrices)
{
    if (matrices == nullptr || numMatrices == 0)
        return E_INVALIDARG;

    ID3DX11EffectMatrixVariable* matrixVariable = Get_MatrixVariable(constantName);
    if (matrixVariable == nullptr)
        return E_FAIL;

    return matrixVariable->SetMatrixArray(
        reinterpret_cast<const float*>(matrices),
        0,
        numMatrices);
}

HRESULT ShaderCom::Bind_RawValue(const char* constantName, const void* data, uint32 length)
{
    if (data == nullptr || length == 0)
        return E_INVALIDARG;

    ID3DX11EffectVariable* variable = Get_EffectVariable(constantName);
    if (variable == nullptr)
        return E_FAIL;

    return variable->SetRawValue(data, 0, length);
}

HRESULT ShaderCom::Update_CBuffer(ConstantBuffer cbType, const void* pData, size_t size)
{
    if (pData == nullptr || size == 0)
        return E_INVALIDARG;

    const uint32 cbIndex = ETOI(cbType);
    if (cbIndex >= ETOI(ConstantBuffer::END))
        return E_FAIL;

    if (_CB[cbIndex] == nullptr || !_CB[cbIndex]->IsValid())
        return S_OK;

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

ID3DX11EffectVariable* ShaderCom::Get_EffectVariable(const char* constantName)
{
    if (constantName == nullptr)
        return nullptr;

    const string key = constantName;

    auto iter = _effectVariableCache.find(key);
    if (iter != _effectVariableCache.end())
        return iter->second;

    ID3DX11EffectVariable* variable = _effect->GetVariableByName(constantName);
    if (variable == nullptr || !variable->IsValid())
        return nullptr;

    _effectVariableCache.emplace(key, variable);
    return variable;
}   

ID3DX11EffectMatrixVariable* ShaderCom::Get_MatrixVariable(const char* constantName)
{
    if (constantName == nullptr)
        return nullptr;

    const string key = constantName;

    auto iter = _matrixVariableCache.find(key);
    if (iter != _matrixVariableCache.end())
        return iter->second;

    ID3DX11EffectVariable* variable = Get_EffectVariable(constantName);
    if (variable == nullptr)
        return nullptr;

    ID3DX11EffectMatrixVariable* matrixVariable = variable->AsMatrix();
    if (matrixVariable == nullptr || !matrixVariable->IsValid())
        return nullptr;

    _matrixVariableCache.emplace(key, matrixVariable);
    return matrixVariable;
}

ID3DX11EffectShaderResourceVariable* ShaderCom::Get_ShaderResourceVariable(const char* constantName)
{
    if (constantName == nullptr)
        return nullptr;

    const string key = constantName;

    auto iter = _shaderResourceVariableCache.find(key);
    if (iter != _shaderResourceVariableCache.end())
        return iter->second;

    ID3DX11EffectVariable* variable = Get_EffectVariable(constantName);
    if (variable == nullptr)
        return nullptr;

    ID3DX11EffectShaderResourceVariable* srVariable = variable->AsShaderResource();
    if (srVariable == nullptr || !srVariable->IsValid())
        return nullptr;

    _shaderResourceVariableCache.emplace(key, srVariable);
    return srVariable;
}

HRESULT ShaderCom::Bind_MaterialTextures(Material& material)
{
    if (_materialTextures == nullptr)
        return E_FAIL;

    // PassIndex 계산을 위해 현재 적용 중인 ShaderType을 Material에 동기화한다
    material.Set_ShaderType(Get_Type());

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

HRESULT ShaderCom::Bind_FeatureTextures(ID3D11ShaderResourceView** ppSRVs, uint32 numSRVs)
{
    if (_drawFeatureTextures == nullptr)
        return E_FAIL;

    const uint32 maxCount = static_cast<uint32>(_countof(_featureSRVs));
    if (numSRVs > maxCount)
        return E_FAIL;

    if (numSRVs > 0 && ppSRVs == nullptr)
        return E_FAIL;

    for (uint32 i = 0; i < maxCount; ++i)
        _featureSRVs[i] = (i < numSRVs) ? ppSRVs[i] : nullptr;

    ID3D11ShaderResourceView* rawSRVs[_countof(_featureSRVs)]{};
    for (uint32 i = 0; i < maxCount; ++i)
        rawSRVs[i] = _featureSRVs[i].Get();

    return _drawFeatureTextures->SetResourceArray(rawSRVs, 0, maxCount);
}

HRESULT ShaderCom::Clear_FeatureTextures()
{
    if (_drawFeatureTextures == nullptr)
        return E_FAIL;

    for (uint32 i = 0; i < _countof(_featureSRVs); ++i)
        _featureSRVs[i] = nullptr;

    ID3D11ShaderResourceView* rawSRVs[4]{};
    return _drawFeatureTextures->SetResourceArray(rawSRVs, 0, static_cast<uint32>(_countof(_featureSRVs)));
}

HRESULT ShaderCom::Bind_DrawFeatures(const DrawFeatureCB& cb, ID3D11ShaderResourceView** ppSRVs, uint32 numSRVs)
{
    CHECK_FAILED(Bind_FeatureTextures(ppSRVs, numSRVs), E_FAIL);
    return Bind_CBufferData(cb);
}

HRESULT ShaderCom::Reset_DrawFeatures()
{
    DrawFeatureCB drawFeatureCB{};
    CHECK_FAILED(Clear_FeatureTextures(), E_FAIL);
    return Bind_CBufferData(drawFeatureCB);
}

HRESULT ShaderCom::Bind_ObjectCB(const Matrix& worldMatrix)
{
    ObjectCB objectCB{};
    objectCB.worldMatrix = worldMatrix;

    return Bind_CBufferData(objectCB);
}

Shared<ShaderCom> ShaderCom::Create(
    const ComPtr<Device>& device,
    const ComPtr<Context>& context,
    const wchar_t* shaderFilePath,
    const D3D11_INPUT_ELEMENT_DESC* elements,
    uint32 numElements,
    ShaderType shader,
    bool registerWithController)
{
    auto instance = make_shared<ShaderCom>(device, context);

    if (FAILED(instance->Initialize_Prototype(shaderFilePath, elements, numElements, shader, registerWithController)))
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
    if (GAME != nullptr)
    {
        if (auto* controller = GAME->Get_ShaderComController())
            controller->Unregister_Shader(this);
    }

    __super::Free();

    Reset_EffectState();
    _shaderFilePath.clear();
    _inputElements.clear();
    _numInputElements = 0;
}


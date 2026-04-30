#include "Texture.h"

#include "Shader.h"

Texture::Texture(ComPtr<Device> device, ComPtr<Context> context)
    : Component { device, context }
{
}

Texture::Texture(const Texture& Prototype)
    : Component{ Prototype }
    , _numSRVs { Prototype._numSRVs }
    , _srvs { Prototype._srvs }
{
    for (auto& pSRV : _srvs)
        Safe_AddRef(pSRV);
}

HRESULT Texture::Initialize_Prototype(const wchar_t* pTextureFilePath, uint32 iNumSRVs)
{
    _numSRVs = iNumSRVs;

    for (size_t i = 0; i < iNumSRVs; i++)
    {
        ID3D11ShaderResourceView* pSRV = { nullptr };

        wchar_t          szTextureFilePath[MAX_PATH] = {};

        wsprintf(szTextureFilePath, pTextureFilePath, i);

        wchar_t          szEXT[MAX_PATH] = {};

        _wsplitpath_s(szTextureFilePath, nullptr, 0, nullptr, 0, nullptr, 0, szEXT, MAX_PATH);

        HRESULT         hr = {};

        if (false == lstrcmp(szEXT, TEXT(".dds")))
            hr = CreateDDSTextureFromFile(_device.Get(), szTextureFilePath, nullptr, &pSRV);

        else if (false == lstrcmp(szEXT, TEXT(".tga")))
            hr = E_FAIL;

        else
            hr = CreateWICTextureFromFile(_device.Get(), szTextureFilePath, nullptr, &pSRV);

        if (FAILED(hr))
            return E_FAIL;

        _srvs.push_back(pSRV);
    }

    return S_OK;
}

HRESULT Texture::Initialize(void* pArg)
{
    return S_OK;
}

HRESULT Texture::Bind_ShaderResourceView(Shader* pShader, const char* pConstantName, uint32 iIndex)
{
    if (iIndex >= _numSRVs)
        return E_FAIL;

    return pShader->Bind_SRV(pConstantName, _srvs[iIndex]);    
}

HRESULT Texture::Bind_ShaderResourceViews(Shader* pShader, const char* pConstantName)
{
    return pShader->Bind_SRVs(pConstantName, &_srvs.front(), _numSRVs);    
}

Shared<Texture> Texture::Create(ComPtr<Device> device, ComPtr<Context> context, const wchar_t* pTextureFilePath, uint32 iNumSRVs)
{
    auto pInstance = make_shared<Texture>(device, context);

    if (FAILED(pInstance->Initialize_Prototype(pTextureFilePath, iNumSRVs)))
    {
        MSG_BOX("Failed to Created : Texture");
        return nullptr;
	}
    return pInstance;
}


Shared<Component> Texture::Clone(void* pArg)
{
    auto pInstance = make_shared<Texture>(*this);

    if (FAILED(pInstance->Initialize(pArg)))
    {
        MSG_BOX("Failed to Cloned : Texture");
        return nullptr;
	}
    return pInstance;
}

void Texture::Free()
{
    __super::Free();

    for (auto& pSRV : _srvs)
        Safe_Release(pSRV);

    _srvs.clear();
}




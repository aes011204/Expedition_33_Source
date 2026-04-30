#include "Texture.h"

#include "Shader.h"

Texture::Texture(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Component { device, context }
{
}

Texture::Texture(const Texture& prototype)
    : Component{ prototype }
    , _numSRVs { prototype._numSRVs }
    , _srvs { prototype._srvs }
{
    for (auto& srv : _srvs)
        Safe_AddRef(srv);
}

HRESULT Texture::Initialize_Prototype(const wchar_t* texturePathPattern, uint32 numSRVs)
{
    if (nullptr == texturePathPattern || L'\0' == *texturePathPattern)
        return E_FAIL;

    _numSRVs = numSRVs;

    for (size_t i = 0; i < numSRVs; i++)
    {
        ID3D11ShaderResourceView* srv = { nullptr };

        wchar_t resolvedTexturePath[MAX_PATH] = {};
        if (nullptr != wcschr(texturePathPattern, L'%'))
            swprintf_s(resolvedTexturePath, texturePathPattern, static_cast<int>(i));
        else
            wcscpy_s(resolvedTexturePath, texturePathPattern);

        wchar_t extensionBuffer[MAX_PATH] = {};

        _wsplitpath_s(resolvedTexturePath, nullptr, 0, nullptr, 0, nullptr, 0, extensionBuffer, MAX_PATH);

        HRESULT         hr = {};

        if (false == lstrcmp(extensionBuffer, TEXT(".dds")))
            hr = CreateDDSTextureFromFile(_device.Get(), resolvedTexturePath, nullptr, &srv);

        else if (false == lstrcmp(extensionBuffer, TEXT(".tga")))
            hr = E_FAIL;

        else
            hr = CreateWICTextureFromFile(_device.Get(), resolvedTexturePath, nullptr, &srv);

        if (FAILED(hr))
            return E_FAIL;

        _srvs.push_back(srv);
    }

    return S_OK;
}

HRESULT Texture::Initialize(void* arg)
{
    return S_OK;
}

HRESULT Texture::Bind_ShaderResourceView(Shader* shader, const char* constantName, uint32 index)
{
    if (index >= _numSRVs)
        return E_FAIL;

    return shader->Bind_SRV(constantName, _srvs[index]);    
}

HRESULT Texture::Bind_ShaderResourceViews(Shader* shader, const char* constantName)
{
    return shader->Bind_SRVs(constantName, &_srvs.front(), _numSRVs);    
}

Shared<Texture> Texture::Create(const ComPtr<Device>& device, const ComPtr<Context>& context, const wchar_t* texturePathPattern, uint32 numSRVs)
{
    auto instance = make_shared<Texture>(device, context);

    if (FAILED(instance->Initialize_Prototype(texturePathPattern, numSRVs)))
    {
        MSG_BOX("Failed to Created : Texture");
        return nullptr;
	}
    return instance;
}


Shared<Component> Texture::Clone(void* arg)
{
    auto instance = make_shared<Texture>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : Texture");
        return nullptr;
	}
    return instance;
}

void Texture::Free()
{
    __super::Free();

    for (auto& srv : _srvs)
        Safe_Release(srv);

    _srvs.clear();
}




#include "pch.h"
#include "UI_Editor_PreviewService.h"

#include "GameInstance.h"
#include "UI_Editor_Utils.h"

namespace UIEditor
{
    namespace
    {
        string Make_TexturePreviewGuidKey(const string& textureGuid)
        {
            if (textureGuid.empty())
                return {};

            return "guid:" + textureGuid;
        }

        string Make_TexturePreviewPathKey(const wstring& texturePath)
        {
            if (texturePath.empty())
                return {};

            wstring normalizedPath;
            try
            {
                normalizedPath = fs::path(texturePath).lexically_normal().wstring();
            }
            catch (...)
            {
                normalizedPath = texturePath;
            }

            replace(normalizedPath.begin(), normalizedPath.end(), L'\\', L'/');
            transform(normalizedPath.begin(), normalizedPath.end(), normalizedPath.begin(), [](wchar_t character)
                {
                    return static_cast<wchar_t>(towlower(character));
                });

            return "path:" + String::ToString(normalizedPath);
        }
    }

    ID3D11ShaderResourceView* UI_Editor_PreviewService::Get_TexturePreviewSRV(const string& textureGuid, const string& texturePath)
    {
        if (nullptr == _device || nullptr == _context)
            return nullptr;

        if (_assetManagerReady && !textureGuid.empty())
        {
            const string guidCacheKey = Make_TexturePreviewGuidKey(textureGuid);
            const wstring guidTexturePath = GAME->Resolve_AssetPath(textureGuid);

            if (!guidTexturePath.empty())
            {
                if (ID3D11ShaderResourceView* guidSRV = Load_TexturePreview(guidCacheKey, guidTexturePath))
                    return guidSRV;
            }
            else if (!guidCacheKey.empty())
            {
                _texturePreviewFailedKeys.insert(guidCacheKey);
            }
        }

        if (!texturePath.empty())
        {
            const wstring resolvedTexturePath = Resolve_ClientRuntimeResourcePath(texturePath);
            const string pathCacheKey = Make_TexturePreviewPathKey(resolvedTexturePath);

            if (ID3D11ShaderResourceView* pathSRV = Load_TexturePreview(pathCacheKey, resolvedTexturePath))
                return pathSRV;
        }

        return nullptr;
    }

    ID3D11ShaderResourceView* UI_Editor_PreviewService::Load_TexturePreview(const string& cacheKey, const wstring& texturePath)
    {
        if (cacheKey.empty() || texturePath.empty())
            return nullptr;

        const auto cachedIter = _textureThumbCache.find(cacheKey);
        if (cachedIter != _textureThumbCache.end())
            return cachedIter->second.Get();

        if (_texturePreviewFailedKeys.find(cacheKey) != _texturePreviewFailedKeys.end())
            return nullptr;

        if (!Can_LoadTexturePreviewThisFrame())
            return nullptr;

        if (!fs::exists(texturePath))
        {
            LOG_WARN("UI Editor texture preview missing file: {}", String::ToString(texturePath));
            _texturePreviewFailedKeys.insert(cacheKey);
            return nullptr;
        }

        ++_texturePreviewLoadCountThisFrame;

        ComPtr<ID3D11ShaderResourceView> textureSRV = nullptr;
        const fs::path textureFilePath = fs::path(texturePath);
        const wstring extension = textureFilePath.extension().wstring();
        HRESULT hr = E_FAIL;

        if (extension == L".dds" || extension == L".DDS")
        {
            hr = DirectX::CreateDDSTextureFromFile(
                _device.Get(),
                texturePath.c_str(),
                nullptr,
                textureSRV.GetAddressOf());
        }
        else
        {
            hr = DirectX::CreateWICTextureFromFileEx(
                _device.Get(),
                texturePath.c_str(),
                0,
                D3D11_USAGE_DEFAULT,
                D3D11_BIND_SHADER_RESOURCE,
                0,
                0,
                DirectX::WIC_LOADER_IGNORE_SRGB,
                nullptr,
                textureSRV.GetAddressOf());
        }

        if (FAILED(hr) || nullptr == textureSRV)
        {
            LOG_WARN("UI Editor texture preview load failed. hr=0x{:08X}, path={}", static_cast<uint32>(hr), String::ToString(texturePath));
            _texturePreviewFailedKeys.insert(cacheKey);
            return nullptr;
        }

        _textureThumbCache.emplace(cacheKey, textureSRV);
        return _textureThumbCache[cacheKey].Get();
    }

    bool UI_Editor_PreviewService::Can_LoadTexturePreviewThisFrame()
    {
        constexpr int32 kMaxTexturePreviewLoadsPerFrame = 6;
        return _texturePreviewLoadCountThisFrame < kMaxTexturePreviewLoadsPerFrame;
    }
}

#include "BattleWorldUI_JsonTextureResolver.h"

#include "Texture.h"

NS_BEGIN(Client)

namespace
{
    bool Try_FindUIImageTexture(const json& node, const string& imageName, string& outGuid, string& outPath)
    {
        if (node.is_object())
        {
            if (node.value("name", string{}) == imageName)
            {
                outGuid = node.value("textureGuid", string{});
                outPath = node.value("texturePath", string{});
                return !outGuid.empty() || !outPath.empty();
            }

            for (const auto& item : node.items())
            {
                if (Try_FindUIImageTexture(item.value(), imageName, outGuid, outPath))
                    return true;
            }
        }
        else if (node.is_array())
        {
            for (const auto& item : node)
            {
                if (Try_FindUIImageTexture(item, imageName, outGuid, outPath))
                    return true;
            }
        }

        return false;
    }

    wstring Resolve_RuntimeResourcePath(const string& path)
    {
        if (path.empty())
            return {};

        string normalizedPath = path;
        replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');

        string loweredPath = normalizedPath;
        transform(loweredPath.begin(), loweredPath.end(), loweredPath.begin(),
            [](unsigned char value) { return static_cast<char>(tolower(value)); });

        string resourceRelativePath = normalizedPath;
        constexpr const char* resourcePrefix = "../bin/resources/";
        constexpr const char* binResourcePrefix = "bin/resources/";
        constexpr const char* resourcesPrefix = "resources/";

        auto startsWith = [](const string& text, const char* prefix)
            {
                return text.rfind(prefix, 0) == 0;
            };

        if (startsWith(loweredPath, resourcePrefix))
            resourceRelativePath = normalizedPath.substr(strlen(resourcePrefix));
        else if (startsWith(loweredPath, binResourcePrefix))
            resourceRelativePath = normalizedPath.substr(strlen(binResourcePrefix));
        else if (startsWith(loweredPath, resourcesPrefix))
            resourceRelativePath = normalizedPath.substr(strlen(resourcesPrefix));

        return (fs::path(GAME->Get_AssetRoot()) / fs::path(String::ToWString(resourceRelativePath))).lexically_normal().wstring();
    }

    wstring Resolve_TexturePath(const string& guid, const string& path)
    {
        if (!guid.empty())
        {
            const wstring guidPath = GAME->Resolve_AssetPath(guid);
            if (!guidPath.empty() && fs::exists(guidPath))
                return guidPath;
        }

        const wstring fallbackPath = Resolve_RuntimeResourcePath(path);
        if (!fallbackPath.empty() && fs::exists(fallbackPath))
            return fallbackPath;

        return {};
    }

    Shared<Texture> Load_TextureFromResolvedPath(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context,
        const wstring& texturePath)
    {
        if (texturePath.empty())
            return nullptr;

        return Texture::Create(device, context, texturePath.c_str(), 1);
    }
}

namespace BattleWorldUI_JsonTextureResolver
{
    Shared<Texture> Load_TextureFromUITextureRef(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context,
        const string& textureGuid,
        const string& texturePath,
        const string& debugName)
    {
        Shared<Texture> texture =
            Load_TextureFromResolvedPath(device, context, Resolve_TexturePath(textureGuid, texturePath));
        if (!texture)
        {
            LOG_WARN("BattleWorldUI json texture ref could not be resolved. name='{}', guid='{}', path='{}'",
                debugName,
                textureGuid,
                texturePath);
        }

        return texture;
    }

    Shared<Texture> Load_TextureFromUIImage(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context,
        const wstring& uiJsonPath,
        const string& imageName,
        const string& fallbackTextureGuid)
    {
        string textureGuid{};
        string texturePath{};

        const fs::path fullJsonPath = (fs::path(GAME->Get_AssetRoot()) / uiJsonPath).lexically_normal();
        if (fs::exists(fullJsonPath))
        {
            try
            {
                ifstream file(fullJsonPath);
                json root{};
                file >> root;
                (void)Try_FindUIImageTexture(root, imageName, textureGuid, texturePath);
            }
            catch (const exception& exception)
            {
                LOG_WARN("BattleWorldUI json texture lookup failed. image='{}', json='{}', error='{}'",
                    imageName,
                    String::ToString(fullJsonPath.wstring()),
                    exception.what());
            }
        }
        else
        {
            LOG_WARN("BattleWorldUI json texture lookup missing json. image='{}', json='{}'",
                imageName,
                String::ToString(fullJsonPath.wstring()));
        }

        Shared<Texture> texture =
            Load_TextureFromResolvedPath(device, context, Resolve_TexturePath(textureGuid, texturePath));
        if (texture)
            return texture;

        texture = Load_TextureFromResolvedPath(device, context, Resolve_TexturePath(fallbackTextureGuid, string{}));
        if (!texture)
        {
            LOG_WARN("BattleWorldUI fallback texture guid could not be resolved. image='{}', fallbackGuid='{}'",
                imageName,
                fallbackTextureGuid);
        }

        return texture;
    }
}

NS_END

#include "pch.h"
#include "UI_Editor_Utils.h"
#include "GameInstance.h"

namespace UIEditor
{
    namespace
    {
        bool StartsWith(const string& value, const string& prefix)
        {
            return value.size() >= prefix.size() && equal(prefix.begin(), prefix.end(), value.begin());
        }

        string NormalizePathKey(string path)
        {
            replace(path.begin(), path.end(), '\\', '/');
            return String::ToLowerCopy(path);
        }

        string MakeFontAssetPathKey(const AssetMeta& assetMeta)
        {
            string pathKey = Make_ClientRuntimeResourcePath(assetMeta);
            if (pathKey.empty())
                pathKey = String::ToString(assetMeta.fullPath);

            return NormalizePathKey(pathKey);
        }
    }

    const char* Get_ObjectTypeName(UI_ObjectType objectType)
    {
        switch (objectType)
        {
        case UI_ObjectType::Image:
            return "Image";
        case UI_ObjectType::Text:
            return "Text";
        case UI_ObjectType::Panel:
            return "Panel";
        case UI_ObjectType::Button:
            return "Button";
        default:
            return "Unknown";
        }
    }

    const char* Get_TextAlignName(UI_TextAlign textAlign)
    {
        switch (textAlign)
        {
        case UI_TextAlign::Left:
            return "Left";
        case UI_TextAlign::Center:
            return "Center";
        case UI_TextAlign::Right:
            return "Right";
        default:
            return "Left";
        }
    }

    ImU32 Get_ObjectFillColor(UI_ObjectType objectType)
    {
        switch (objectType)
        {
        case UI_ObjectType::Image:
            return IM_COL32(76, 125, 194, 110);
        case UI_ObjectType::Text:
            return IM_COL32(104, 178, 120, 110);
        case UI_ObjectType::Panel:
            return IM_COL32(125, 128, 136, 150);
        case UI_ObjectType::Button:
            return IM_COL32(90, 145, 210, 140);
        default:
            return IM_COL32(180, 180, 180, 110);
        }
    }

    bool Is_ContainerObjectType(UI_ObjectType objectType)
    {
        return objectType == UI_ObjectType::Panel || objectType == UI_ObjectType::Button;
    }

    string Normalize_AssetRelativePath(const AssetMeta& assetMeta)
    {
        string relativePath = String::ToString(assetMeta.relativePath);
        replace(relativePath.begin(), relativePath.end(), '\\', '/');
        return relativePath;
    }

    string Make_ClientRuntimeResourcePath(const AssetMeta& assetMeta)
    {
        const string relativePath = Normalize_AssetRelativePath(assetMeta);
        if (relativePath.empty())
            return {};

        return "../Bin/Resources/" + relativePath;
    }

    wstring Resolve_ClientRuntimeResourcePath(const string& runtimePath)
    {
        if (runtimePath.empty())
            return {};

        string normalizedPath = runtimePath;
        replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');

        fs::path inputPath = fs::path(String::ToWString(normalizedPath));
        if (inputPath.is_absolute())
            return inputPath.lexically_normal().wstring();

        const string resourcePrefix = "../Bin/Resources/";
        const string binResourcePrefix = "Bin/Resources/";
        const string resourcesPrefix = "Resources/";

        string resourceRelativePath = normalizedPath;
        if (StartsWith(resourceRelativePath, resourcePrefix))
            resourceRelativePath = resourceRelativePath.substr(resourcePrefix.size());
        else if (StartsWith(resourceRelativePath, binResourcePrefix))
            resourceRelativePath = resourceRelativePath.substr(binResourcePrefix.size());
        else if (StartsWith(resourceRelativePath, resourcesPrefix))
            resourceRelativePath = resourceRelativePath.substr(resourcesPrefix.size());

        const wstring assetRoot = GAME->Get_AssetRoot();
        if (assetRoot.empty())
            return {};

        return (fs::path(assetRoot) / fs::path(String::ToWString(resourceRelativePath))).lexically_normal().wstring();
    }

    string Get_AssetFileName(const AssetMeta& assetMeta)
    {
        if (!assetMeta.relativePath.empty())
            return String::ToString(fs::path(assetMeta.relativePath).filename().wstring());

        return String::ToString(fs::path(assetMeta.fullPath).filename().wstring());
    }

    bool Is_UITextureAsset(const AssetMeta& assetMeta)
    {
        if (assetMeta.type != "Texture")
            return false;

        const string relativePath = String::ToLowerCopy(Normalize_AssetRelativePath(assetMeta));
        return StartsWith(relativePath, "textures/ui/");
    }

    bool Is_UISpriteFontAsset(const AssetMeta& assetMeta)
    {
        if (assetMeta.type != "Font")
            return false;

        const string relativePath = String::ToLowerCopy(Normalize_AssetRelativePath(assetMeta));
        if (!StartsWith(relativePath, "fonts/"))
            return false;

        return String::ToLowerCopy(fs::path(relativePath).extension().string()) == ".spritefont";
    }

    vector<AssetMeta> Collect_UISpriteFontAssets(const string& searchText)
    {
        vector<AssetMeta> uiFontAssets;
        unordered_set<string> collectedPathKeys;

        const auto tryAddFontAsset = [&](const AssetMeta& assetMeta)
            {
                if (!Is_UISpriteFontAsset(assetMeta) || !Matches_Search(assetMeta, searchText))
                    return;

                const string pathKey = MakeFontAssetPathKey(assetMeta);
                if (pathKey.empty() || !collectedPathKeys.insert(pathKey).second)
                    return;

                uiFontAssets.push_back(assetMeta);
            };

        for (const AssetMeta& assetMeta : GAME->Get_AssetsByType("Font"))
            tryAddFontAsset(assetMeta);

        const wstring assetRoot = GAME->Get_AssetRoot();
        if (!assetRoot.empty())
        {
            const fs::path assetRootPath = fs::path(assetRoot);
            const fs::path fontDirectory = assetRootPath / L"Fonts";

            try
            {
                if (fs::exists(fontDirectory) && fs::is_directory(fontDirectory))
                {
                    for (const fs::directory_entry& entry : fs::directory_iterator(fontDirectory))
                    {
                        if (!entry.is_regular_file())
                            continue;

                        if (String::ToLowerCopy(entry.path().extension().string()) != ".spritefont")
                            continue;

                        AssetMeta fontAsset = {};
                        fontAsset.type = "Font";
                        fontAsset.fullPath = fs::absolute(entry.path()).lexically_normal().wstring();

                        try
                        {
                            fontAsset.relativePath = fs::relative(entry.path(), assetRootPath).lexically_normal().wstring();
                        }
                        catch (...)
                        {
                            fontAsset.relativePath = (fs::path(L"Fonts") / entry.path().filename()).wstring();
                        }

                        tryAddFontAsset(fontAsset);
                    }
                }
            }
            catch (...)
            {
            }
        }

        sort(uiFontAssets.begin(), uiFontAssets.end(), [](const AssetMeta& lhs, const AssetMeta& rhs)
            {
                return NormalizePathKey(Normalize_AssetRelativePath(lhs)) < NormalizePathKey(Normalize_AssetRelativePath(rhs));
            });

        return uiFontAssets;
    }

    string Get_FontAssetStableId(const AssetMeta& assetMeta)
    {
        if (!assetMeta.guid.empty())
            return assetMeta.guid;

        string stableId = Make_ClientRuntimeResourcePath(assetMeta);
        if (!stableId.empty())
            return stableId;

        return String::ToString(assetMeta.fullPath);
    }

    bool Is_FontAssetApplied(const UI_Object& object, const AssetMeta& assetMeta)
    {
        if (!object.fontGuid.empty() && !assetMeta.guid.empty())
            return object.fontGuid == assetMeta.guid;

        const string assetPath = Make_ClientRuntimeResourcePath(assetMeta);
        return !object.fontPath.empty() && !assetPath.empty() && NormalizePathKey(object.fontPath) == NormalizePathKey(assetPath);
    }

    string Make_FontTag(const AssetMeta& assetMeta)
    {
        const fs::path path = !assetMeta.relativePath.empty() ? fs::path(assetMeta.relativePath) : fs::path(assetMeta.fullPath);
        return String::ToString(path.stem().wstring());
    }

    bool Matches_Search(const AssetMeta& assetMeta, const string& searchText)
    {
        if (searchText.empty())
            return true;

        string haystack = Normalize_AssetRelativePath(assetMeta);
        haystack += " ";
        haystack += Get_AssetFileName(assetMeta);
        haystack += " ";
        haystack += Make_FontTag(assetMeta);
        haystack += " ";
        haystack += assetMeta.guid;
        haystack = String::ToLowerCopy(haystack);

        return haystack.find(searchText) != string::npos;
    }

    ImTextureID To_ImTextureID(ID3D11ShaderResourceView* srv)
    {
        return static_cast<ImTextureID>(reinterpret_cast<intptr_t>(srv));
    }

    float Get_UITextRuntimeScale(const UI_Object& object)
    {
        const float fontScale = max(4.f, object.fontSize) / 32.f;
        return max(0.01f, object.scale.x * fontScale);
    }

    ImVec2 Get_UIObjectRuntimeSize(const UI_Object& object)
    {
        if (object.type == UI_ObjectType::Text)
        {
            const float textScale = Get_UITextRuntimeScale(object);
            return ImVec2(
                max(1.f, object.size.x * textScale),
                max(1.f, object.size.y * textScale));
        }

        return ImVec2(
            max(1.f, object.size.x * object.scale.x),
            max(1.f, object.size.y * object.scale.y));
    }

    ImVec2 Clamp_ObjectPosition(const UI_Object& object, const ImVec2& canvasSize, const ImVec2& position)
    {
        const ImVec2 objectSize = Get_UIObjectRuntimeSize(object);

        const float maxX = max(0.f, canvasSize.x - objectSize.x);
        const float maxY = max(0.f, canvasSize.y - objectSize.y);

        return ImVec2(
            min(max(position.x, 0.f), maxX),
            min(max(position.y, 0.f), maxY));
    }

    bool Build_ScreenClipRect(const ImVec2& minPosition, const ImVec2& maxPosition, D3D11_RECT& outRect)
    {
        const float left = max(0.f, minPosition.x);
        const float top = max(0.f, minPosition.y);
        const float right = max(left, maxPosition.x);
        const float bottom = max(top, maxPosition.y);

        if (right <= left || bottom <= top)
            return false;

        outRect.left = static_cast<LONG>(left);
        outRect.top = static_cast<LONG>(top);
        outRect.right = static_cast<LONG>(right);
        outRect.bottom = static_cast<LONG>(bottom);

        return outRect.right > outRect.left && outRect.bottom > outRect.top;
    }

    wstring Build_SafeSpriteFontText(const SpriteFont& font, const wstring& text)
    {
        if (text.empty())
            return {};

        const bool hasFallback = font.ContainsCharacter(L'?');
        const bool hasSpace = font.ContainsCharacter(L' ');
        wstring safeText;
        safeText.reserve(text.size());

        for (const wchar_t character : text)
        {
            if (character == L'\r' || character == L'\n')
            {
                safeText.push_back(character);
                continue;
            }

            if (font.ContainsCharacter(character))
            {
                safeText.push_back(character);
                continue;
            }

            if (hasFallback)
                safeText.push_back(L'?');
            else if (hasSpace)
                safeText.push_back(L' ');
        }

        return safeText;
    }

    void Apply_TextureAsset(UI_Object& object, const AssetMeta& assetMeta)
    {
        object.textureGuid = assetMeta.guid;
        object.texturePath = Make_ClientRuntimeResourcePath(assetMeta);
    }

    void Apply_FontAsset(UI_Object& object, const AssetMeta& assetMeta)
    {
        object.fontGuid = assetMeta.guid;
        object.fontPath = Make_ClientRuntimeResourcePath(assetMeta);
        object.fontTag = Make_FontTag(assetMeta);
    }
}

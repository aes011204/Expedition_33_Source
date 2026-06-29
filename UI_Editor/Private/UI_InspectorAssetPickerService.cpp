#include "pch.h"
#include "UI_InspectorAssetPickerService.h"

#include "GameInstance.h"
#include "UI_Editor_Utils.h"

namespace UIEditor
{
    namespace
    {
        string Normalize_PathKey(string path)
        {
            replace(path.begin(), path.end(), '\\', '/');
            return String::ToLowerCopy(path);
        }

        bool Is_UITextureFileExtension(const fs::path& path)
        {
            const string extension = String::ToLowerCopy(path.extension().string());
            return extension == ".png" ||
                extension == ".jpg" ||
                extension == ".jpeg" ||
                extension == ".dds" ||
                extension == ".tga";
        }

        string Build_SearchHaystackLower(const AssetMeta& assetMeta, const string& displayName, const string& runtimePath, const string& fontTag)
        {
            string haystack = Normalize_AssetRelativePath(assetMeta);
            haystack += " ";
            haystack += displayName;
            haystack += " ";
            haystack += runtimePath;
            haystack += " ";
            haystack += fontTag;
            haystack += " ";
            haystack += assetMeta.guid;
            return String::ToLowerCopy(haystack);
        }

        string Make_StableId(const AssetMeta& assetMeta, const string& runtimePath)
        {
            if (!assetMeta.guid.empty())
                return assetMeta.guid;

            if (!runtimePath.empty())
                return runtimePath;

            return String::ToString(assetMeta.fullPath);
        }
    }

    void UI_InspectorAssetPickerService::Mark_TextureAssetsDirty()
    {
        _textureAssetsDirty = true;
        _filteredTextureAssetsDirty = true;
    }

    void UI_InspectorAssetPickerService::Mark_FontAssetsDirty()
    {
        _fontAssetsDirty = true;
        _filteredFontAssetsDirty = true;
    }

    void UI_InspectorAssetPickerService::Mark_AllDirty()
    {
        Mark_TextureAssetsDirty();
        Mark_FontAssetsDirty();
    }

    const vector<const UI_InspectorAssetEntry*>& UI_InspectorAssetPickerService::Get_FilteredTextureAssets(bool assetManagerReady, const string& searchText)
    {
        if (_textureAssetsDirty)
            Refresh_TextureAssetCache(assetManagerReady);

        Refresh_FilteredTextureAssets(String::ToLowerCopy(searchText));
        return _filteredTextureAssets;
    }

    const vector<const UI_InspectorAssetEntry*>& UI_InspectorAssetPickerService::Get_FilteredFontAssets(const string& searchText)
    {
        if (_fontAssetsDirty)
            Refresh_FontAssetCache();

        Refresh_FilteredFontAssets(String::ToLowerCopy(searchText));
        return _filteredFontAssets;
    }

    bool UI_InspectorAssetPickerService::Is_TextureEntryApplied(const string& textureGuid, const string& texturePath, const UI_InspectorAssetEntry& entry) const
    {
        if (!textureGuid.empty() && !entry.assetMeta.guid.empty())
            return textureGuid == entry.assetMeta.guid;

        return !texturePath.empty() &&
            !entry.runtimePath.empty() &&
            Normalize_PathKey(texturePath) == Normalize_PathKey(entry.runtimePath);
    }

    bool UI_InspectorAssetPickerService::Is_FontEntryApplied(const UI_Object& object, const UI_InspectorAssetEntry& entry) const
    {
        if (!object.fontGuid.empty() && !entry.assetMeta.guid.empty())
            return object.fontGuid == entry.assetMeta.guid;

        return !object.fontPath.empty() &&
            !entry.runtimePath.empty() &&
            Normalize_PathKey(object.fontPath) == Normalize_PathKey(entry.runtimePath);
    }

    void UI_InspectorAssetPickerService::Refresh_TextureAssetCache(bool assetManagerReady)
    {
        _textureAssets.clear();
        _filteredTextureAssets.clear();
        _lastTextureSearchText.clear();
        _filteredTextureAssetsDirty = true;

        if (assetManagerReady)
        {
            for (const AssetMeta& assetMeta : GAME->Get_AssetsByType("Texture"))
            {
                if (Is_UITextureAsset(assetMeta))
                    _textureAssets.push_back(Make_TextureEntry(assetMeta));
            }
        }

        if (_textureAssets.empty())
        {
            const wstring resourceRoot = Resolve_UIEditorClientResourceRoot();
            const fs::path uiTextureRoot = fs::path(resourceRoot) / L"Textures" / L"UI";

            try
            {
                if (!resourceRoot.empty() && fs::exists(uiTextureRoot) && fs::is_directory(uiTextureRoot))
                {
                    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(uiTextureRoot))
                    {
                        if (!entry.is_regular_file() || !Is_UITextureFileExtension(entry.path()))
                            continue;

                        AssetMeta textureAsset = {};
                        textureAsset.type = "Texture";
                        textureAsset.fullPath = fs::absolute(entry.path()).lexically_normal().wstring();
                        textureAsset.relativePath = fs::relative(entry.path(), fs::path(resourceRoot)).lexically_normal().wstring();

                        _textureAssets.push_back(Make_TextureEntry(textureAsset));
                    }
                }
            }
            catch (...)
            {
            }
        }

        _textureAssetsDirty = false;
    }

    void UI_InspectorAssetPickerService::Refresh_FilteredTextureAssets(const string& searchText)
    {
        if (!_filteredTextureAssetsDirty && _lastTextureSearchText == searchText)
            return;

        _filteredTextureAssets.clear();
        _filteredTextureAssets.reserve(_textureAssets.size());

        for (const UI_InspectorAssetEntry& entry : _textureAssets)
        {
            if (searchText.empty() || entry.searchHaystackLower.find(searchText) != string::npos)
                _filteredTextureAssets.push_back(&entry);
        }

        _lastTextureSearchText = searchText;
        _filteredTextureAssetsDirty = false;
    }

    void UI_InspectorAssetPickerService::Refresh_FontAssetCache()
    {
        _fontAssets.clear();
        _filteredFontAssets.clear();
        _lastFontSearchText.clear();
        _filteredFontAssetsDirty = true;

        const vector<AssetMeta> fontAssets = Collect_UISpriteFontAssets("");
        _fontAssets.reserve(fontAssets.size());
        for (const AssetMeta& assetMeta : fontAssets)
            _fontAssets.push_back(Make_FontEntry(assetMeta));

        _fontAssetsDirty = false;
    }

    void UI_InspectorAssetPickerService::Refresh_FilteredFontAssets(const string& searchText)
    {
        if (!_filteredFontAssetsDirty && _lastFontSearchText == searchText)
            return;

        _filteredFontAssets.clear();
        _filteredFontAssets.reserve(_fontAssets.size());

        for (const UI_InspectorAssetEntry& entry : _fontAssets)
        {
            if (searchText.empty() || entry.searchHaystackLower.find(searchText) != string::npos)
                _filteredFontAssets.push_back(&entry);
        }

        _lastFontSearchText = searchText;
        _filteredFontAssetsDirty = false;
    }

    UI_InspectorAssetEntry UI_InspectorAssetPickerService::Make_TextureEntry(const AssetMeta& assetMeta)
    {
        UI_InspectorAssetEntry entry = {};
        entry.assetMeta = assetMeta;
        entry.displayName = Get_AssetFileName(assetMeta);
        entry.runtimePath = Make_ClientRuntimeResourcePath(assetMeta);
        entry.fontTag = Make_FontTag(assetMeta);
        entry.stableId = Make_StableId(assetMeta, entry.runtimePath);
        entry.searchHaystackLower = Build_SearchHaystackLower(assetMeta, entry.displayName, entry.runtimePath, entry.fontTag);
        return entry;
    }

    UI_InspectorAssetEntry UI_InspectorAssetPickerService::Make_FontEntry(const AssetMeta& assetMeta)
    {
        UI_InspectorAssetEntry entry = {};
        entry.assetMeta = assetMeta;
        entry.displayName = Get_AssetFileName(assetMeta);
        entry.runtimePath = Make_ClientRuntimeResourcePath(assetMeta);
        entry.fontTag = Make_FontTag(assetMeta);
        entry.stableId = Get_FontAssetStableId(assetMeta);
        entry.searchHaystackLower = Build_SearchHaystackLower(assetMeta, entry.displayName, entry.runtimePath, entry.fontTag);
        return entry;
    }
}

#pragma once

#include "UI_Document.h"

namespace UIEditor
{
    struct UI_InspectorAssetEntry
    {
        AssetMeta assetMeta = {};
        string stableId = "";
        string displayName = "";
        string runtimePath = "";
        string searchHaystackLower = "";
        string fontTag = "";
    };

    class UI_InspectorAssetPickerService final
    {
    public:
        void Mark_TextureAssetsDirty();
        void Mark_FontAssetsDirty();
        void Mark_AllDirty();

        const vector<const UI_InspectorAssetEntry*>& Get_FilteredTextureAssets(bool assetManagerReady, const string& searchText);
        const vector<const UI_InspectorAssetEntry*>& Get_FilteredFontAssets(const string& searchText);

        bool Is_TextureEntryApplied(const string& textureGuid, const string& texturePath, const UI_InspectorAssetEntry& entry) const;
        bool Is_FontEntryApplied(const UI_Object& object, const UI_InspectorAssetEntry& entry) const;

    private:
        void Refresh_TextureAssetCache(bool assetManagerReady);
        void Refresh_FilteredTextureAssets(const string& searchText);
        void Refresh_FontAssetCache();
        void Refresh_FilteredFontAssets(const string& searchText);

        static UI_InspectorAssetEntry Make_TextureEntry(const AssetMeta& assetMeta);
        static UI_InspectorAssetEntry Make_FontEntry(const AssetMeta& assetMeta);

    private:
        vector<UI_InspectorAssetEntry> _textureAssets = {};
        vector<const UI_InspectorAssetEntry*> _filteredTextureAssets = {};
        vector<UI_InspectorAssetEntry> _fontAssets = {};
        vector<const UI_InspectorAssetEntry*> _filteredFontAssets = {};
        string _lastTextureSearchText = "";
        string _lastFontSearchText = "";
        bool _textureAssetsDirty = true;
        bool _filteredTextureAssetsDirty = true;
        bool _fontAssetsDirty = true;
        bool _filteredFontAssetsDirty = true;
    };
}

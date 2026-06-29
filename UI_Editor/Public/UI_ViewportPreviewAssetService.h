#pragma once

#include "UI_SpriteVfx.h"

namespace UIEditor
{
    class UI_ViewportPreviewAssetService final
    {
    public:
        bool Try_LoadUIVfxAsset(const string& vfxPath, Client::UISpriteVfxAsset& outAsset);
        void Clear();

    private:
        struct CachedUIVfxAsset
        {
            Client::UISpriteVfxAsset asset = {};
            fs::file_time_type writeTime = {};
            double lastCheckedTimeSeconds = 0.0;
            bool loaded = false;
        };

    private:
        static string Make_NormalizedPathKey(const wstring& path);

    private:
        unordered_map<string, CachedUIVfxAsset> _uiVfxAssetCache = {};
    };
}

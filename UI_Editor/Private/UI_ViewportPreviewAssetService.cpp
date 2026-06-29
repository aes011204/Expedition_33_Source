#include "pch.h"
#include "UI_ViewportPreviewAssetService.h"

#include "UI_Editor_Utils.h"

namespace UIEditor
{
    namespace
    {
        constexpr double kUIVfxFileCheckIntervalSeconds = 0.25;
    }

    string UI_ViewportPreviewAssetService::Make_NormalizedPathKey(const wstring& path)
    {
        wstring normalizedPath = path;
        replace(normalizedPath.begin(), normalizedPath.end(), L'\\', L'/');
        transform(normalizedPath.begin(), normalizedPath.end(), normalizedPath.begin(), [](wchar_t character)
            {
                return static_cast<wchar_t>(towlower(character));
            });

        return String::ToString(normalizedPath);
    }

    bool UI_ViewportPreviewAssetService::Try_LoadUIVfxAsset(const string& vfxPath, Client::UISpriteVfxAsset& outAsset)
    {
        if (vfxPath.empty())
            return false;

        const wstring resolvedPath = Resolve_ClientRuntimeResourcePath(vfxPath);
        if (resolvedPath.empty() || !fs::exists(resolvedPath))
            return false;

        const string cacheKey = Make_NormalizedPathKey(resolvedPath);
        const double nowSeconds = ImGui::GetTime();

        auto cachedIter = _uiVfxAssetCache.find(cacheKey);
        if (cachedIter != _uiVfxAssetCache.end() &&
            cachedIter->second.loaded &&
            nowSeconds - cachedIter->second.lastCheckedTimeSeconds < kUIVfxFileCheckIntervalSeconds)
        {
            outAsset = cachedIter->second.asset;
            return true;
        }

        fs::file_time_type writeTime = {};
        try
        {
            writeTime = fs::last_write_time(resolvedPath);
        }
        catch (...)
        {
        }

        if (cachedIter != _uiVfxAssetCache.end() &&
            cachedIter->second.loaded &&
            cachedIter->second.writeTime == writeTime)
        {
            cachedIter->second.lastCheckedTimeSeconds = nowSeconds;
            outAsset = cachedIter->second.asset;
            return true;
        }

        Client::UISpriteVfxAsset asset = {};
        if (!Client::UISpriteVfxSerializer::Load_FromFile(resolvedPath, asset))
            return false;

        CachedUIVfxAsset cachedAsset = {};
        cachedAsset.asset = asset;
        cachedAsset.writeTime = writeTime;
        cachedAsset.lastCheckedTimeSeconds = nowSeconds;
        cachedAsset.loaded = true;
        _uiVfxAssetCache[cacheKey] = cachedAsset;

        outAsset = asset;
        return true;
    }

    void UI_ViewportPreviewAssetService::Clear()
    {
        _uiVfxAssetCache.clear();
    }
}

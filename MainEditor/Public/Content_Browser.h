#pragma once

#include "Editor_Window.h"
#include "ContentBrowserCore.h"

NS_BEGIN(Engine)
class Texture;
NS_END

NS_BEGIN(MainEditor)

class Content_Browser final : public Editor_Window, private ContentBrowserCore
{
private:
    enum class EIconType
    {
        Folder,
        Csv,
        Json,
        Python,
        Xlsx,
        END
    };

private:
    using FFileEntry = ContentBrowserFileEntry;
    using FFolderNode = ContentBrowserFolderNode;
    using FThumbnailPolicy = ContentBrowserThumbnailPolicy;
    using FGridItemRef = ContentBrowserGridItemRef;

    struct FPrefabCreateEntry
    {
        string className{};
        string displayName{};
        string category{};
    };

public:
    Content_Browser();
    ~Content_Browser() override;

public:
    void Update(float timeDelta) override;
    void Render() override;

private:
    void Refresh_Resources();
    void Refresh_CurrentFolder();
    void Refresh_CurrentFolderAssetState();
    void Load_Icons();

    void Draw_FolderTree(FFolderNode& node);
    void Draw_AssetView();
    void Draw_ThumbnailSettingsPopup();

    void Draw_FolderTile(FFolderNode& folder);
    void Open_Folder(FFolderNode* folder);
    void Draw_FolderContextMenu(FFolderNode& folder, const string& popupId);
    void Draw_FileContextMenu(const wstring& filePath, const string& popupId);
    void Draw_CreatePrefabPopup();
    void Open_CreatePrefabPopup(const wstring& targetFolderPath);
    void Rebuild_CreatePrefabEntries();
    Shared<Texture> Get_FileThumbnail(const FFileEntry& fileEntry, bool allowLoad);
    bool Is_LargeTextureFolder() const;
    int Resolve_CurrentThumbnailLoadBudget() const;

    void Save_Settings() const;
    void Load_Settings();

    EIconType Resolve_IconType(const filesystem::path& filePath) const;
    ImTextureID Get_IconTextureId(EIconType iconType) const;

    bool Can_GoParentFolder() const;
    void Go_ParentFolder();
    void Go_BackFolder();
    void Go_ForwardFolder();
    void Refresh_AssetState();
    void Draw_RefreshPopup();
    void Handle_SideButtonEvent();
    void Handle_ThumbnailZoom();

private:
    float _leftPanelWidth = 300.f;
    float _thumbnailSize = 64.f;
    char _searchBuffer[128] = "";
    char _assetSearchBuffer[128] = "";

    vector<Shared<Texture>> _iconFiles{};
    map<wstring, Shared<Texture>> _thumbnailCache{};
    set<wstring> _noThumbnailPaths{};
    FThumbnailPolicy _thumbnailPolicy{};
    int _thumbnailLoadsThisFrame{ 0 };
    int _thumbnailLoadBudgetThisFrame{ 4 };

    FFolderNode* _gridItemCacheFolder{};
    size_t _gridItemCacheSubFolderCount{};
    size_t _gridItemCacheFileCount{};
    string _gridItemCacheSearchLower{};
    vector<FGridItemRef> _gridItemCache{};

    bool _needsInitialRefresh = true;

    wstring _selectedFilePath{};
    vector<FPrefabCreateEntry> _createPrefabEntries{};
    wstring _createPrefabTargetFolder{};
    string _selectedPrefabClass{};
    char _createPrefabNameBuffer[128] = "";
    char _createPrefabSearchBuffer[128] = "";
    bool _openCreatePrefabPopup = false;

    static constexpr const wchar_t* CONTENT_BROWSER_SETTINGS_PATH =
        L"../../../Client/Bin/Resources/Data/json/EditorSettings/ContentBrowser.json";

public:
    static Shared<Content_Browser> Create();
};

NS_END

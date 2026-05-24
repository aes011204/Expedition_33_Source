#pragma once

#include "Editor_Window.h"

NS_BEGIN(Engine)
class Texture;
NS_END

NS_BEGIN(MainEditor)

class Content_Browser final : public Editor_Window
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
    struct FFileEntry
    {
        wstring filePath{};
        string guid{};
    };

    struct FFolderNode
    {
        wstring name{};
        wstring fullPath{};

        vector<FFolderNode> subFolders{};
        vector<FFileEntry> files{};
    };

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
    void Load_Icons();

    void Scan_Folder(const wstring& path, FFolderNode& node);
    void Draw_FolderTree(FFolderNode& node);
    void Draw_AssetView();

    void Draw_FolderTile(FFolderNode& folder);
    void Draw_FileTile(const FFileEntry& fileEntry);
    void Open_Folder(FFolderNode* folder);
    void Draw_FolderContextMenu(FFolderNode& folder, const string& popupId);
    void Draw_FileContextMenu(const wstring& filePath, const string& popupId);
    void Draw_CreatePrefabPopup();
    void Open_CreatePrefabPopup(const wstring& targetFolderPath);
    void Rebuild_CreatePrefabEntries();
    Shared<Texture> Get_FileThumbnail(const FFileEntry& fileEntry);
    static bool Is_ThumbnailFileExtension(const filesystem::path& filePath);

    void Save_Settings() const;
    void Load_Settings();

    FFolderNode* Find_FolderNode(FFolderNode& node, const wstring& path);
    void Expand_PathTo(const wstring& targetPath);
    static bool IsFolderMatchingSearch(const FFolderNode& node, const string& searchStr);
    FFolderNode* Get_FirstMatchingFolder(FFolderNode& node, const string& searchStr);
    EIconType Resolve_IconType(const filesystem::path& filePath) const;
    ImTextureID Get_IconTextureId(EIconType iconType) const;

    void Record_FolderHistory(const wstring& path);
    bool Can_GoBack() const;
    bool Can_GoForward() const;
    void Go_BackFolder();
    void Go_ForwardFolder();
    void Refresh_AssetState();
    void Handle_SideButtonEvent();
    void Handle_ThumbnailZoom();

private:
    FFolderNode _rootFolder{};
    FFolderNode* _currentFolder = nullptr;

    filesystem::path _resourceRoot{};

    float _leftPanelWidth = 300.f;
    float _thumbnailSize = 64.f;
    char _searchBuffer[128] = "";
    char _assetSearchBuffer[128] = "";

    vector<Shared<Texture>> _iconFiles{};
    map<wstring, Shared<Texture>> _thumbnailCache{};
    set<wstring> _noThumbnailPaths{};
    int _thumbnailLoadsThisFrame = 0;

    set<wstring> _expandedFolders{};
    vector<wstring> _folderHistory{};
    int _folderHistoryIndex = -1;
    bool _suppressHistoryRecord = false;

    wstring _selectedFilePath{};
    vector<FPrefabCreateEntry> _createPrefabEntries{};
    wstring _createPrefabTargetFolder{};
    string _selectedPrefabClass{};
    char _createPrefabNameBuffer[128] = "";
    char _createPrefabSearchBuffer[128] = "";
    bool _openCreatePrefabPopup = false;

    static constexpr const wchar_t* CONTENT_BROWSER_SETTINGS_PATH =
        L"../../../Client/Bin/Resources/Data/json/EditorSettings/ContentBrowser.json";
    static constexpr int MAX_THUMBNAIL_LOADS_PER_FRAME = 2;

public:
    static Shared<Content_Browser> Create();
};

NS_END

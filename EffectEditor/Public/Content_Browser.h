#pragma once

#include "Editor_Window.h"

NS_BEGIN(Engine)
class Texture;
NS_END

NS_BEGIN(EffectEditor)

class Content_Browser final : public Editor_Window
{
public:
    Content_Browser();
    ~Content_Browser() override;

public:
    void Update(float timeDelta) override;
    void Render() override;

private: //## Types
    enum class IconType
    {
        Folder,
        Csv,
        Json,
        Python,
        Xlsx,
        End
    };

    struct FileEntry
    {
        wstring filePath{};
        string guid{};
    };

    struct FolderNode
    {
        wstring name{};
        wstring fullPath{};

        vector<FolderNode> subFolders{};
        vector<FileEntry> files{};
    };

private: //## Static Values
    static constexpr const wchar_t* kContentBrowserSettingsPath{
        L"../../../Client/Bin/Resources/Data/json/EditorSettings/ContentBrowser.json"
    };

private: //## Data
    FolderNode  _rootFolder{};
    FolderNode* _currentFolder{ nullptr };

    filesystem::path _resourceRoot{};

    float _leftPanelWidth{ 300.f };
    float _thumbnailSize{ 64.f };
    char _searchBuffer[128]{};
    char _assetSearchBuffer[128]{};

    vector<Shared<Texture>> _iconFiles{};
    map<wstring, Shared<Texture>> _thumbnailCache{};
    set<wstring> _noThumbnailPaths{};

    set<wstring> _expandedFolders{};
    vector<wstring> _folderHistory{};
    int _folderHistoryIndex{ -1 };
    bool _suppressHistoryRecord{ false };
    bool _needsInitialRefresh{ true };

    wstring _selectedFilePath{};

private: //## Helper::Refresh
    void Refresh_Resources();
    void Refresh_CurrentFolder();
    void Load_Icons();

private: //## Helper::Scan
    void Scan_Folder(const wstring& path, FolderNode& node);

private: //## Helper::View
    void Draw_FolderTree(FolderNode& node);
    void Draw_AssetView();
    void Draw_FolderTile(FolderNode& folder);
    void Open_Folder(FolderNode* folder);
    void Open_File(const FileEntry& fileEntry);
    void Draw_FolderContextMenu(const FolderNode& folder, const string& popupId);
    void Draw_FileContextMenu(const wstring& filePath, const string& popupId);

private: //## Helper::ThumbnailIcon
    Shared<Texture> Get_FileThumbnail(const FileEntry& fileEntry);
    static bool Is_ThumbnailFileExtension(const filesystem::path& filePath);
    static bool Is_EffectMaterialPresetFile(const filesystem::path& filePath);

private: //## Helper::SettingsSearch
    void Save_Settings() const;
    void Load_Settings();
    FolderNode* Find_FolderNode(FolderNode& node, const wstring& path);
    void Expand_PathTo(const wstring& targetPath);
    static bool IsFolderMatchingSearch(const FolderNode& node, const string& searchStr);
    FolderNode* Get_FirstMatchingFolder(FolderNode& node, const string& searchStr);
    IconType Resolve_IconType(const filesystem::path& filePath) const;
    ImTextureID Get_IconTextureId(IconType iconType) const;

private: //## Helper::NavigationHistory
    void Record_FolderHistory(const wstring& path);
    bool Can_GoBack() const;
    bool Can_GoForward() const;
    void Go_BackFolder();
    void Go_ForwardFolder();
    void Handle_SideButtonEvent();
    void Handle_ThumbnailZoom();

public:
    static Shared<Content_Browser> Create();
    void Free() override;
};

NS_END

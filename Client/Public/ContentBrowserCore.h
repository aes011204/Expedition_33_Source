#pragma once

#include "Client_Defines.h"

NS_BEGIN(Client)

struct ContentBrowserFileEntry
{
    wstring filePath{};
    string guid{};
};

struct ContentBrowserFolderNode
{
    wstring name{};
    wstring fullPath{};

    vector<ContentBrowserFolderNode> subFolders{};
    vector<ContentBrowserFileEntry> files{};
};

struct ContentBrowserThumbnailPolicy
{
    bool textureThumbnails{ true };
    bool showDdsThumbnails{ false };
    bool adaptiveLargeFolders{ true };
    int largeFolderThreshold{ 100 };
    int thumbnailLoadBudget{ 4 };
    int largeFolderThumbnailLoadBudget{ 0 };
};

enum class ContentBrowserGridItemType
{
    Folder,
    File
};

struct ContentBrowserGridItemRef
{
    ContentBrowserGridItemType type{};
    size_t index{};
};

struct ContentBrowserGridLayout
{
    float tileWidth{};
    float tileHeight{};
    int columnCount{ 1 };
    int rowCount{};
};

struct ContentBrowserGridVisibleRange
{
    bool hasItems{};
    int firstRow{};
    int lastRow{};
    size_t firstItem{};
    size_t lastItem{};
};

class ContentBrowserCore
{
protected:
    using FileEntry = ContentBrowserFileEntry;
    using FolderNode = ContentBrowserFolderNode;
    using ThumbnailPolicy = ContentBrowserThumbnailPolicy;
    using GridItemRef = ContentBrowserGridItemRef;
    using GridLayout = ContentBrowserGridLayout;
    using GridVisibleRange = ContentBrowserGridVisibleRange;

protected:
    FolderNode _rootFolder{};
    FolderNode* _currentFolder{ nullptr };
    filesystem::path _resourceRoot{};

    set<wstring> _expandedFolders{};
    vector<wstring> _folderHistory{};
    int _folderHistoryIndex{ -1 };
    bool _suppressHistoryRecord{ false };

protected:
    void Set_ResourceRoot(const filesystem::path& resourceRoot);
    const filesystem::path& Get_ResourceRoot() const;

    void Refresh_BrowserResources(const function<string(const wstring&)>& resolveGuid);
    void Refresh_CurrentFolder(const function<string(const wstring&)>& resolveGuid);
    void Scan_Folder(const wstring& path, FolderNode& node, const function<string(const wstring&)>& resolveGuid);

    FolderNode* Find_FolderNode(FolderNode& node, const wstring& path);
    void Expand_PathTo(const wstring& targetPath);
    static bool IsFolderMatchingSearch(const FolderNode& node, const string& searchStr);
    FolderNode* Get_FirstMatchingFolder(FolderNode& node, const string& searchStr);

    void Record_FolderHistory(const wstring& path);
    bool Can_GoBack() const;
    bool Can_GoForward() const;

    void Save_CurrentPathSetting(const filesystem::path& settingsPath, const string& editorKey) const;
    void Load_CurrentPathSetting(const filesystem::path& settingsPath, const string& editorKey);

    static string ToLowerCopy(string value);
    static bool Is_ThumbnailFileExtension(const filesystem::path& filePath);
    static bool Is_TextureThumbnailCandidate(const filesystem::path& filePath);
    static bool Is_DdsFile(const filesystem::path& filePath);
    static bool Should_RequestThumbnail(const FileEntry& fileEntry, bool isTileVisible, int loadsThisFrame, int budgetThisFrame);
    static void Build_GridItems(
        const FolderNode& folder,
        const string& searchLower,
        const function<bool(const FolderNode&, const string&)>& includeFolder,
        const function<bool(const FileEntry&, const string&)>& includeFile,
        vector<GridItemRef>& gridItems);
    static GridLayout Calculate_GridLayout(float panelWidth, float thumbnailSize, float textLineHeight, size_t itemCount);
    static GridVisibleRange Calculate_GridVisibleRange(
        const GridLayout& layout,
        float scrollY,
        float viewportHeight,
        size_t itemCount,
        int overscanRows);
    static bool Is_LargeTextureFolder(const FolderNode* folder, const ThumbnailPolicy& policy);
    static int Resolve_ThumbnailLoadBudget(const FolderNode* folder, const ThumbnailPolicy& policy);
    static void Clamp_ThumbnailPolicy(ThumbnailPolicy& policy);
    static json Read_SettingsFile(const filesystem::path& settingsPath);
    static void Write_SettingsFile(const filesystem::path& settingsPath, const json& settings);
    static void Save_ThumbnailPolicySetting(
        const filesystem::path& settingsPath,
        const string& editorKey,
        const ThumbnailPolicy& policy);
    static void Load_ThumbnailPolicySetting(
        const filesystem::path& settingsPath,
        const string& editorKey,
        ThumbnailPolicy& policy);
};

NS_END

#include "pch.h"
#include "ContentBrowserCore.h"

#include "Helper_String.h"

#include <fstream>

namespace fs = std::filesystem;

NS_BEGIN(Client)

string ContentBrowserCore::ToLowerCopy(string value)
{
    transform(value.begin(), value.end(), value.begin(), ::tolower);
    return value;
}

json ContentBrowserCore::Read_SettingsFile(const fs::path& settingsPath)
{
    ifstream file(settingsPath);
    if (!file.is_open())
        return {};

    json settings{};
    file >> settings;
    return settings;
}

void ContentBrowserCore::Write_SettingsFile(const fs::path& settingsPath, const json& settings)
{
    if (!fs::exists(settingsPath.parent_path()))
        fs::create_directories(settingsPath.parent_path());

    ofstream file(settingsPath);
    if (!file.is_open())
        return;

    file << settings.dump(4);
}

bool ContentBrowserCore::Is_ThumbnailFileExtension(const fs::path& filePath)
{
    const string extension = ToLowerCopy(String::ToString(filePath.extension().wstring()));

    return extension == ".png" ||
           extension == ".jpg" ||
           extension == ".jpeg" ||
           extension == ".bmp" ||
           extension == ".dds";
}

bool ContentBrowserCore::Is_TextureThumbnailCandidate(const fs::path& filePath)
{
    return Is_ThumbnailFileExtension(filePath);
}

bool ContentBrowserCore::Is_DdsFile(const fs::path& filePath)
{
    return ToLowerCopy(String::ToString(filePath.extension().wstring())) == ".dds";
}

bool ContentBrowserCore::Should_RequestThumbnail(
    const FileEntry& fileEntry,
    bool isTileVisible,
    int loadsThisFrame,
    int budgetThisFrame)
{
    if (!isTileVisible)
        return false;

    if (fileEntry.filePath.empty())
        return false;

    return loadsThisFrame < budgetThisFrame;
}

void ContentBrowserCore::Build_GridItems(
    const FolderNode& folder,
    const string& searchLower,
    const function<bool(const FolderNode&, const string&)>& includeFolder,
    const function<bool(const FileEntry&, const string&)>& includeFile,
    vector<GridItemRef>& gridItems)
{
    gridItems.clear();
    gridItems.reserve(folder.subFolders.size() + folder.files.size());

    for (size_t index = 0; index < folder.subFolders.size(); ++index)
    {
        const FolderNode& childFolder = folder.subFolders[index];
        if (includeFolder && !includeFolder(childFolder, searchLower))
            continue;

        gridItems.push_back(GridItemRef{ ContentBrowserGridItemType::Folder, index });
    }

    for (size_t index = 0; index < folder.files.size(); ++index)
    {
        const FileEntry& fileEntry = folder.files[index];
        if (includeFile && !includeFile(fileEntry, searchLower))
            continue;

        gridItems.push_back(GridItemRef{ ContentBrowserGridItemType::File, index });
    }
}

ContentBrowserCore::GridLayout ContentBrowserCore::Calculate_GridLayout(
    float panelWidth,
    float thumbnailSize,
    float textLineHeight,
    size_t itemCount)
{
    GridLayout layout{};
    layout.tileWidth = thumbnailSize + 40.f;
    layout.tileHeight = thumbnailSize + textLineHeight * 2.f + 12.f;
    layout.columnCount = max(1, static_cast<int>(panelWidth / layout.tileWidth));
    layout.rowCount = itemCount == 0 ?
        0 :
        static_cast<int>((itemCount + static_cast<size_t>(layout.columnCount) - 1) / static_cast<size_t>(layout.columnCount));
    return layout;
}

ContentBrowserCore::GridVisibleRange ContentBrowserCore::Calculate_GridVisibleRange(
    const GridLayout& layout,
    float scrollY,
    float viewportHeight,
    size_t itemCount,
    int overscanRows)
{
    GridVisibleRange range{};
    if (itemCount == 0 || layout.rowCount <= 0 || layout.columnCount <= 0 || layout.tileHeight <= 0.f)
        return range;

    overscanRows = max(0, overscanRows);
    range.firstRow = max(0, static_cast<int>(floor(scrollY / layout.tileHeight)) - overscanRows);

    const int visibleRowCount = static_cast<int>(ceil(viewportHeight / layout.tileHeight)) + overscanRows * 2 + 1;
    range.lastRow = min(layout.rowCount - 1, range.firstRow + visibleRowCount - 1);

    range.firstItem = static_cast<size_t>(range.firstRow * layout.columnCount);
    range.lastItem = min(itemCount - 1, static_cast<size_t>((range.lastRow + 1) * layout.columnCount) - 1);
    range.hasItems = range.firstItem <= range.lastItem;
    return range;
}

bool ContentBrowserCore::Is_LargeTextureFolder(const FolderNode* folder, const ThumbnailPolicy& policy)
{
    if (!folder || !policy.adaptiveLargeFolders)
        return false;

    int textureFileCount = 0;
    for (const FileEntry& fileEntry : folder->files)
    {
        if (Is_TextureThumbnailCandidate(fileEntry.filePath))
            ++textureFileCount;
    }

    return textureFileCount >= policy.largeFolderThreshold;
}

int ContentBrowserCore::Resolve_ThumbnailLoadBudget(const FolderNode* folder, const ThumbnailPolicy& policy)
{
    return Is_LargeTextureFolder(folder, policy) ?
        policy.largeFolderThumbnailLoadBudget :
        policy.thumbnailLoadBudget;
}

void ContentBrowserCore::Clamp_ThumbnailPolicy(ThumbnailPolicy& policy)
{
    policy.largeFolderThreshold = clamp(policy.largeFolderThreshold, 1, 10000);
    policy.thumbnailLoadBudget = clamp(policy.thumbnailLoadBudget, 1, 64);
    policy.largeFolderThumbnailLoadBudget = clamp(policy.largeFolderThumbnailLoadBudget, 1, 64);
}

void ContentBrowserCore::Save_ThumbnailPolicySetting(
    const fs::path& settingsPath,
    const string& editorKey,
    const ThumbnailPolicy& policy)
{
    json settings = Read_SettingsFile(settingsPath);
    if (!settings.is_object())
        settings = json::object();

    if (!settings.contains(editorKey) || !settings[editorKey].is_object())
        settings[editorKey] = json::object();

    json& policyRoot = settings[editorKey]["thumbnailPolicy"];
    policyRoot["textureThumbnails"] = policy.textureThumbnails;
    policyRoot["showDdsThumbnails"] = policy.showDdsThumbnails;
    policyRoot["adaptiveLargeFolders"] = policy.adaptiveLargeFolders;
    policyRoot["largeFolderThreshold"] = policy.largeFolderThreshold;
    policyRoot["thumbnailLoadBudget"] = policy.thumbnailLoadBudget;
    policyRoot["largeFolderThumbnailLoadBudget"] = policy.largeFolderThumbnailLoadBudget;

    Write_SettingsFile(settingsPath, settings);
}

void ContentBrowserCore::Load_ThumbnailPolicySetting(
    const fs::path& settingsPath,
    const string& editorKey,
    ThumbnailPolicy& policy)
{
    json settings = Read_SettingsFile(settingsPath);
    if (!settings.is_object())
        return;

    const json* policyRoot = nullptr;
    if (settings.contains(editorKey) &&
        settings[editorKey].is_object() &&
        settings[editorKey].contains("thumbnailPolicy") &&
        settings[editorKey]["thumbnailPolicy"].is_object())
    {
        policyRoot = &settings[editorKey]["thumbnailPolicy"];
    }

    if (!policyRoot)
        return;

    if (policyRoot->contains("textureThumbnails") && (*policyRoot)["textureThumbnails"].is_boolean())
        policy.textureThumbnails = (*policyRoot)["textureThumbnails"].get<bool>();
    if (policyRoot->contains("showDdsThumbnails") && (*policyRoot)["showDdsThumbnails"].is_boolean())
        policy.showDdsThumbnails = (*policyRoot)["showDdsThumbnails"].get<bool>();
    if (policyRoot->contains("adaptiveLargeFolders") && (*policyRoot)["adaptiveLargeFolders"].is_boolean())
        policy.adaptiveLargeFolders = (*policyRoot)["adaptiveLargeFolders"].get<bool>();
    if (policyRoot->contains("largeFolderThreshold") && (*policyRoot)["largeFolderThreshold"].is_number_integer())
        policy.largeFolderThreshold = (*policyRoot)["largeFolderThreshold"].get<int>();
    if (policyRoot->contains("thumbnailLoadBudget") && (*policyRoot)["thumbnailLoadBudget"].is_number_integer())
        policy.thumbnailLoadBudget = (*policyRoot)["thumbnailLoadBudget"].get<int>();
    if (policyRoot->contains("largeFolderThumbnailLoadBudget") &&
        (*policyRoot)["largeFolderThumbnailLoadBudget"].is_number_integer())
    {
        policy.largeFolderThumbnailLoadBudget = (*policyRoot)["largeFolderThumbnailLoadBudget"].get<int>();
    }

    Clamp_ThumbnailPolicy(policy);
}

void ContentBrowserCore::Set_ResourceRoot(const filesystem::path& resourceRoot)
{
    _resourceRoot = resourceRoot;
}

const filesystem::path& ContentBrowserCore::Get_ResourceRoot() const
{
    return _resourceRoot;
}

void ContentBrowserCore::Refresh_BrowserResources(const function<string(const wstring&)>& resolveGuid)
{
    if (_resourceRoot.empty() || !fs::exists(_resourceRoot))
    {
        _rootFolder = {};
        _currentFolder = nullptr;
        return;
    }

    const wstring savedPath = _currentFolder ? _currentFolder->fullPath : L"";

    _rootFolder = {};
    Scan_Folder(_resourceRoot.wstring(), _rootFolder, resolveGuid);

    if (!savedPath.empty())
    {
        FolderNode* found = Find_FolderNode(_rootFolder, savedPath);
        _currentFolder = found ? found : &_rootFolder;
    }
    else
    {
        _currentFolder = &_rootFolder;
    }

    if (_currentFolder && _folderHistory.empty())
        Record_FolderHistory(_currentFolder->fullPath);
}

void ContentBrowserCore::Refresh_CurrentFolder(const function<string(const wstring&)>& resolveGuid)
{
    if (!_currentFolder || !fs::exists(_currentFolder->fullPath))
        return;

    _currentFolder->files.clear();

    for (const auto& entry : fs::directory_iterator(_currentFolder->fullPath))
    {
        if (entry.is_directory() || entry.path().extension() == L".meta")
            continue;

        FileEntry fileEntry{};
        fileEntry.filePath = entry.path().wstring();
        fileEntry.guid = resolveGuid ? resolveGuid(fileEntry.filePath) : string{};
        _currentFolder->files.emplace_back(move(fileEntry));
    }
}

void ContentBrowserCore::Scan_Folder(
    const wstring& path,
    FolderNode& node,
    const function<string(const wstring&)>& resolveGuid)
{
    node.fullPath = path;
    node.name = fs::path(path).filename().wstring();
    if (node.name.empty())
        node.name = fs::path(path).wstring();

    for (const auto& entry : fs::directory_iterator(path))
    {
        if (entry.is_directory())
        {
            FolderNode child{};
            Scan_Folder(entry.path().wstring(), child, resolveGuid);
            node.subFolders.emplace_back(move(child));
        }
        else if (entry.path().extension() != L".meta")
        {
            FileEntry fileEntry{};
            fileEntry.filePath = entry.path().wstring();
            fileEntry.guid = resolveGuid ? resolveGuid(fileEntry.filePath) : string{};
            node.files.emplace_back(move(fileEntry));
        }
    }
}

ContentBrowserCore::FolderNode* ContentBrowserCore::Find_FolderNode(FolderNode& node, const wstring& path)
{
    if (node.fullPath == path)
        return &node;

    for (auto& child : node.subFolders)
    {
        FolderNode* found = Find_FolderNode(child, path);
        if (found)
            return found;
    }

    return nullptr;
}

void ContentBrowserCore::Expand_PathTo(const wstring& targetPath)
{
    _expandedFolders.clear();

    fs::path current(targetPath);
    const fs::path root(_rootFolder.fullPath);

    while (current != root && !current.empty())
    {
        _expandedFolders.insert(current.wstring());
        current = current.parent_path();
    }

    _expandedFolders.insert(root.wstring());
}

bool ContentBrowserCore::IsFolderMatchingSearch(const FolderNode& node, const string& searchStr)
{
    const string nameLower = ToLowerCopy(String::ToString(node.name));
    if (nameLower.find(searchStr) != string::npos)
        return true;

    for (const auto& child : node.subFolders)
    {
        if (IsFolderMatchingSearch(child, searchStr))
            return true;
    }

    return false;
}

ContentBrowserCore::FolderNode* ContentBrowserCore::Get_FirstMatchingFolder(FolderNode& node, const string& searchStr)
{
    const string nameLower = ToLowerCopy(String::ToString(node.name));
    if (nameLower.find(searchStr) != string::npos)
        return &node;

    for (auto& child : node.subFolders)
    {
        FolderNode* found = Get_FirstMatchingFolder(child, searchStr);
        if (found)
            return found;
    }

    return nullptr;
}

void ContentBrowserCore::Record_FolderHistory(const wstring& path)
{
    if (_folderHistoryIndex >= 0 &&
        _folderHistoryIndex < static_cast<int>(_folderHistory.size()) &&
        _folderHistory[_folderHistoryIndex] == path)
    {
        return;
    }

    if (_folderHistoryIndex + 1 < static_cast<int>(_folderHistory.size()))
        _folderHistory.erase(_folderHistory.begin() + _folderHistoryIndex + 1, _folderHistory.end());

    _folderHistory.push_back(path);
    _folderHistoryIndex = static_cast<int>(_folderHistory.size()) - 1;
}

bool ContentBrowserCore::Can_GoBack() const
{
    return _folderHistoryIndex > 0;
}

bool ContentBrowserCore::Can_GoForward() const
{
    return _folderHistoryIndex >= 0 &&
           _folderHistoryIndex + 1 < static_cast<int>(_folderHistory.size());
}

void ContentBrowserCore::Save_CurrentPathSetting(const filesystem::path& settingsPath, const string& editorKey) const
{
    json settings = Read_SettingsFile(settingsPath);
    if (!settings.is_object())
        settings = json::object();

    const wstring currentPath = _currentFolder ? _currentFolder->fullPath : wstring{};
    settings[editorKey]["lastPath"] = String::ToString(currentPath);

    if (!fs::exists(settingsPath.parent_path()))
        fs::create_directories(settingsPath.parent_path());

    ofstream file(settingsPath);
    if (!file.is_open())
        return;

    file << settings.dump(4);
}

void ContentBrowserCore::Load_CurrentPathSetting(const filesystem::path& settingsPath, const string& editorKey)
{
    json settings = Read_SettingsFile(settingsPath);
    if (!settings.is_object())
        return;

    string lastPathString{};
    if (settings.contains(editorKey) &&
        settings[editorKey].is_object() &&
        settings[editorKey].contains("lastPath") &&
        settings[editorKey]["lastPath"].is_string())
    {
        lastPathString = settings[editorKey]["lastPath"].get<string>();
    }
    else if (settings.contains("lastPath") && settings["lastPath"].is_string())
    {
        lastPathString = settings["lastPath"].get<string>();
    }

    if (lastPathString.empty())
        return;

    const wstring lastPath = String::ToWString(lastPathString);
    FolderNode* found = Find_FolderNode(_rootFolder, lastPath);
    if (!found)
        return;

    _currentFolder = found;
    Expand_PathTo(lastPath);
}

NS_END

#include "Content_Browser.h"

#include "EditorInstance.h"
#include "Editor_Macro.h"
#include "Notification_Manager.h"
#include "GameInstance.h"
#include "Helper_String.h"
#include "Texture.h"
#include "BT_VIew.h"
#include "Prefab_View.h"

#include <fstream>
#include <shellapi.h>

#include "Animation_View.h"

namespace fs = std::filesystem;

static ImTextureID ToImTextureId(const Shared<Texture>& texture)
{
    if (texture == nullptr)
        return static_cast<ImTextureID>(0);

    const auto& srvs = texture->Get_SRVs();
    if (srvs.empty() || srvs[0] == nullptr)
        return static_cast<ImTextureID>(0);

    return static_cast<ImTextureID>(reinterpret_cast<intptr_t>(srvs[0].Get()));
}

static void DrawTileLabel(const string& text, float maxWidth)
{
    if (ImGui::CalcTextSize(text.c_str()).x <= maxWidth)
    {
        ImGui::TextUnformatted(text.c_str());
        return;
    }

    constexpr const char* ellipsis = "...";
    const float ellipsisWidth = ImGui::CalcTextSize(ellipsis).x;
    if (maxWidth <= ellipsisWidth)
    {
        ImGui::TextUnformatted(ellipsis);
        return;
    }

    size_t low = 0;
    size_t high = text.size();
    while (low < high)
    {
        const size_t mid = (low + high + 1) / 2;
        const string candidate = text.substr(0, mid) + ellipsis;
        if (ImGui::CalcTextSize(candidate.c_str()).x <= maxWidth)
            low = mid;
        else
            high = mid - 1;
    }

    const string label = text.substr(0, low) + ellipsis;
    ImGui::TextUnformatted(label.c_str());
}

static fs::path Resolve_ModelPath_FromAnimation(const fs::path& animPath)
{
    const auto findModelInDirectory = [](const fs::path& directory, const string& preferredStem) -> fs::path
    {
        if (directory.empty() || !fs::exists(directory))
            return {};

        fs::path firstModelPath{};

        for (const auto& entry : fs::directory_iterator(directory))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".model")
                continue;

            if (entry.path().stem().string() == preferredStem)
                return entry.path();

            if (firstModelPath.empty())
                firstModelPath = entry.path();
        }

        return firstModelPath;
    };

    const fs::path sameDirectory = animPath.parent_path();
    if (fs::path modelPath = findModelInDirectory(sameDirectory, animPath.stem().string()); !modelPath.empty())
        return modelPath;

    const fs::path parentDirectory = sameDirectory.parent_path();
    if (fs::path modelPath = findModelInDirectory(parentDirectory, parentDirectory.filename().string()); !modelPath.empty())
        return modelPath;

    return {};
}

static void Open_AnimationViewFromAssetPath(const fs::path& path, uint32 preferredDockId = 0)
{
    Shared<Editor_Window> viewBase = EDITOR->Get_Window(L"Animation");
    Shared<Animation_View> animationView = dynamic_pointer_cast<Animation_View>(viewBase);
    if (animationView == nullptr)
        return;

    fs::path modelPath = path;
    string preferredAnimationName{};

    if (path.extension() == L".anim")
    {
        preferredAnimationName = path.stem().string();
        modelPath = Resolve_ModelPath_FromAnimation(path);
    }

    if (modelPath.empty())
    {
        NOTIFY("Animation View: model asset not found");
        return;
    }

    const string modelGuid = GAME->Ensure_AssetGUID(modelPath.wstring(), "Model");
    if (modelGuid.empty())
    {
        NOTIFY("Animation View: model GUID not found");
        return;
    }

    animationView->Open_FromAsset(modelPath.wstring(), modelGuid, preferredAnimationName, preferredDockId);
}

Content_Browser::Content_Browser()
    : Editor_Window{ L"Content Browser" }
{
    const wstring assetRoot = GAME->Get_AssetRoot();
    if (!assetRoot.empty())
        Set_ResourceRoot(fs::path(assetRoot));

    Load_Icons();
}

Content_Browser::~Content_Browser()
{
    Save_Settings();
}

void Content_Browser::Update(float timeDelta)
{
    Editor_Window::Update(timeDelta);

    // F5 키 겹쳐서 새로고침 이거로 사용 ㄴ
    //if (_isFocused && ImGui::IsKeyPressed(ImGuiKey_F5, false))
    //    Refresh_AssetState();
}

void Content_Browser::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();

    const string windowName = String::ToString(Get_WindowName());
    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

        if (!_isFocused)
            _selectedFilePath.clear();

        if (_resourceRoot.empty())
        {
            const wstring assetRoot = GAME->Get_AssetRoot();
            if (!assetRoot.empty())
                Set_ResourceRoot(fs::path(assetRoot));
        }

        if (_needsInitialRefresh && _rootFolder.fullPath.empty() && !_resourceRoot.empty() && fs::exists(_resourceRoot))
        {
            Refresh_Resources();
            Load_Settings();
            _needsInitialRefresh = false;
        }

        Handle_SideButtonEvent();
        Handle_ThumbnailZoom();

        ImGui::BeginChild("FolderTree", ImVec2(_leftPanelWidth, 0.f), true);
        {
            if (ImGui::InputTextWithHint("##Search", "Search folders...", _searchBuffer, IM_ARRAYSIZE(_searchBuffer)))
            {
                if (strlen(_searchBuffer) > 0)
                {
                    string searchLower = ToLowerCopy(_searchBuffer);

                    FFolderNode* firstMatch = Get_FirstMatchingFolder(_rootFolder, searchLower);
                    if (firstMatch && _currentFolder != firstMatch)
                    {
                        Open_Folder(firstMatch);
                    }
                }
            }

            ImGui::Separator();

            ImGui::BeginChild("FolderTreeScroll", ImVec2(0, 0), false);
            if (_rootFolder.fullPath.empty())
                ImGui::TextDisabled("Resource folder is not available yet.");
            else
                Draw_FolderTree(_rootFolder);
            ImGui::EndChild();
        }
        ImGui::EndChild();

        ImGui::SameLine();

        const float availHeight = ImGui::GetContentRegionAvail().y;
        if (availHeight > 0.f)
            ImGui::InvisibleButton("vsplitter", ImVec2(4.f, availHeight));

        if (ImGui::IsItemActive())
            _leftPanelWidth = max(180.f, _leftPanelWidth + ImGui::GetIO().MouseDelta.x);

        if (ImGui::IsItemHovered())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

        ImGui::SameLine();

        ImGui::BeginChild("AssetView", ImVec2(0.f, 0.f), true);
        {
            if (_currentFolder)
            {
                ImGui::BeginDisabled(!Can_GoParentFolder());
                if (ImGui::ArrowButton("##ParentFolder", ImGuiDir_Up))
                    Go_ParentFolder();
                ImGui::EndDisabled();

                ImGui::SameLine();

                ImGui::BeginDisabled(!Can_GoBack());
                if (ImGui::ArrowButton("##BackFolder", ImGuiDir_Left))
                    Go_BackFolder();
                ImGui::EndDisabled();

                ImGui::SameLine();

                ImGui::BeginDisabled(!Can_GoForward());
                if (ImGui::ArrowButton("##ForwardFolder", ImGuiDir_Right))
                    Go_ForwardFolder();
                ImGui::EndDisabled();

                ImGui::SameLine();

                string pathStr = String::ToString(_currentFolder->fullPath);
                char pathBuffer[512] = {};
                strncpy_s(pathBuffer, pathStr.c_str(), _TRUNCATE);

                const float refreshWidth = 60.f;
                const float settingsWidth = 28.f;
                const float controlSpacing = 8.f;
                const float minPathWidth = 80.f;
                const float preferredPathWidth = 150.f;
                const float minSearchWidth = 120.f;
                const bool isLargeTextureFolder = Is_LargeTextureFolder();
                const float statusWidth = isLargeTextureFolder ? 170.f : 0.f;
                const float toolbarAvailWidth = ImGui::GetContentRegionAvail().x;
                const float rightControlWidth = refreshWidth + settingsWidth + statusWidth + controlSpacing * 2.f;

                float searchWidth = 0.f;
                if (toolbarAvailWidth >= rightControlWidth + preferredPathWidth + controlSpacing + minSearchWidth)
                    searchWidth = min(220.f, toolbarAvailWidth - rightControlWidth - preferredPathWidth - controlSpacing);

                float pathWidth = toolbarAvailWidth - rightControlWidth;
                if (searchWidth > 0.f)
                    pathWidth -= controlSpacing + searchWidth;

                pathWidth = clamp(pathWidth, minPathWidth, 500.f);

                ImGui::SetNextItemWidth(pathWidth);
                ImGui::InputText("##CurrentPath", pathBuffer, IM_ARRAYSIZE(pathBuffer), ImGuiInputTextFlags_ReadOnly);

                if (searchWidth > 0.f)
                {
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(searchWidth);
                    ImGui::InputTextWithHint("##AssetNameSearch", "Search files...", _assetSearchBuffer, IM_ARRAYSIZE(_assetSearchBuffer));
                }

                ImGui::SameLine();

                float rightX = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x -
                    refreshWidth - settingsWidth - statusWidth - controlSpacing * 2.f;
                if (rightX > ImGui::GetCursorPosX())
                    ImGui::SetCursorPosX(rightX);

                if (isLargeTextureFolder)
                {
                    ImGui::TextDisabled("Large texture folder: %d/frame", Resolve_CurrentThumbnailLoadBudget());
                    ImGui::SameLine();
                }

                if (ImGui::Button(ICON_FA_SLIDERS "##ContentBrowserThumbnailSettings", ImVec2(settingsWidth, 24.f)))
                    ImGui::OpenPopup("ContentBrowserThumbnailSettings");

                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Thumbnail settings");

                Draw_ThumbnailSettingsPopup();

                ImGui::SameLine();

                if (ImGui::Button(ICON_FA_ROTATE_RIGHT "##ContentBrowserRefresh", ImVec2(refreshWidth, 24.f)))
                    Refresh_AssetState();

                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("새로고침");

                ImGui::Separator();

                ImGui::BeginChild("AssetViewScroll", ImVec2(0, 0), false);
                Draw_AssetView();
                ImGui::EndChild();
            }
            else
            {
                ImGui::TextDisabled("No folder selected.");
            }
        }
        ImGui::EndChild();
    }

    Draw_CreatePrefabPopup();

    Set_Open(isOpen);
    ImGui::End();
}

void Content_Browser::Refresh_Resources()
{
    _thumbnailCache.clear();
    _noThumbnailPaths.clear();
    _gridItemCache.clear();
    _gridItemCacheFolder = nullptr;
    Refresh_BrowserResources([](const wstring& filePath)
    {
        return GAME->Find_AssetGUID(filePath);
    });
}

void Content_Browser::Refresh_AssetState()
{
    const wstring assetRoot = GAME->Get_AssetRoot();
    if (assetRoot.empty())
    {
        NOTIFY("새로고침 실패");
        return;
    }

    if (SUCCEEDED(GAME->Initialize_AssetManager(assetRoot, true)))
    {
        Refresh_Resources();
        NOTIFY("새로고침");
    }
    else
    {
        NOTIFY("새로고침 실패");
    }
}

void Content_Browser::Load_Icons()
{
    _iconFiles.assign(static_cast<size_t>(EIconType::END), nullptr);

    fs::path iconRoot = _resourceRoot;
    if (iconRoot.empty())
    {
        const wstring assetRoot = GAME->Get_AssetRoot();
        if (!assetRoot.empty())
            iconRoot = fs::path(assetRoot);
    }

    if (iconRoot.empty())
        return;

    iconRoot /= L"Textures";
    iconRoot /= L"UI";
    iconRoot /= L"Common";
    iconRoot /= L"FileType";

    const auto loadTexture = [&](const wchar_t* fileName) -> Shared<Texture>
    {
        const fs::path filePath = iconRoot / fileName;
        if (!fs::exists(filePath))
            return nullptr;

        return Texture::Create(EDITOR->Get_Device(), EDITOR->Get_Context(), filePath.c_str(), 1);
    };

    _iconFiles[static_cast<size_t>(EIconType::Folder)] = loadTexture(L"Folder_Icon.png");

    Shared<Texture> dataTableIcon = loadTexture(L"DataTable.png");
    _iconFiles[static_cast<size_t>(EIconType::Csv)] = dataTableIcon;
    _iconFiles[static_cast<size_t>(EIconType::Xlsx)] = dataTableIcon;

    _iconFiles[static_cast<size_t>(EIconType::Json)] = loadTexture(L"JSON.png");
    _iconFiles[static_cast<size_t>(EIconType::Python)] = loadTexture(L"Python.png");
}

void Content_Browser::Refresh_CurrentFolder()
{
    _gridItemCache.clear();
    _gridItemCacheFolder = nullptr;
    Client::ContentBrowserCore::Refresh_CurrentFolder([](const wstring& filePath)
    {
        return GAME->Find_AssetGUID(filePath);
    });
}

void Content_Browser::Draw_FolderTree(FFolderNode& node)
{
    if (strlen(_searchBuffer) > 0 && _currentFolder != &node)
    {
        string searchLower = ToLowerCopy(_searchBuffer);

        if (node.fullPath != _rootFolder.fullPath && !IsFolderMatchingSearch(node, searchLower))
            return;

        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    }

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (_currentFolder == &node)
        flags |= ImGuiTreeNodeFlags_Selected;
    if (node.subFolders.empty())
        flags |= ImGuiTreeNodeFlags_Leaf;

    if (_expandedFolders.contains(node.fullPath))
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        _expandedFolders.erase(node.fullPath);
    }

    const string id = "##" + String::ToString(node.fullPath);
    const bool isOpen = ImGui::TreeNodeEx(id.c_str(), flags);

    Draw_FolderContextMenu(node, "FolderTreeContext_" + String::ToString(node.fullPath));

    const bool isClicked = ImGui::IsItemClicked();
    ImGui::SameLine();

    if (ImTextureID iconId = Get_IconTextureId(EIconType::Folder))
    {
        ImGui::Image(iconId, ImVec2(16.f, 16.f));
        ImGui::SameLine(0.f, 6.f);
    }

    const string folderName = String::ToString(node.name);
    ImGui::TextUnformatted(folderName.c_str());

    if (isClicked)
        Open_Folder(&node);

    if (isOpen)
    {
        for (auto& child : node.subFolders)
            Draw_FolderTree(child);

        ImGui::TreePop();
    }
}

void Content_Browser::Draw_AssetView()
{
    if (!_currentFolder)
        return;

    _thumbnailLoadsThisFrame = 0;
    _thumbnailLoadBudgetThisFrame = Resolve_CurrentThumbnailLoadBudget();

    const string assetSearchLower = ToLowerCopy(_assetSearchBuffer);
    const bool shouldRebuildGridItems =
        _gridItemCacheFolder != _currentFolder ||
        _gridItemCacheSubFolderCount != _currentFolder->subFolders.size() ||
        _gridItemCacheFileCount != _currentFolder->files.size() ||
        _gridItemCacheSearchLower != assetSearchLower;

    if (shouldRebuildGridItems)
    {
        Build_GridItems(
            *_currentFolder,
            assetSearchLower,
            [](const FFolderNode& folder, const string& searchLower)
            {
                if (searchLower.empty())
                    return true;

                const string folderNameLower = ToLowerCopy(String::ToString(folder.name));
                return folderNameLower.find(searchLower) != string::npos;
            },
            [](const FFileEntry& fileEntry, const string& searchLower)
            {
                if (searchLower.empty())
                    return true;

                const fs::path path(fileEntry.filePath);
                const string fileName = String::ToString(path.filename().wstring());
                const string prefabSuffix = ".prefab.json";
                string pureName = String::ToString(path.stem().wstring());

                if (fileName.ends_with(prefabSuffix))
                    pureName = fileName.substr(0, fileName.size() - prefabSuffix.size());

                const string pureNameLower = ToLowerCopy(pureName);
                const string fileNameLower = ToLowerCopy(fileName);
                return pureNameLower.find(searchLower) != string::npos ||
                    fileNameLower.find(searchLower) != string::npos;
            },
            _gridItemCache);

        _gridItemCacheFolder = _currentFolder;
        _gridItemCacheSubFolderCount = _currentFolder->subFolders.size();
        _gridItemCacheFileCount = _currentFolder->files.size();
        _gridItemCacheSearchLower = assetSearchLower;
    }

    const GridLayout gridLayout = Calculate_GridLayout(
        ImGui::GetContentRegionAvail().x,
        _thumbnailSize,
        ImGui::GetTextLineHeightWithSpacing(),
        _gridItemCache.size());
    const GridVisibleRange visibleRange = Calculate_GridVisibleRange(
        gridLayout,
        ImGui::GetScrollY(),
        ImGui::GetWindowHeight(),
        _gridItemCache.size(),
        1);

    const auto resolveGridItem = [&](const FGridItemRef& item) -> pair<FFolderNode*, const FFileEntry*>
    {
        if (item.type == ContentBrowserGridItemType::Folder)
        {
            if (item.index >= _currentFolder->subFolders.size())
                return { nullptr, nullptr };

            return { &_currentFolder->subFolders[item.index], nullptr };
        }

        if (item.index >= _currentFolder->files.size())
            return { nullptr, nullptr };

        return { nullptr, &_currentFolder->files[item.index] };
    };

    const auto drawFileTile = [&](const FFileEntry& fileEntry)
    {
        const fs::path path(fileEntry.filePath);
        const string fileName = String::ToString(path.filename().wstring());
        const string prefabSuffix = ".prefab.json";
        string pureName = String::ToString(path.stem().wstring());

        if (fileName.ends_with(prefabSuffix))
            pureName = fileName.substr(0, fileName.size() - prefabSuffix.size());

        ImGui::PushID(fileName.c_str());

        const bool isTileVisible = ImGui::IsRectVisible(
            ImVec2(_thumbnailSize, _thumbnailSize + ImGui::GetTextLineHeightWithSpacing() * 2.f));
        Shared<Texture> thumbnailTexture = Get_FileThumbnail(
            fileEntry,
            Should_RequestThumbnail(fileEntry, isTileVisible, _thumbnailLoadsThisFrame, _thumbnailLoadBudgetThisFrame));
        if (thumbnailTexture)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.f, 0.f));
            ImGui::ImageButton(
                fileName.c_str(),
                ToImTextureId(thumbnailTexture),
                ImVec2(_thumbnailSize, _thumbnailSize));
            ImGui::PopStyleVar();
        }
        else
        {
            const ImTextureID iconId = Get_IconTextureId(Resolve_IconType(path));
            if (iconId)
            {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.f, 0.f));
                ImGui::ImageButton(fileName.c_str(), iconId, ImVec2(_thumbnailSize, _thumbnailSize));
                ImGui::PopStyleVar();
            }
            else
            {
                ImGui::Button(pureName.c_str(), ImVec2(_thumbnailSize, _thumbnailSize));
            }
        }

        // 드래그해서 프리펩 스폰
        {
            if (path.extension() == L".json" && fileName.ends_with(prefabSuffix))
            {
                if (ImGui::BeginDragDropSource())
                {
                    const string prefabAssetKey = !fileEntry.guid.empty() ? fileEntry.guid : pureName; // guid가 있으면 경로 대신 guid를 payload로 싣고, 없을 때만 기존 prefab 이름 fallback을 쓴다.
                    ImGui::SetDragDropPayload(
                        "CONTENT_BROWSER_PREFAB",
                        prefabAssetKey.c_str(),
                        prefabAssetKey.size() + 1);

                    ImGui::Text("Prefab");
                    ImGui::Separator();
                    ImGui::Text("%s", pureName.c_str());

                    ImGui::EndDragDropSource();
                }
            }
        }

        // 맵 스태틱 매쉬 스폰
        {
            const wstring ext = path.extension().wstring();
            if (ext == L".model")
            {
                if (ImGui::BeginDragDropSource())
                {
                    const string pathA = String::ToString(fileEntry.filePath);
                    ImGui::SetDragDropPayload(
                        "CONTENT_BROWSER_MODEL",
                        pathA.c_str(),
                        pathA.size() + 1);
                    ImGui::Text("Map Static Mesh");
                    ImGui::Separator();
                    ImGui::Text("%s", pureName.c_str());

                    ImGui::EndDragDropSource();
                }
            }
        }
        //

        // 드래그해서 이펙트 스폰
        {
            if (fileName.ends_with(".effect.json"))
            {
                if (ImGui::BeginDragDropSource())
                {
                    const string pathA = String::ToString(fileEntry.filePath);
                    ImGui::SetDragDropPayload(
                        "CONTENT_BROWSER_EFFECT",
                        pathA.c_str(),
                        pathA.size() + 1);

                    ImGui::Text("Effect");
                    ImGui::Separator();
                    ImGui::Text("%s", pureName.c_str());

                    ImGui::EndDragDropSource();
                }
            }
        }

        const bool isFocused = ImGui::IsItemFocused();
        const bool isSelected = (_selectedFilePath == fileEntry.filePath);

        if (isSelected)
        {
            const ImVec2 itemMin = ImGui::GetItemRectMin();
            const ImVec2 itemMax = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddRect(itemMin, itemMax, IM_COL32(70, 130, 210, 255), 0.f, 0, 2.f);
        }

        if (isFocused && !isSelected)
            _selectedFilePath = fileEntry.filePath;

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            _selectedFilePath = fileEntry.filePath;

        // 더블클릭으로 프리펩 열기 / TODO : Behavior Tree 도 추가하기
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            // BT_*.json 은 Prefab_View가 아니라 BT_View에서 바로 연다.
            if (path.extension() == L".json" && fileName.starts_with("BT_"))
            {
                Shared<Editor_Window> btWindowBase = EDITOR->Get_Window(L"BT View");
                Shared<BT_View> btView = dynamic_pointer_cast<BT_View>(btWindowBase);

                if (btView)
                {
                    btView->Open_BTAsset(path.string());
                }
            }
            else if (path.extension() == L".json" && fileName.ends_with(prefabSuffix))
            {
                Shared<Editor_Window> prefabWindowBase = EDITOR->Get_Window(L"Prefab");
                Shared<Prefab_View> prefabView = dynamic_pointer_cast<Prefab_View>(prefabWindowBase);

                if (prefabView)
                {
                    const string prefabName = fileName.substr(0, fileName.size() - prefabSuffix.size());
                    prefabView->Open_Prefab(prefabName, path);
                }
            }
            else if (path.extension() == L".model" || path.extension() == L".anim")
            {
                Open_AnimationViewFromAssetPath(path);
            }
        }

        Draw_FileContextMenu(fileEntry.filePath, "FileItemContext");

        DrawTileLabel(pureName, gridLayout.tileWidth - 8.f);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", fileName.c_str());

        ImGui::PopID();
    };

    if (visibleRange.hasItems)
    {
        const ImVec2 startCursorPos = ImGui::GetCursorPos();

        for (int rowIndex = visibleRange.firstRow; rowIndex <= visibleRange.lastRow; ++rowIndex)
        {
            const float rowY = startCursorPos.y + static_cast<float>(rowIndex) * gridLayout.tileHeight;

            for (int columnIndex = 0; columnIndex < gridLayout.columnCount; ++columnIndex)
            {
                const size_t itemIndex = static_cast<size_t>(rowIndex * gridLayout.columnCount + columnIndex);
                if (itemIndex > visibleRange.lastItem || itemIndex >= _gridItemCache.size())
                    break;

                ImGui::SetCursorPos(ImVec2(startCursorPos.x + static_cast<float>(columnIndex) * gridLayout.tileWidth, rowY));
                ImGui::BeginGroup();

                const auto [folder, file] = resolveGridItem(_gridItemCache[itemIndex]);
                if (folder)
                    Draw_FolderTile(*folder);
                else if (file)
                    drawFileTile(*file);

                ImGui::EndGroup();
            }
        }

        ImGui::SetCursorPos(ImVec2(startCursorPos.x, startCursorPos.y + static_cast<float>(gridLayout.rowCount) * gridLayout.tileHeight));
        ImGui::Dummy(ImVec2(1.f, 1.f));
    }

    if (_currentFolder &&
        ImGui::BeginPopupContextWindow("CurrentFolderContext",
            ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
    {
        if (ImGui::MenuItem("프리팹 만들기"))
            Open_CreatePrefabPopup(_currentFolder->fullPath);

        if (ImGui::MenuItem("탐색기에서 열기"))
        {
            const wstring absPath = fs::absolute(_currentFolder->fullPath).wstring();

            ShellExecuteW(
                nullptr,
                L"open",
                absPath.c_str(),
                nullptr,
                nullptr,
                SW_SHOWNORMAL);
        }

        ImGui::EndPopup();
    }
}

void Content_Browser::Draw_ThumbnailSettingsPopup()
{
    if (!ImGui::BeginPopup("ContentBrowserThumbnailSettings"))
        return;

    bool changed = false;
    changed |= ImGui::Checkbox("Texture thumbnails", &_thumbnailPolicy.textureThumbnails);
    changed |= ImGui::Checkbox("Show DDS thumbnails", &_thumbnailPolicy.showDdsThumbnails);
    changed |= ImGui::Checkbox("Adaptive large folders", &_thumbnailPolicy.adaptiveLargeFolders);

    changed |= ImGui::InputInt("Large folder threshold", &_thumbnailPolicy.largeFolderThreshold, 10, 100);
    changed |= ImGui::InputInt("Thumbnail load budget", &_thumbnailPolicy.thumbnailLoadBudget, 1, 4);
    changed |= ImGui::InputInt("Large folder budget", &_thumbnailPolicy.largeFolderThumbnailLoadBudget, 1, 4);

    _thumbnailPolicy.largeFolderThreshold = clamp(_thumbnailPolicy.largeFolderThreshold, 1, 10000);
    _thumbnailPolicy.thumbnailLoadBudget = clamp(_thumbnailPolicy.thumbnailLoadBudget, 1, 64);
    _thumbnailPolicy.largeFolderThumbnailLoadBudget = clamp(_thumbnailPolicy.largeFolderThumbnailLoadBudget, 0, 64);

    if (changed)
        Save_Settings();

    ImGui::EndPopup();
}

void Content_Browser::Draw_FolderTile(FFolderNode& folder)
{
    ImGui::PushID(String::ToString(folder.fullPath).c_str());

    const ImTextureID iconId = Get_IconTextureId(EIconType::Folder);
    if (iconId)
        ImGui::Image(iconId, ImVec2(_thumbnailSize, _thumbnailSize));
    else
        ImGui::Button("[Folder]", ImVec2(_thumbnailSize, _thumbnailSize));

    const bool isFocused = ImGui::IsItemFocused();
    const bool isSelected = (_selectedFilePath == folder.fullPath);

    if (isSelected)
    {
        const ImVec2 itemMin = ImGui::GetItemRectMin();
        const ImVec2 itemMax = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddRect(itemMin, itemMax, IM_COL32(70, 130, 210, 255), 0.f, 0, 2.f);
    }

    if (isFocused && !isSelected)
        _selectedFilePath = folder.fullPath;

    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
        Open_Folder(&folder);

    Draw_FolderContextMenu(folder, "FolderTileContext_" + String::ToString(folder.fullPath));

    const string folderName = String::ToString(folder.name);
    DrawTileLabel(folderName, _thumbnailSize + 32.f);

    ImGui::PopID();
}

void Content_Browser::Open_Folder(FFolderNode* folder)
{
    if (!folder)
        return;

    _currentFolder = folder;
    _selectedFilePath.clear();

    Expand_PathTo(folder->fullPath);

    if (!_suppressHistoryRecord)
        Record_FolderHistory(folder->fullPath);

    Save_Settings();
}

void Content_Browser::Draw_FolderContextMenu(FFolderNode& folder, const string& popupId)
{
    if (!ImGui::BeginPopupContextItem(popupId.c_str()))
        return;

    if (ImGui::MenuItem("프리팹 만들기"))
        Open_CreatePrefabPopup(folder.fullPath);

    if (ImGui::MenuItem("탐색기에서 열기"))
    {
        const wstring absPath = fs::absolute(folder.fullPath).wstring();

        ShellExecuteW(
            nullptr,
            L"open",
            absPath.c_str(),
            nullptr,
            nullptr,
            SW_SHOWNORMAL);
    }

    ImGui::EndPopup();
}

void Content_Browser::Draw_CreatePrefabPopup()
{
    if (_openCreatePrefabPopup)
    {
        ImGui::OpenPopup("CreatePrefabPopup");
        _openCreatePrefabPopup = false;
    }

    if (!ImGui::BeginPopupModal("CreatePrefabPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    ImGui::InputTextWithHint(
        "##CreatePrefabSearch",
        "Search game object...",
        _createPrefabSearchBuffer,
        IM_ARRAYSIZE(_createPrefabSearchBuffer));

    ImGui::Separator();

    const string searchLower = ToLowerCopy(_createPrefabSearchBuffer);

    string currentCategory{};
    ImGui::BeginChild("CreatePrefabList", ImVec2(420.f, 320.f), true);

    for (const auto& entry : _createPrefabEntries)
    {
        if (!searchLower.empty())
        {
            const string nameLower = ToLowerCopy(entry.displayName);
            const string classLower = ToLowerCopy(entry.className);
            const string categoryLower = ToLowerCopy(entry.category);

            if (nameLower.find(searchLower) == string::npos &&
                classLower.find(searchLower) == string::npos &&
                categoryLower.find(searchLower) == string::npos)
            {
                continue;
            }
        }

        if (currentCategory != entry.category)
        {
            currentCategory = entry.category;
            ImGui::Separator();
            ImGui::TextDisabled("%s", currentCategory.c_str());
            ImGui::Separator();
        }

        const bool isSelected = (_selectedPrefabClass == entry.className);
        if (ImGui::Selectable(entry.displayName.c_str(), isSelected))
        {
            _selectedPrefabClass = entry.className;
            strcpy_s(_createPrefabNameBuffer, entry.displayName.c_str());
        }
    }

    ImGui::EndChild();

    ImGui::Separator();
    ImGui::InputText("Prefab Name", _createPrefabNameBuffer, IM_ARRAYSIZE(_createPrefabNameBuffer));

    const bool canCreate =
        !_selectedPrefabClass.empty() &&
        strlen(_createPrefabNameBuffer) > 0 &&
        !_createPrefabTargetFolder.empty();

    ImGui::BeginDisabled(!canCreate);
    if (ImGui::Button("Create", ImVec2(120.f, 0.f)))
    {
        const string prefabName = _createPrefabNameBuffer;
        const fs::path targetPath =
            fs::path(_createPrefabTargetFolder) / (String::ToWString(prefabName) + L".prefab.json");

        if (SUCCEEDED(GAME->Create_PrefabAsset(targetPath, _selectedPrefabClass, prefabName)))
        {
            Refresh_Resources();

            FFolderNode* found = Find_FolderNode(_rootFolder, _createPrefabTargetFolder);
            if (found)
                _currentFolder = found;

            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::EndDisabled();

    ImGui::SameLine();

    if (ImGui::Button("Cancel", ImVec2(120.f, 0.f)))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}

void Content_Browser::Open_CreatePrefabPopup(const wstring& targetFolderPath)
{
    _createPrefabTargetFolder = targetFolderPath;
    _selectedPrefabClass.clear();
    _createPrefabNameBuffer[0] = '\0';
    _createPrefabSearchBuffer[0] = '\0';

    Rebuild_CreatePrefabEntries();
    _openCreatePrefabPopup = true;
}

void Content_Browser::Rebuild_CreatePrefabEntries()
{
    _createPrefabEntries.clear();

    GAME->Visit_GameObjectTypes([&](const TypeRegistry::GameObjectTypeInfo& info)
    {
        if (info.className.empty() || info.reflectionInfo == nullptr)
            return;

        const string className = info.className;
        const string displayName =
            info.reflectionInfo->displayName.empty() ? className : info.reflectionInfo->displayName;
        const string category = info.reflectionInfo->category;

        if (category.empty())
            return;

        _createPrefabEntries.push_back({ className, displayName, category });
    });

    sort(_createPrefabEntries.begin(), _createPrefabEntries.end(),
        [](const FPrefabCreateEntry& a, const FPrefabCreateEntry& b)
        {
            if (a.category != b.category)
                return a.category < b.category;

            return a.displayName < b.displayName;
        });
}

void Content_Browser::Draw_FileContextMenu(const wstring& filePath, const string& popupId)
{
    if (!ImGui::BeginPopupContextItem(popupId.c_str()))
        return;

    // 프리펩 열기
    const fs::path path(filePath);
    const string fileName = String::ToString(path.filename().wstring());
    const string suffix = ".prefab.json";

    if (path.extension() == L".json" && fileName.ends_with(suffix))
    {
        if (ImGui::MenuItem("프리펩 열기"))
        {
            Shared<Editor_Window> prefabWindowBase = EDITOR->Get_Window(L"Prefab");
            Shared<Prefab_View> prefabView = dynamic_pointer_cast<Prefab_View>(prefabWindowBase);

            if (prefabView)
            {
                string prefabName = fileName.substr(0, fileName.size() - suffix.size());
                prefabView->Open_Prefab(prefabName, path);
            }
        }
    }

    // 애니메이션 열기
    if (path.extension() == L".model" || path.extension() == L".anim")
    {
        if (ImGui::MenuItem("애니메이션 열기"))
            Open_AnimationViewFromAssetPath(path);
    }


    if (ImGui::MenuItem("탐색기에서 열기"))
    {
        const wstring absPath = fs::absolute(filePath).wstring();
        const wstring args = L"/select,\"" + absPath + L"\"";

        ShellExecuteW(
            nullptr,
            L"open",
            L"explorer.exe",
            args.c_str(),
            nullptr,
            SW_SHOWNORMAL);
    }

    ImGui::EndPopup();
}

Shared<Texture> Content_Browser::Get_FileThumbnail(const FFileEntry& fileEntry, bool allowLoad)
{
    if (fileEntry.filePath.empty())
        return nullptr;

    const fs::path sourcePath(fileEntry.filePath);
    if (Is_TextureThumbnailCandidate(sourcePath))
    {
        if (!_thumbnailPolicy.textureThumbnails)
            return nullptr;

        if (Is_DdsFile(sourcePath) && !_thumbnailPolicy.showDdsThumbnails)
            return nullptr;
    }

    auto cacheIt = _thumbnailCache.find(fileEntry.filePath);
    if (cacheIt != _thumbnailCache.end())
        return cacheIt->second;

    if (_noThumbnailPaths.contains(fileEntry.filePath))
        return nullptr;

    if (!allowLoad)
        return nullptr;

    if (_thumbnailLoadsThisFrame >= _thumbnailLoadBudgetThisFrame)
        return nullptr;

    fs::path thumbnailPath{};
    if (!fileEntry.guid.empty())
    {
        thumbnailPath = _resourceRoot / L"Thumbnails" / String::ToWString(fileEntry.guid + ".png");
        if (!fs::exists(thumbnailPath))
            thumbnailPath.clear();
    }

    if (thumbnailPath.empty())
    {
        if (!Is_ThumbnailFileExtension(sourcePath))
        {
            _noThumbnailPaths.insert(fileEntry.filePath);
            return nullptr;
        }

        thumbnailPath = sourcePath;
    }

    ++_thumbnailLoadsThisFrame;

    Shared<Texture> thumbnailTexture =
        Texture::Create(EDITOR->Get_Device(), EDITOR->Get_Context(), thumbnailPath.c_str(), 1);

    if (thumbnailTexture == nullptr)
    {
        _noThumbnailPaths.insert(fileEntry.filePath);
        return nullptr;
    }

    _thumbnailCache[fileEntry.filePath] = thumbnailTexture;
    return thumbnailTexture;
}

bool Content_Browser::Is_LargeTextureFolder() const
{
    return ContentBrowserCore::Is_LargeTextureFolder(_currentFolder, _thumbnailPolicy);
}

int Content_Browser::Resolve_CurrentThumbnailLoadBudget() const
{
    return Resolve_ThumbnailLoadBudget(_currentFolder, _thumbnailPolicy);
}

void Content_Browser::Save_Settings() const
{
    Save_CurrentPathSetting(CONTENT_BROWSER_SETTINGS_PATH, "mainEditor");
    Save_ThumbnailPolicySetting(CONTENT_BROWSER_SETTINGS_PATH, "mainEditor", _thumbnailPolicy);
}

void Content_Browser::Load_Settings()
{
    Load_CurrentPathSetting(CONTENT_BROWSER_SETTINGS_PATH, "mainEditor");
    Load_ThumbnailPolicySetting(CONTENT_BROWSER_SETTINGS_PATH, "mainEditor", _thumbnailPolicy);
}

Content_Browser::EIconType Content_Browser::Resolve_IconType(const filesystem::path& filePath) const
{
    const string extension = ToLowerCopy(String::ToString(filePath.extension().wstring()));

    if (extension == ".csv")
        return EIconType::Csv;
    if (extension == ".json")
        return EIconType::Json;
    if (extension == ".py")
        return EIconType::Python;
    if (extension == ".xlsx")
        return EIconType::Xlsx;

    return EIconType::Json;
}

ImTextureID Content_Browser::Get_IconTextureId(EIconType iconType) const
{
    const size_t iconIndex = static_cast<size_t>(iconType);
    if (iconIndex >= _iconFiles.size())
        return static_cast<ImTextureID>(0);

    return ToImTextureId(_iconFiles[iconIndex]);
}

void Content_Browser::Go_BackFolder()
{
    if (!Can_GoBack())
        return;

    --_folderHistoryIndex;

    FFolderNode* target = Find_FolderNode(_rootFolder, _folderHistory[_folderHistoryIndex]);
    if (!target)
        return;

    _suppressHistoryRecord = true;
    Open_Folder(target);
    _suppressHistoryRecord = false;
}

bool Content_Browser::Can_GoParentFolder() const
{
    if (_currentFolder == nullptr || _rootFolder.fullPath.empty())
        return false;

    const fs::path currentPath(_currentFolder->fullPath);
    const fs::path rootPath(_rootFolder.fullPath);
    if (currentPath.empty() || currentPath == rootPath)
        return false;

    const fs::path parentPath = currentPath.parent_path();
    return !parentPath.empty() && parentPath != currentPath;
}

void Content_Browser::Go_ParentFolder()
{
    if (!Can_GoParentFolder())
        return;

    const fs::path parentPath = fs::path(_currentFolder->fullPath).parent_path();
    FFolderNode* target = Find_FolderNode(_rootFolder, parentPath.wstring());
    if (!target)
        return;

    Open_Folder(target);
}

void Content_Browser::Go_ForwardFolder()
{
    if (!Can_GoForward())
        return;

    ++_folderHistoryIndex;

    FFolderNode* target = Find_FolderNode(_rootFolder, _folderHistory[_folderHistoryIndex]);
    if (!target)
        return;

    _suppressHistoryRecord = true;
    Open_Folder(target);
    _suppressHistoryRecord = false;
}

void Content_Browser::Handle_SideButtonEvent()
{
    const bool isBrowserFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    const bool isBrowserHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

    if (!(isBrowserFocused || isBrowserHovered))
        return;

    if (ImGui::IsMouseClicked(3))
        Go_BackFolder();

    if (ImGui::IsMouseClicked(4))
        Go_ForwardFolder();
}

void Content_Browser::Handle_ThumbnailZoom()
{
    const bool isBrowserFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    if (!isBrowserFocused)
        return;

    ImGuiIO& io = ImGui::GetIO();
    if (!io.KeyCtrl || io.MouseWheel == 0.f)
        return;

    _thumbnailSize += io.MouseWheel * 8.f;
    _thumbnailSize = clamp(_thumbnailSize, 32.f, 128.f);
}

Shared<Content_Browser> Content_Browser::Create()
{
    return make_shared<Content_Browser>();
}

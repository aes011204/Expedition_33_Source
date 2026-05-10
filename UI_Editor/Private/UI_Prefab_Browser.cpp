#include "pch.h"
#include "UI_Prefab_Browser.h"
#include "UI_Editor_Manager.h"

namespace UIEditor
{
    namespace
    {
        constexpr wchar_t kPrefabExtension[] = L".uiprefab.json";
        constexpr const char* kDefaultPrefabName = "UIPrefab";

        bool Has_PrefabExtension(const fs::path& filePath)
        {
            const wstring fileName = filePath.filename().wstring();
            if (fileName.size() < wcslen(kPrefabExtension))
                return false;

            const wstring suffix = fileName.substr(fileName.size() - wcslen(kPrefabExtension));
            return _wcsicmp(suffix.c_str(), kPrefabExtension) == 0;
        }

        string Sanitize_FolderName(string folderName)
        {
            for (char& character : folderName)
            {
                switch (character)
                {
                case '<':
                case '>':
                case ':':
                case '"':
                case '/':
                case '\\':
                case '|':
                case '?':
                case '*':
                    character = '_';
                    break;
                default:
                    break;
                }
            }

            while (!folderName.empty() && (folderName.back() == ' ' || folderName.back() == '.'))
                folderName.pop_back();

            return folderName;
        }

        vector<fs::path> Collect_ChildDirectories(const fs::path& directoryPath)
        {
            vector<fs::path> childDirectories;
            try
            {
                if (!fs::exists(directoryPath) || !fs::is_directory(directoryPath))
                    return childDirectories;

                for (const fs::directory_entry& entry : fs::directory_iterator(directoryPath, fs::directory_options::skip_permission_denied))
                {
                    if (entry.is_directory())
                        childDirectories.push_back(entry.path());
                }
            }
            catch (const exception& exception)
            {
                LOG_WARN("UI prefab folder scan failed: {} ({})", exception.what(), String::ToString(directoryPath.wstring()));
            }

            sort(childDirectories.begin(), childDirectories.end(), [](const fs::path& lhs, const fs::path& rhs)
                {
                    return String::ToLowerCopy(String::ToString(lhs.filename().wstring())) <
                        String::ToLowerCopy(String::ToString(rhs.filename().wstring()));
                });

            return childDirectories;
        }

        bool Matches_SearchText(const string& haystack, const string& searchText)
        {
            if (searchText.empty())
                return true;

            return String::ToLowerCopy(haystack).find(searchText) != string::npos;
        }
    }

    UI_Prefab_Browser::UI_Prefab_Browser(UI_Editor_Manager& editorManager)
        : UI_Editor_Window{ L"Prefab Browser" }
        , _editorManager{ editorManager }
    {
        strncpy_s(_prefabNameBuffer, kDefaultPrefabName, _TRUNCATE);

        if (FAILED(_prefabService.Initialize()))
            LOG_WARN("UI Prefab Browser: prefab root directory initialization failed.");

        _selectedDirectory = _prefabService.Get_RootDirectory();
    }

    void UI_Prefab_Browser::Render()
    {
        if (!Is_Open())
            return;

        bool isOpen = Is_Open();
        const string windowName = String::ToString(Get_WindowName());

        if (ImGui::Begin(windowName.c_str(), &isOpen))
        {
            _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
            _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

            if (_prefabService.Get_RootDirectory().empty() || !_prefabService.Ensure_RootDirectory())
            {
                ImGui::TextDisabled("Prefab root directory could not be resolved.");
            }
            else
            {
                ImGui::TextDisabled("%s", String::ToString(_prefabService.Get_RootDirectory()).c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton("Refresh"))
                    _needsRefresh = true;

                ImGui::Separator();

                const float folderPanelWidth = 260.f;
                if (ImGui::BeginChild("UIPrefabFolderTree", ImVec2(folderPanelWidth, 0.f), true))
                {
                    Render_CreateFolderControls();
                    ImGui::Separator();
                    Render_FolderTree();
                }
                ImGui::EndChild();

                ImGui::SameLine();

                if (ImGui::BeginChild("UIPrefabSelectedFolder", ImVec2(0.f, 0.f), false))
                    Render_SelectedFolderPanel();
                ImGui::EndChild();
            }
        }

        Set_Open(isOpen);
        ImGui::End();
    }

    void UI_Prefab_Browser::Render_FolderTree()
    {
        const fs::path rootDirectory = fs::path(_prefabService.Get_RootDirectory());
        Render_FolderNode(rootDirectory);
    }

    void UI_Prefab_Browser::Render_FolderNode(const fs::path& directoryPath)
    {
        const vector<fs::path> childDirectories = Collect_ChildDirectories(directoryPath);
        const bool isSelected = fs::path(_selectedDirectory).lexically_normal() == directoryPath.lexically_normal();

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (isSelected)
            flags |= ImGuiTreeNodeFlags_Selected;
        if (childDirectories.empty())
            flags |= ImGuiTreeNodeFlags_Leaf;

        const fs::path rootDirectory = fs::path(_prefabService.Get_RootDirectory()).lexically_normal();
        string label = directoryPath.lexically_normal() == rootDirectory
            ? "UI_Prefabs"
            : String::ToString(directoryPath.filename().wstring());
        const string stableId = String::ToString(directoryPath.wstring());

        ImGui::PushID(stableId.c_str());
        const bool isOpen = ImGui::TreeNodeEx("Folder", flags, "%s", label.c_str());
        if (ImGui::IsItemClicked())
            Select_Directory(directoryPath);

        if (isOpen)
        {
            for (const fs::path& childDirectory : childDirectories)
                Render_FolderNode(childDirectory);

            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    void UI_Prefab_Browser::Render_SelectedFolderPanel()
    {
        if (_selectedDirectory.empty())
            _selectedDirectory = _prefabService.Get_RootDirectory();

        if (_needsRefresh)
            Refresh_PrefabEntries();

        ImGui::Text("Folder: %s", Make_RelativeDisplayPath(fs::path(_selectedDirectory)).c_str());
        ImGui::Separator();

        Render_SaveControls();

        ImGui::SetNextItemWidth(260.f);
        ImGui::InputText("Search", _searchBuffer, size(_searchBuffer));
        ImGui::Separator();

        const string searchText = String::ToLowerCopy(_searchBuffer);
        int32 visiblePrefabCount = 0;

        for (const PrefabFileEntry& prefabEntry : _prefabEntries)
        {
            const string haystack = prefabEntry.name + " " + prefabEntry.relativePath;
            if (!Matches_SearchText(haystack, searchText))
                continue;

            ++visiblePrefabCount;
            ImGui::PushID(String::ToString(prefabEntry.filePath).c_str());

            const bool selected = false;
            if (ImGui::Selectable(prefabEntry.name.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick))
            {
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    Create_PrefabInstance(prefabEntry.filePath);
            }

            ImGui::SameLine();
            if (ImGui::SmallButton("Create"))
                Create_PrefabInstance(prefabEntry.filePath);

            ImGui::TextDisabled("%s", prefabEntry.relativePath.c_str());
            ImGui::Separator();
            ImGui::PopID();
        }

        if (visiblePrefabCount == 0)
            ImGui::TextDisabled("No prefabs found in this folder.");
    }

    void UI_Prefab_Browser::Render_SaveControls()
    {
        const bool canSavePrefab = _editorManager.Has_SelectedObjects();

        ImGui::SetNextItemWidth(260.f);
        ImGui::InputText("Prefab Name", _prefabNameBuffer, size(_prefabNameBuffer));
        ImGui::SameLine();

        ImGui::BeginDisabled(!canSavePrefab);
        if (ImGui::Button("Save Selected"))
            Save_SelectedPrefab();
        ImGui::EndDisabled();

        if (!canSavePrefab)
            ImGui::TextDisabled("Select one or more UI objects to save a prefab.");

        ImGui::Separator();
    }

    void UI_Prefab_Browser::Render_CreateFolderControls()
    {
        ImGui::TextUnformatted("New Folder");
        ImGui::SetNextItemWidth(-1.f);
        ImGui::InputText("##UIPrefabNewFolder", _newFolderNameBuffer, size(_newFolderNameBuffer));

        if (ImGui::SmallButton("Create Folder"))
        {
            const string safeFolderName = Sanitize_FolderName(_newFolderNameBuffer);
            if (!safeFolderName.empty())
            {
                const fs::path parentDirectory = _selectedDirectory.empty()
                    ? fs::path(_prefabService.Get_RootDirectory())
                    : fs::path(_selectedDirectory);
                const fs::path newDirectory = (parentDirectory / String::ToWString(safeFolderName)).lexically_normal();

                try
                {
                    fs::create_directories(newDirectory);
                    Select_Directory(newDirectory);
                    _newFolderNameBuffer[0] = '\0';
                }
                catch (const exception& exception)
                {
                    LOG_WARN("UI prefab folder create failed: {} ({})", exception.what(), String::ToString(newDirectory.wstring()));
                }
            }
        }
    }

    void UI_Prefab_Browser::Refresh_PrefabEntries()
    {
        _prefabEntries.clear();
        _needsRefresh = false;

        const fs::path selectedDirectory = _selectedDirectory.empty()
            ? fs::path(_prefabService.Get_RootDirectory())
            : fs::path(_selectedDirectory);

        try
        {
            if (!fs::exists(selectedDirectory) || !fs::is_directory(selectedDirectory))
                return;

            for (const fs::directory_entry& entry : fs::directory_iterator(selectedDirectory, fs::directory_options::skip_permission_denied))
            {
                if (!entry.is_regular_file() || !Has_PrefabExtension(entry.path()))
                    continue;

                UI_PrefabAsset prefab = {};
                const wstring filePath = entry.path().wstring();
                if (!_prefabService.Load_Prefab(filePath, prefab))
                    continue;

                PrefabFileEntry prefabEntry = {};
                prefabEntry.filePath = filePath;
                prefabEntry.name = prefab.name.empty() ? String::ToString(entry.path().filename().wstring()) : prefab.name;
                prefabEntry.relativePath = Make_RelativeDisplayPath(entry.path());
                _prefabEntries.push_back(move(prefabEntry));
            }
        }
        catch (const exception& exception)
        {
            LOG_WARN("UI prefab file scan failed: {} ({})", exception.what(), String::ToString(selectedDirectory.wstring()));
        }

        sort(_prefabEntries.begin(), _prefabEntries.end(), [](const PrefabFileEntry& lhs, const PrefabFileEntry& rhs)
            {
                return String::ToLowerCopy(lhs.name) < String::ToLowerCopy(rhs.name);
            });
    }

    void UI_Prefab_Browser::Select_Directory(const fs::path& directoryPath)
    {
        const wstring normalizedDirectory = directoryPath.lexically_normal().wstring();
        if (_selectedDirectory == normalizedDirectory)
            return;

        _selectedDirectory = normalizedDirectory;
        _needsRefresh = true;
    }

    void UI_Prefab_Browser::Save_SelectedPrefab()
    {
        const string prefabName = _prefabNameBuffer[0] == '\0' ? kDefaultPrefabName : _prefabNameBuffer;
        const wstring filePath = _prefabService.Build_PrefabFilePath(_selectedDirectory, prefabName);

        if (_prefabService.Save_Prefab(_editorManager.Get_Document(), _editorManager.Get_RootSelectedObjectIds(), prefabName, filePath))
            _needsRefresh = true;
    }

    void UI_Prefab_Browser::Create_PrefabInstance(const wstring& filePath)
    {
        UI_PrefabAsset prefab = {};
        if (!_prefabService.Load_Prefab(filePath, prefab))
            return;

        if (_editorManager.Create_UIObjectsFromPrefab(prefab))
            _needsRefresh = true;
    }

    string UI_Prefab_Browser::Make_RelativeDisplayPath(const fs::path& path) const
    {
        try
        {
            const fs::path rootDirectory = fs::path(_prefabService.Get_RootDirectory()).lexically_normal();
            const fs::path normalizedPath = path.lexically_normal();
            const fs::path relativePath = fs::relative(normalizedPath, rootDirectory);
            const string relativeText = String::ToString(relativePath.wstring());
            return relativeText.empty() || relativeText == "." ? "UI_Prefabs" : relativeText;
        }
        catch (...)
        {
            return String::ToString(path.filename().wstring());
        }
    }

    Shared<UI_Prefab_Browser> UI_Prefab_Browser::Create(UI_Editor_Manager& editorManager)
    {
        return make_shared<UI_Prefab_Browser>(editorManager);
    }
}

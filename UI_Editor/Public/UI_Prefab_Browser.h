#pragma once

#include "UI_Editor_Window.h"
#include "UI_PrefabService.h"

namespace UIEditor
{
    class UI_Editor_Manager;

    struct UI_PrefabBrowserEntry
    {
        UI_PrefabFileEntry prefabEntry = {};
        string searchHaystackLower = "";
    };

    class UI_Prefab_Browser final : public UI_Editor_Window
    {
    public:
        explicit UI_Prefab_Browser(UI_Editor_Manager& editorManager);
        ~UI_Prefab_Browser() override = default;

    public:
        void Render() override;

    private:
        void Render_FolderTree();
        void Render_FolderNode(const fs::path& directoryPath);
        void Render_SelectedFolderPanel();
        void Render_SaveControls();
        void Render_CreateFolderControls();
        void Refresh_PrefabEntries();
        void Refresh_RootDirectoryState();
        void Clear_FolderTreeCache();
        const vector<fs::path>& Get_CachedChildDirectories(const fs::path& directoryPath);
        void Select_Directory(const fs::path& directoryPath);
        void Save_SelectedPrefab();
        void Create_PrefabInstance(const wstring& filePath);

    public:
        static Shared<UI_Prefab_Browser> Create(UI_Editor_Manager& editorManager);

    private:
        UI_Editor_Manager& _editorManager;
        UI_PrefabService _prefabService = {};
        wstring _selectedDirectory = {};
        vector<UI_PrefabBrowserEntry> _prefabEntries = {};
        bool _needsRefresh = true;
        bool _rootDirectoryReady = false;
        unordered_map<wstring, vector<fs::path>> _childDirectoryCache = {};
        char _searchBuffer[128] = {};
        char _prefabNameBuffer[128] = {};
        char _newFolderNameBuffer[128] = {};
    };
}

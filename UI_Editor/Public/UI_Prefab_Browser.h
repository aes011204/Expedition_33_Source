#pragma once

#include "UI_Editor_Window.h"
#include "UI_PrefabService.h"

namespace UIEditor
{
    class UI_Editor_Manager;

    class UI_Prefab_Browser final : public UI_Editor_Window
    {
    public:
        explicit UI_Prefab_Browser(UI_Editor_Manager& editorManager);
        ~UI_Prefab_Browser() override = default;

    public:
        void Render() override;

    private:
        struct PrefabFileEntry
        {
            wstring filePath = {};
            string name = {};
            string relativePath = {};
        };

    private:
        void Render_FolderTree();
        void Render_FolderNode(const fs::path& directoryPath);
        void Render_SelectedFolderPanel();
        void Render_SaveControls();
        void Render_CreateFolderControls();
        void Refresh_PrefabEntries();
        void Select_Directory(const fs::path& directoryPath);
        void Save_SelectedPrefab();
        void Create_PrefabInstance(const wstring& filePath);
        string Make_RelativeDisplayPath(const fs::path& path) const;

    public:
        static Shared<UI_Prefab_Browser> Create(UI_Editor_Manager& editorManager);

    private:
        UI_Editor_Manager& _editorManager;
        UI_PrefabService _prefabService = {};
        wstring _selectedDirectory = {};
        vector<PrefabFileEntry> _prefabEntries = {};
        bool _needsRefresh = true;
        char _searchBuffer[128] = {};
        char _prefabNameBuffer[128] = {};
        char _newFolderNameBuffer[128] = {};
    };
}

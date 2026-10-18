#pragma once

#include "UI_Document.h"

namespace UIEditor
{
    struct UI_PrefabAsset
    {
        string name = "";
        vector<ObjectId> rootObjectIds = {};
        vector<UI_Object> objects = {};
    };

    struct UI_PrefabFileEntry
    {
        wstring filePath = {};
        string name = {};
        string relativePath = {};
    };

    class UI_PrefabService final
    {
    public:
        UI_PrefabService() = default;
        ~UI_PrefabService() = default;

    public:
        HRESULT Initialize();
        const wstring& Get_RootDirectory() const { return _rootDirectory; }
        const char* Get_DefaultPrefabName() const;
        bool Ensure_RootDirectory() const;
        bool Is_PrefabFilePath(const fs::path& filePath) const;
        vector<fs::path> Collect_ChildDirectories(const fs::path& directoryPath) const;
        bool Create_Folder(const wstring& parentDirectory, const string& folderName, wstring& outDirectory) const;
        vector<UI_PrefabFileEntry> Collect_PrefabEntries(const wstring& directoryPath) const;
        string Make_RelativeDisplayPath(const fs::path& path) const;
        wstring Build_PrefabFilePath(const wstring& directoryPath, const string& prefabName) const;
        bool Save_Prefab(const UI_Document& document, const vector<ObjectId>& rootObjectIds, const string& prefabName, const wstring& filePath) const;
        bool Load_Prefab(const wstring& filePath, UI_PrefabAsset& outPrefab) const;

    private:
        wstring Resolve_PrefabRootDirectory() const;

    private:
        wstring _rootDirectory = {};
    };
}

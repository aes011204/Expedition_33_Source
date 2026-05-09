#pragma once
#include "EditorObject.h"

NS_BEGIN(MapEditor)

struct SceneContext
{
    vector<Shared<EditorObject>> _objects;
    Shared<EditorObject> _selected;
    wstring _mapName = L"Untitled";
    wstring _modelFolder = {};

    bool _dirtyRenderHandles = false;

    Vec3 _cursorPos = {};
    bool _cursorVisible = false;

    void SaveToFile(const string& filePath);
    void LoadFromFile(const string& filePath);
    Shared<EditorObject> SpawnObject(const wstring& modelPath, const Vec3& pos);
    Shared<EditorObject> SpawnFromPrefab(const string& prefabPath, const Vec3& pos);


    void Add(Shared<EditorObject> obj);

    void Remove(const Shared<EditorObject>& obj);

    Shared<EditorObject> Duplicate(const Shared<EditorObject>& src);
};

NS_END

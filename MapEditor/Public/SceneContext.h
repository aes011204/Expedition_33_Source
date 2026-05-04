#pragma once
#include "EditorObject.h"

NS_BEGIN(MapEditor)

struct SceneContext
{
    vector<Shared<EditorObject>> _objects;
    Shared<EditorObject> _selected;
    wstring _mapName = L"Untitled";

    void SaveToFile(const string& filePath);
    void LoadFromFile(const string& filePath);

    void Add(Shared<EditorObject> obj);

    void Remove(const Shared<EditorObject>& obj);

    Shared<EditorObject> Duplicate(const Shared<EditorObject>& src);
};

NS_END

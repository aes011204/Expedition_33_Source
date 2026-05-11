#pragma once
#include "EditorObject.h"

NS_BEGIN(MapEditor)

class IEditorCommand;

struct SceneContext
{
    vector<Shared<IEditorCommand>> _undoStack;
    vector<Shared<IEditorCommand>> _redoStack;

    vector<Shared<EditorObject>> _objects;
    vector<Shared<EditorObject>> _staticMeshes;
    vector<Shared<EditorObject>> _lights;
    vector<Shared<EditorObject>> _instancedMeshes;

    Shared<EditorObject> _selected;
    wstring _mapName = L"Untitled";
    wstring _modelFolder = {};

    bool _dirtyRenderHandles = false;

    Vec3 _cursorPos = {};
    bool _cursorVisible = false;

    bool _octreeDirty = true;

    void SaveToFile(const string& filePath);
    void LoadFromFile(const string& filePath);
    Shared<EditorObject> SpawnObject(const wstring& modelPath, const Vec3& pos);
    Shared<EditorObject> SpawnFromPrefab(const string& prefabPath, const Vec3& pos);

    void PushCommand(Shared<IEditorCommand> cmd);
    void UndoCommand();
    void RedoCommand();

    void Add(Shared<EditorObject> obj);
    void Remove(const Shared<EditorObject>& obj);
    Shared<EditorObject> Duplicate(const Shared<EditorObject>& src);

    void ClearScene();
};

NS_END

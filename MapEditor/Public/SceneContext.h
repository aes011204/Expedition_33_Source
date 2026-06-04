#pragma once
#include "EditorObject.h"
#include "MELandscapeActor.h"

NS_BEGIN(MapEditor)

class IEditorCommand;

struct SceneContext
{
    vector<Shared<IEditorCommand>> _undoStack;
    vector<Shared<IEditorCommand>> _redoStack;

    vector<Shared<EditorObject>> _objects;
    vector<Shared<EditorObject>> _staticMeshes;
    vector<Shared<EditorObject>> _lights;
    vector<Shared<EditorObject>> _postProcessVolumes;
    vector<Shared<EditorObject>> _instancedMeshes;

    Shared<EditorObject> _selected;
    wstring _mapName = L"Untitled";
    wstring _modelFolder = {};
    string _currentFilePath = {};

    bool _dirtyRenderHandles = false;

    Vec3 _cursorPos = {};
    bool _cursorVisible = false;

    bool _octreeDirty = true;
    float _octreeRebuildTimer = 0.f;
    static constexpr float _octreeRebuildDelay = 1.5f;

    bool _lightDirty = true;

    map<string, ModelOverrideEntry> _modelOverrides; // model filename -> forced pass index (-1 = none)
    bool _dirtyPassOverrides = false;

    void SaveToFile(const string& filePath);
    void LoadFromFile(const string& filePath, bool bMerge = false);
    Shared<EditorObject> SpawnObject(const wstring& modelPath, const Vec3& pos);
    Shared<EditorObject> SpawnFromPrefab(const string& prefabPath, const Vec3& pos);

    void PushCommand(Shared<IEditorCommand> cmd);
    void UndoCommand();
    void RedoCommand();

    void Add(Shared<EditorObject> obj);
    void Remove(const Shared<EditorObject>& obj);
    Shared<EditorObject> Duplicate(const Shared<EditorObject>& src);

    void ClearScene();

    CameraBookmark _cameraBookmarks[CameraBookmarkCount];

    void SaveCamData(const string& filePath);
    void LoadCamData(const string& filePath);

    TerrainConfig _terrainConfig;

    void LoadTerrainConfig(const string& filePath);

    bool _terrainDirty = false;

    vector<LandscapeConfig> _landscapeConfigs;
    vector<Shared<MELandscapeActor>> _landscapes;
    int32 _activeLandscapeIdx = -1;

    bool _objectBBoxMode = false;
    bool _hideInstancedMesh = false;

public: // Minor 테스트 맵 인스턴스 분할
    void Export_MapInstance(const string& filePath);
    void Import_MapInstance(const string& mapName);

};

NS_END

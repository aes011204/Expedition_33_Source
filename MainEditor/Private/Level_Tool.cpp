#include "Level_Tool.h"

#include "GameInstance.h"
#include "GameObject.h"
#include "Helper_String.h"
#include "Level_Loading.h"
#include "MapAuthoringIO.h"
#include "MapLoader.h"
#include "Notification_Manager.h"
#include "EditorInstance.h"

Level_Tool::Level_Tool()
    : Editor_Window{ L"Level Tool" }
{
    Set_Open(true);
}

HRESULT Level_Tool::Initialize()
{
    Refresh_MapJsonFiles();
    return S_OK;
}

void Level_Tool::Render()
{
    if (!Is_Open())
        return;

    if (!ImGui::Begin("Level Tool", &_isOpen))
    {
        ImGui::End();
        return;
    }

    Render_LevelSwitcher();
    ImGui::Separator();
    Render_MapLoader();

    ImGui::End();
}

void Level_Tool::Render_LevelSwitcher()
{
    ImGui::Text("Level Switcher");
    ImGui::Spacing();

    ImGui::Combo("Target Level", &_selectedLevelIndex, _levelNames, IM_ARRAYSIZE(_levelNames));

    const bool isSameLevel = (GAME->Current_LevelIndex() == ETOI(_levelTypes[_selectedLevelIndex]));
    if (isSameLevel)
        ImGui::BeginDisabled();

    if (ImGui::Button("Switch Level", ImVec2(-1, 0)))
        Change_To(_levelTypes[_selectedLevelIndex]);

    if (isSameLevel)
        ImGui::EndDisabled();
}

void Level_Tool::Render_MapLoader()
{
    ImGui::Text("Map Json Loader");
    ImGui::Spacing();

    if (ImGui::Button("Refresh"))
        Refresh_MapJsonFiles();

    ImGui::SameLine();
    ImGui::TextDisabled("(%zu files)", _jsonFiles.size());

    const fs::path mapSaveJsonDir = Resolve_MapSaveJsonDir();
    const string mapSaveJsonDirText = mapSaveJsonDir.string();
    ImGui::TextDisabled("%s", mapSaveJsonDirText.c_str());

    for (int i = 0; i < static_cast<int>(_jsonFiles.size()); ++i)
    {
        const bool selected = (_selectedJsonIndex == i);
        if (ImGui::Selectable(_jsonFiles[i].c_str(), selected))
            _selectedJsonIndex = i;
    }

    if (_selectedJsonIndex < 0 || _selectedJsonIndex >= static_cast<int>(_jsonFiles.size()))
        return;

    ImGui::Spacing();

    if (ImGui::Button("Load Map", ImVec2(-1, 0)))
        Load_SelectedMap();

    if (ImGui::Button("Reload Map", ImVec2(-1, 0)))
        Reload_SelectedMap();

    if (ImGui::Button("Save Map State", ImVec2(-1, 0)))
        Save_SelectedMapState();

    if (ImGui::Button("Reload Authored Objects", ImVec2(-1, 0)))
        Reload_SelectedAuthoredObjects();
}

void Level_Tool::Refresh_MapJsonFiles()
{
    const string previousSelection =
        (_selectedJsonIndex >= 0 && _selectedJsonIndex < static_cast<int>(_jsonFiles.size()))
        ? _jsonFiles[_selectedJsonIndex]
        : string{};

    _jsonFiles.clear();
    _selectedJsonIndex = -1;

    const fs::path mapSaveJsonDir = Resolve_MapSaveJsonDir();
    if (!fs::exists(mapSaveJsonDir) || !fs::is_directory(mapSaveJsonDir))
        return;

    for (const fs::directory_entry& entry : fs::directory_iterator(mapSaveJsonDir))
    {
        if (entry.path().extension() == ".json")
            _jsonFiles.push_back(entry.path().filename().string());
    }

    std::sort(_jsonFiles.begin(), _jsonFiles.end());

    if (!previousSelection.empty())
    {
        const auto found = std::find(_jsonFiles.begin(), _jsonFiles.end(), previousSelection);
        if (found != _jsonFiles.end())
            _selectedJsonIndex = static_cast<int>(std::distance(_jsonFiles.begin(), found));
    }
}

fs::path Level_Tool::Resolve_MapSaveJsonDir() const
{
    const wstring assetRoot = GAME->Get_AssetRoot();
    if (!assetRoot.empty())
        return fs::path(assetRoot) / L"Map" / L"SaveJson";

    return fs::path(Client::Constants::Map::SaveJson_Dir);
}

fs::path Level_Tool::Resolve_SelectedMapJsonPath() const
{
    if (_selectedJsonIndex < 0 || _selectedJsonIndex >= static_cast<int>(_jsonFiles.size()))
        return {};

    return Resolve_MapSaveJsonDir() / String::ToWString(_jsonFiles[_selectedJsonIndex]);
}

void Level_Tool::Load_SelectedMap()
{
    const fs::path selectedPath = Resolve_SelectedMapJsonPath();
    if (selectedPath.empty())
        return;

    if (FAILED(MapLoader::Load(
        GAME->Get_Device(),
        GAME->Get_Context(),
        selectedPath.wstring(),
        GAME->Current_LevelIndex(),
        L"Layer_MapStaticMesh")))
    {
        NOTIFY("Map 로드 실패");
        return;
    }

    NOTIFY("Map 로드 완료");
}

void Level_Tool::Reload_SelectedMap()
{
    const fs::path selectedPath = Resolve_SelectedMapJsonPath();
    if (selectedPath.empty())
        return;

    Remove_CurrentLevelMapObjects();
    Remove_CurrentLevelAuthoredObjects();

    if (FAILED(MapLoader::Load(
        GAME->Get_Device(),
        GAME->Get_Context(),
        selectedPath.wstring(),
        GAME->Current_LevelIndex(),
        L"Layer_MapStaticMesh")))
    {
        NOTIFY("Map 리로드 실패");
        return;
    }

    NOTIFY("Map 리로드 완료");
}

void Level_Tool::Reload_SelectedAuthoredObjects()
{
    const fs::path selectedPath = Resolve_SelectedMapJsonPath();
    if (selectedPath.empty())
        return;

    const json root = GAME->Load_JsonFile(selectedPath);
    if (!root.is_object() || !root.contains("MapData") || !root["MapData"].is_object())
    {
        NOTIFY("Authored Objects 리로드 실패");
        return;
    }

    Remove_CurrentLevelAuthoredObjects();

    vector<Shared<GameObject>> spawnedObjects{};
    if (!Client::MapAuthoringIO::Load_AuthoredObjects_FromMapData(
        root["MapData"],
        GAME->Current_LevelIndex(),
        &spawnedObjects))
    {
        NOTIFY("Authored Objects 리로드 실패");
        return;
    }

    _reloadedMonsterObjects.clear();
    for (const Shared<GameObject>& spawnedObject : spawnedObjects)
        _reloadedMonsterObjects.push_back(spawnedObject);

    NOTIFY("Authored Objects 리로드 완료");
}

void Level_Tool::Save_SelectedMapState()
{
    const fs::path selectedPath = Resolve_SelectedMapJsonPath();
    if (selectedPath.empty())
        return;

    const bool saved = Client::MapAuthoringIO::Save_MapState_ToMapJson(
        selectedPath.wstring(),
        GAME->Current_LevelIndex());

    if (saved)
        NOTIFY("Map State 저장 완료");
    else
        NOTIFY("Map State 저장 실패");
}

void Level_Tool::Remove_CurrentLevelMapObjects()
{
    vector<Shared<GameObject>> mapObjects{};

    GAME->Visit_LevelObjects(
        GAME->Current_LevelIndex(),
        [&](const wstring& layerTag, const Shared<GameObject>& gameObject)
        {
            if (layerTag == L"Layer_MapStaticMesh" && gameObject != nullptr)
                mapObjects.push_back(gameObject);
        });

    for (const Shared<GameObject>& mapObject : mapObjects)
        GAME->Remove_GameObject(GAME->Current_LevelIndex(), mapObject);

    GAME->Flush_RemovedGameObjects();
}

void Level_Tool::Remove_CurrentLevelAuthoredObjects()
{
    vector<Shared<GameObject>> authoredObjects{};
    std::unordered_set<uintptr_t> authoredObjectKeys{};

    auto QueueRemove = [&](const Shared<GameObject>& gameObject)
    {
        if (gameObject == nullptr || !gameObject->Is_RuntimeAuthoringLoaded())
            return;

        const uintptr_t objectKey = reinterpret_cast<uintptr_t>(gameObject.get());
        if (!authoredObjectKeys.insert(objectKey).second)
            return;

        authoredObjects.push_back(gameObject);
    };

    for (const Weak<GameObject>& weakObject : _reloadedMonsterObjects)
        QueueRemove(weakObject.lock());

    GAME->Visit_LevelObjects(
        GAME->Current_LevelIndex(),
        [&](const wstring&, const Shared<GameObject>& gameObject)
        {
            QueueRemove(gameObject);
        });

    for (const Shared<GameObject>& authoredObject : authoredObjects)
        GAME->Remove_GameObject(GAME->Current_LevelIndex(), authoredObject);

    GAME->Flush_RemovedGameObjects();
    _reloadedMonsterObjects.clear();
}

void Level_Tool::Change_To(LevelType targetLevel)
{
    Shared<Level_Loading> loadingLevel = Level_Loading::Create(
        GAME->Get_Device(),
        GAME->Get_Context(),
        targetLevel);

    if (loadingLevel == nullptr)
        return;

    GAME->Change_Level(ETOI(LevelType::Loading), loadingLevel);
}

Shared<Level_Tool> Level_Tool::Create()
{
    return make_shared<Level_Tool>();
}

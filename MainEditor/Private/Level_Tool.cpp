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
    Refresh_MapInstanceFolders();
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
    ImGui::Text("Map Instance Loader");
    ImGui::Spacing();

    if (ImGui::Button("Refresh"))
        Refresh_MapInstanceFolders();

    ImGui::SameLine();
    ImGui::TextDisabled("(%zu folders)", _mapInstanceFolders.size());

    const fs::path mapInstanceDir = Resolve_MapInstanceDir();
    const string mapInstanceDirText = mapInstanceDir.string();
    ImGui::TextDisabled("%s", mapInstanceDirText.c_str());

    for (int i = 0; i < static_cast<int>(_mapInstanceFolders.size()); ++i)
    {
        const bool selected = (_selectedMapInstanceIndex == i);
        if (ImGui::Selectable(_mapInstanceFolders[i].c_str(), selected))
            _selectedMapInstanceIndex = i;
    }

    if (_selectedMapInstanceIndex < 0 || _selectedMapInstanceIndex >= static_cast<int>(_mapInstanceFolders.size()))
        return;

    ImGui::Spacing();

    if (ImGui::Button("Load Map", ImVec2(-1, 0)))
        Load_SelectedMap();

    if (ImGui::Button("Reload Map", ImVec2(-1, 0)))
        Reload_SelectedMap();

    if (ImGui::Button("Save Prefab Instances", ImVec2(-1, 0)))
        Save_SelectedPrefabInstances();

    ImGui::TextDisabled("Saved to selected MapInstance/PrefabInstances.json.");
}

void Level_Tool::Refresh_MapInstanceFolders()
{
    const string previousSelection =
        (_selectedMapInstanceIndex >= 0 && _selectedMapInstanceIndex < static_cast<int>(_mapInstanceFolders.size()))
        ? _mapInstanceFolders[_selectedMapInstanceIndex]
        : string{};

    _mapInstanceFolders.clear();
    _selectedMapInstanceIndex = -1;

    const fs::path mapInstanceDir = Resolve_MapInstanceDir();
    if (!fs::exists(mapInstanceDir) || !fs::is_directory(mapInstanceDir))
        return;

    for (const fs::directory_entry& entry : fs::directory_iterator(mapInstanceDir))
    {
        if (entry.is_directory())
            _mapInstanceFolders.push_back(entry.path().filename().string());
    }

    std::sort(_mapInstanceFolders.begin(), _mapInstanceFolders.end());

    if (!previousSelection.empty())
    {
        const auto found = std::find(_mapInstanceFolders.begin(), _mapInstanceFolders.end(), previousSelection);
        if (found != _mapInstanceFolders.end())
            _selectedMapInstanceIndex = static_cast<int>(std::distance(_mapInstanceFolders.begin(), found));
    }
}

fs::path Level_Tool::Resolve_MapInstanceDir() const
{
    const wstring assetRoot = GAME->Get_AssetRoot();
    if (!assetRoot.empty())
        return fs::path(assetRoot) / L"Map" / L"MapInstance";

    return fs::path(L"../../../Client/Bin/Resources/Map/MapInstance");
}

fs::path Level_Tool::Resolve_SelectedMapInstancePath() const
{
    if (_selectedMapInstanceIndex < 0 || _selectedMapInstanceIndex >= static_cast<int>(_mapInstanceFolders.size()))
        return {};

    return Resolve_MapInstanceDir() / String::ToWString(_mapInstanceFolders[_selectedMapInstanceIndex]);
}

void Level_Tool::Load_SelectedMap()
{
    const fs::path selectedPath = Resolve_SelectedMapInstancePath();
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
    const fs::path selectedPath = Resolve_SelectedMapInstancePath();
    if (selectedPath.empty())
        return;

    Remove_CurrentLevelMapObjects();

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

void Level_Tool::Save_SelectedPrefabInstances()
{
    const fs::path selectedPath = Resolve_SelectedMapInstancePath();
    if (selectedPath.empty())
        return;

    const bool saved = Client::MapAuthoringIO::Save_PrefabInstances_ToMapInstanceAuthoring(
        selectedPath.wstring(),
        GAME->Current_LevelIndex());

    if (saved)
        NOTIFY("Prefab Instances 저장 완료");
    else
        NOTIFY("Prefab Instances 저장 실패");
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

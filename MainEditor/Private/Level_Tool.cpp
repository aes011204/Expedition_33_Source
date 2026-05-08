#include "Level_Tool.h"
#include "GameInstance.h"
#include "Level_Loading.h"

Level_Tool::Level_Tool()
    : Editor_Window{ L"Level Tool" }
{
    Set_Open(false);
}

HRESULT Level_Tool::Initialize()
{
    

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

    if (ImGui::Button("Switch Level", ImVec2(-1, 0)))
    {
        Change_To(_levelTypes[_selectedLevelIndex]);
    }
}

void Level_Tool::Render_MapLoader()
{
    ImGui::Text("Map Json Loader");
    ImGui::Spacing();

    if (ImGui::Button("Refresh"))
    {
        _jsonFiles.clear();
        string dirPath = String::ToString(Client::Constants::Map::SaveJson_Dir);
        if (fs::exists(dirPath))
        {
            for (auto& entry : fs::directory_iterator(dirPath))
            {
                if (entry.path().extension() == ".json")
                {
                    _jsonFiles.push_back(entry.path().filename().string());
                }
            }
        }
    }

    ImGui::SameLine();
    ImGui::TextDisabled("(%zu files)", _jsonFiles.size());

    for (int i = 0; i < static_cast<int>(_jsonFiles.size()); ++i)
    {
        bool selected = (_selectedJsonIndex == i);
        if (ImGui::Selectable(_jsonFiles[i].c_str(), selected))
        {
            _selectedJsonIndex = i;
        }
    }

    if (_selectedJsonIndex >= 0 && _selectedJsonIndex < static_cast<int>(_jsonFiles.size()))
    {
        if (ImGui::Button("Load Map", ImVec2(-1, 0)))
        {
            wstring fullPath = wstring(Client::Constants::Map::SaveJson_Dir) +
                String::ToWString(_jsonFiles[_selectedJsonIndex]);
            // TODO: 현재 레벨의 MapStaticMesh 레이어에 로드
            // MapLoader::Load(_device, _context, fullPath, GAME->Current_LevelIndex(), L"Layer_MapStaticMesh");
        }
    }
}

void Level_Tool::Change_To(LevelType targetLevel)
{
    auto loadingLevel = Level_Loading::Create(
        GAME->Get_Device(), GAME->Get_Context(), targetLevel);

    if (nullptr == loadingLevel)
    {
        return;
    }

    GAME->Change_Level(ETOI(LevelType::Loading), loadingLevel);
}

Shared<Level_Tool> Level_Tool::Create()
{
    return make_shared<Level_Tool>();
}

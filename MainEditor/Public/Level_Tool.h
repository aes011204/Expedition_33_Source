#pragma once
#include "Editor_Window.h"
#include "Client_Defines.h"
#include <filesystem>

NS_BEGIN(Engine)
class GameObject;
NS_END

NS_BEGIN(MainEditor)

class Level_Tool : public Editor_Window
{
private:
    static constexpr const char* _levelNames[] = { "GamePlay", "MapTest", "Stage01" };
    static constexpr LevelType _levelTypes[] = { LevelType::GamePlay, LevelType::MapTest, LevelType::Stage01 };

public:
    Level_Tool();
    virtual ~Level_Tool() = default;

public:
    HRESULT Initialize() override;
    virtual void Render() override;

private:
    void Render_LevelSwitcher();
    void Render_MapLoader();

    void Refresh_MapJsonFiles();
    std::filesystem::path Resolve_MapSaveJsonDir() const;
    std::filesystem::path Resolve_SelectedMapJsonPath() const;

    void Load_SelectedMap();
    void Reload_SelectedMap();
    void Reload_SelectedPrefabInstances();
    void Save_SelectedMapPrefabInstances();

    void Remove_CurrentLevelMapObjects();
    void Remove_CurrentLevelAuthoredPrefabInstances();

    void Change_To(LevelType targetLevel);

private:
    int _selectedLevelIndex = { 0 };
    int _selectedJsonIndex = { -1 };
    vector<string> _jsonFiles = {};
    vector<Weak<GameObject>> _reloadedMonsterObjects = {};

public:
    static Shared<Level_Tool> Create();
};

NS_END

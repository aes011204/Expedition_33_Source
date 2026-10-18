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
    static constexpr const char* _levelNames[] = { "GamePlay", "MapTest", "Stage01", "Simon", "Gestral Village" };
    static constexpr LevelType _levelTypes[] = { LevelType::GamePlay, LevelType::MapTest, LevelType::Stage01, LevelType::Stage01, LevelType::GestralVillage };

public:
    Level_Tool();
    virtual ~Level_Tool() = default;

public:
    HRESULT Initialize() override;
    virtual void Render() override;

private:
    void Render_LevelSwitcher();
    void Render_MapLoader();

    void Refresh_MapInstanceFolders();
    std::filesystem::path Resolve_MapInstanceDir() const;
    std::filesystem::path Resolve_SelectedMapInstancePath() const;

    void Load_SelectedMap();
    void Reload_SelectedMap();
    void Save_SelectedPrefabInstances();

    void Remove_CurrentLevelMapObjects();

    void Change_To(LevelType targetLevel);

private:
    int _selectedLevelIndex = { 0 };
    int _selectedMapInstanceIndex = { -1 };
    vector<string> _mapInstanceFolders = {};

public:
    static Shared<Level_Tool> Create();
};

NS_END

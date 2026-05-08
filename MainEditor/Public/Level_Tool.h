#pragma once
#include "Editor_Window.h"
#include "Client_Defines.h"

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
    virtual void Render() override;

private:
    void Render_LevelSwitcher();
    void Render_MapLoader();

    void Change_To(LevelType targetLevel);

private:
    int _selectedLevelIndex = { 0 };
    int _selectedJsonIndex = { -1 };
    vector<string> _jsonFiles;

public:
    static Shared<Level_Tool> Create();
};

NS_END

#include "Level_Manager.h"
#include "GameInstance.h"
#include "Level.h"

Level_Manager::Level_Manager()
{
}

void Level_Manager::Update(float timeDelta)
{
    if (nullptr != _currentLevel)
    {
        _currentLevel->Try_BeginPlay();
        _currentLevel->Update(timeDelta);
    }
}

HRESULT Level_Manager::Render()
{
    if (nullptr != _currentLevel)
        _currentLevel->Render();

    return S_OK;
}

HRESULT Level_Manager::Change_Level(uint32 newLevelIndex, const Shared<Level>& newLevel, bool clearPreviousLevelPrototypes)
{
    if (nullptr != _currentLevel)
    {
        _currentLevel->Free();
        GAME->Clear_Resources(_currentLevelIndex, clearPreviousLevelPrototypes);
    }

    _currentLevel.reset();
    _currentLevel = newLevel;
    _currentLevelIndex = newLevelIndex;

    return S_OK;
}

Unique<Level_Manager> Level_Manager::Create()
{
    return Unique<Level_Manager>{ new Level_Manager() };
}

void Level_Manager::Free()
{
    if (nullptr != _currentLevel)
    {
        _currentLevel->Free();
        _currentLevel.reset();
    }

    __super::Free();
}

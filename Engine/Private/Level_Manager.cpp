#include "Level_Manager.h"
#include "GameInstance.h"
#include "Level.h"

Level_Manager::Level_Manager()
{
}

HRESULT Level_Manager::Change_Level(uint32 iNewLevelIndex, Shared<Level> pNewLevel)
{
	if (nullptr != _currentLevel)
		GAME->Clear_Resources(_currentLevelIndex);

	_currentLevel.reset();
	_currentLevel = pNewLevel;
	_currentLevelIndex = iNewLevelIndex;

	return S_OK;
}

void Level_Manager::Update(float fTimeDelta)
{
	if (nullptr != _currentLevel)
		_currentLevel->Update(fTimeDelta);
}

HRESULT Level_Manager::Render()
{
	if (nullptr != _currentLevel)
		_currentLevel->Render();

	return S_OK;
}

Unique<Level_Manager> Level_Manager::Create()
{
	return Unique<Level_Manager>{ new Level_Manager() };
}

void Level_Manager::Free()
{
	__super::Free();
	_currentLevel.reset();
}

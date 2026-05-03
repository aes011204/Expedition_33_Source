#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class Level;

class Level_Manager final : public Base
{
public:
	Level_Manager();
	virtual ~Level_Manager() = default;

public:
	void    Update(float timeDelta);
	HRESULT Render();

public:
	HRESULT Change_Level(uint32 newLevelIndex, Shared<Level> newLevel);

    uint32  Get_CurrentLevelIndex() { return _currentLevelIndex; }
    Shared<Level> Get_CurrentLevelType() { return _currentLevel; }

private:
	Shared<Level> _currentLevel = nullptr;
	uint32 _currentLevelIndex = {};

public:
	static Unique<Level_Manager> Create();
	virtual void Free() override;
};

NS_END

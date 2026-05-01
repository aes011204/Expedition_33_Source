#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class Level_Manager final : public Base
{
private:
	Level_Manager();

public:
	virtual ~Level_Manager() = default;
	HRESULT Change_Level(uint32 newLevelIndex, Shared<class Level> newLevel);
	void Update(float timeDelta);
	HRESULT Render();

private:
	Shared<class Level> _currentLevel = nullptr;
	uint32 _currentLevelIndex = {};

public:
	static Unique<Level_Manager> Create();
	virtual void Free() override;
};

NS_END

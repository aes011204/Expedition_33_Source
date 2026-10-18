#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class ENGINE_DLL Timer : public Base
{
public:
	Timer();
	virtual ~Timer() = default;


public:
	HRESULT			Ready_Timer();
	float Update_Timer();

private:
	LARGE_INTEGER		_frameTime = {};
	LARGE_INTEGER		_fixTime = {};
	LARGE_INTEGER		_lastTime = {};
	LARGE_INTEGER		_cpuTick = {};

	float				_timeDelta = {};

public:
	static Shared<Timer> Create();

public:
	virtual void	Free();

};

NS_END

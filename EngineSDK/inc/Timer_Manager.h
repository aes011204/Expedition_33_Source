#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class Timer;
class Timer_Manager : public Base
{
private:
	Timer_Manager();

public:
	virtual ~Timer_Manager() = default;

public:
	HRESULT			Add_Timer(const wstring& strTimerTag);
	float			Compute_TimeDelta(const wstring& strTimerTag);

private:
	map<wstring, Shared<Timer>>		_timers;

private:
	Shared<Timer> Find_Timer(const wstring& strTimerTag);


public:
	static Unique<Timer_Manager> Create();
	virtual void		Free();
};

NS_END

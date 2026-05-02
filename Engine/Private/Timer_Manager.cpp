#include "Timer_Manager.h"
#include "Timer.h"

Timer_Manager::Timer_Manager()
{

}


HRESULT Timer_Manager::Add_Timer(const wstring& timerTag)
{
	Shared<Timer> timer = Find_Timer(timerTag);
	if (nullptr != timer)
		return E_FAIL;

	timer = Timer::Create();
	if (nullptr == timer)
		return E_FAIL;

	_timers.emplace(timerTag, timer);

	return S_OK;
}

float Timer_Manager::Compute_TimeDelta(const wstring& timerTag)
{
	Shared<Timer> timer = Find_Timer(timerTag);
	if (nullptr == timer)
		return 0.f;

	return timer->Update_Timer();
}

Shared<Timer> Timer_Manager::Find_Timer(const wstring& timerTag)
{
	auto		iter = _timers.find(timerTag);
	if (iter == _timers.end())
		return nullptr;

	return iter->second;
}

Unique<Timer_Manager> Timer_Manager::Create()
{
	return Unique<Timer_Manager>{ new Timer_Manager() };
}

void Timer_Manager::Free()
{
	__super::Free();

	for (auto& Pair : _timers)	
		Safe_Release(Pair.second);
	_timers.clear();
}


#include "Timer_Manager.h"
#include "Timer.h"

Timer_Manager::Timer_Manager()
{

}


HRESULT Timer_Manager::Add_Timer(const wstring& strTimerTag)
{
	Shared<Timer> pTimer = Find_Timer(strTimerTag);
	if (nullptr != pTimer)
		return E_FAIL;

	pTimer = Timer::Create();
	if (nullptr == pTimer)
		return E_FAIL;

	_timers.emplace(strTimerTag, pTimer);

	return S_OK;
}

float Timer_Manager::Compute_TimeDelta(const wstring& strTimerTag)
{
	Shared<Timer> pTimer = Find_Timer(strTimerTag);
	if (nullptr == pTimer)
		return 0.f;

	return pTimer->Update_Timer();
}

Shared<Timer> Timer_Manager::Find_Timer(const wstring& strTimerTag)
{
	auto		iter = _timers.find(strTimerTag);
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


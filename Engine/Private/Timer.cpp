#include "Timer.h"

Timer::Timer() 
	: _timeDelta(0.f)
{
	ZeroMemory(&_frameTime, sizeof(LARGE_INTEGER));
	ZeroMemory(&_lastTime, sizeof(LARGE_INTEGER));
	ZeroMemory(&_frameTime, sizeof(LARGE_INTEGER));
	ZeroMemory(&_cpuTick, sizeof(LARGE_INTEGER));
}

HRESULT Timer::Ready_Timer()
{
	QueryPerformanceCounter(&_frameTime);			// 1077
	QueryPerformanceCounter(&_lastTime);			// 1085
	QueryPerformanceCounter(&_frameTime);			// 1090


	return S_OK;
}

float Timer::Update_Timer()
{
	QueryPerformanceCounter(&_frameTime);			// 1500


	if (_frameTime.QuadPart - _frameTime.QuadPart >= _cpuTick.QuadPart)
	{
		QueryPerformanceFrequency(&_cpuTick);
		_frameTime = _frameTime;
	}


	_timeDelta = (_frameTime.QuadPart - _lastTime.QuadPart) 
		/ static_cast<float>(_cpuTick.QuadPart);

	_lastTime = _frameTime;

	return _timeDelta;
}

Shared<Timer> Timer::Create()
{
	auto instance = make_shared<Timer>();

	if (FAILED(instance->Ready_Timer()))
	{
		return nullptr;
	}

	return instance;
}

void Timer::Free()
{
	__super::Free();

}


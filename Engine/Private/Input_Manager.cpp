#include "Input_Manager.h"

Input_Manager::Input_Manager()
{
}

HRESULT Input_Manager::Initialize(HWND hWnd)
{
	if (nullptr == hWnd)
		return E_FAIL;

	_windowHandle = hWnd;
	_keyStates.assign(256, KEY_STATE::NONE);

	RECT ClientRect = {};
	GetClientRect(_windowHandle, &ClientRect);
	_screenCenter.x = (ClientRect.right - ClientRect.left) / 2;
	_screenCenter.y = (ClientRect.bottom - ClientRect.top) / 2;

	Reset();

	return S_OK;
}

void Input_Manager::Update(float timeDelta)
{
	_mouseWheelDelta = 0.f;

	if (_isInputBlocked)
	{
		Reset();
		return;
	}

	if (_windowHandle != GetActiveWindow())
	{
		Reset();
		if (_isMouseLocked)
			Unlock_Mouse();
		return;
	}

	BYTE KeyBuffer[256] = {};
	if (FALSE == GetKeyboardState(KeyBuffer))
		return;

	for (uint32 key = 0; key < _keyStates.size(); ++key)
	{
		KEY_STATE& state = _keyStates[key];
		if (KeyBuffer[key] & 0x80)
			state = (KEY_STATE::PRESS == state || KEY_STATE::DOWN == state) ? KEY_STATE::PRESS : KEY_STATE::DOWN;
		else
			state = (KEY_STATE::PRESS == state || KEY_STATE::DOWN == state) ? KEY_STATE::UP : KEY_STATE::NONE;
	}

	POINT CurrentPosition = {};
	GetCursorPos(&CurrentPosition);
	ScreenToClient(_windowHandle, &CurrentPosition);

	_mouseDelta.x = static_cast<float>(CurrentPosition.x - _mousePosition.x);
	_mouseDelta.y = static_cast<float>(CurrentPosition.y - _mousePosition.y);

	if (_isMouseLocked)
	{
		POINT CenterScreen = _screenCenter;
		ClientToScreen(_windowHandle, &CenterScreen);
		SetCursorPos(CenterScreen.x, CenterScreen.y);
		_mousePosition = _screenCenter;
	}
	else
	{
		_mousePosition = CurrentPosition;
	}
}

void Input_Manager::Reset()
{
	for (KEY_STATE& state : _keyStates)
		state = KEY_STATE::NONE;

	_mouseDelta = Vec2::Zero;
	_mouseWheelDelta = 0.f;

	if (nullptr != _windowHandle)
	{
		POINT CurrentPosition = {};
		GetCursorPos(&CurrentPosition);
		ScreenToClient(_windowHandle, &CurrentPosition);
		_mousePosition = CurrentPosition;
	}
	else
	{
		_mousePosition = {};
	}
}

bool Input_Manager::KeyPress(KEY_TYPE key) const
{
	return KEY_STATE::PRESS == Get_State(key);
}

bool Input_Manager::KeyDown(KEY_TYPE key) const
{
	return KEY_STATE::DOWN == Get_State(key);
}

bool Input_Manager::KeyUp(KEY_TYPE key) const
{
	return KEY_STATE::UP == Get_State(key);
}

void Input_Manager::Lock_Mouse()
{
	if (_isMouseLocked || nullptr == _windowHandle)
		return;

	_isMouseLocked = true;
	ShowCursor(FALSE);

	POINT CenterScreen = _screenCenter;
	ClientToScreen(_windowHandle, &CenterScreen);
	SetCursorPos(CenterScreen.x, CenterScreen.y);

	RECT ClientRect = {};
	GetClientRect(_windowHandle, &ClientRect);
	ClientToScreen(_windowHandle, reinterpret_cast<POINT*>(&ClientRect.left));
	ClientToScreen(_windowHandle, reinterpret_cast<POINT*>(&ClientRect.right));
	ClipCursor(&ClientRect);

	_mousePosition = _screenCenter;
}

void Input_Manager::Unlock_Mouse()
{
	if (!_isMouseLocked)
		return;

	_isMouseLocked = false;
	ShowCursor(TRUE);
	ClipCursor(nullptr);
}

KEY_STATE Input_Manager::Get_State(KEY_TYPE key) const
{
	const uint32 index = static_cast<uint32>(key);
	if (index >= _keyStates.size())
		return KEY_STATE::NONE;

	return _keyStates[index];
}

Unique<Input_Manager> Input_Manager::Create(HWND hWnd)
{
	Unique<Input_Manager> instance{ new Input_Manager() };
	if (FAILED(instance->Initialize(hWnd)))
	{
		MSG_BOX("Failed to Created : Input_Manager");
		instance->Free();
		return nullptr;
	}

	return instance;
}

void Input_Manager::Free()
{
	Unlock_Mouse();
	_keyStates.clear();

	__super::Free();
}

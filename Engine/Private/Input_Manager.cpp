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
	if (_isInputBlocked)
	{
		if (_isEditorMouseLockSessionActive)
			End_EditorMouseLockSession();
		if (_isMouseLocked)
			Unlock_Mouse();

		Reset();
		return;
	}

	if (_windowHandle != GetActiveWindow())
	{
		const bool canKeepEditorMouseLock =
			_isEditorMouseLockSessionActive &&
			Is_WindowOwnedByCurrentProcess(GetForegroundWindow());

		if (!canKeepEditorMouseLock)
		{
			if (_isEditorMouseLockSessionActive)
				End_EditorMouseLockSession();

			Reset();
			if (_isMouseLocked)
				Unlock_Mouse();
			return;
		}
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
		const POINT centerScreen = Get_ActiveMouseLockCenterScreen();
		SetCursorPos(centerScreen.x, centerScreen.y);

		POINT centerClient = centerScreen;
		ScreenToClient(_windowHandle, &centerClient);
		_mousePosition = centerClient;
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

float Input_Manager::Get_MouseWheel()
{
    float mouseWheel =_mouseWheelDelta;

    _mouseWheelDelta = 0.f;

    return mouseWheel;
}

void Input_Manager::Lock_Mouse()
{
	if (_isMouseLocked || nullptr == _windowHandle)
		return;

	POINT currentPosition = {};
	GetCursorPos(&currentPosition);
	_mouseRestorePosition = currentPosition;

	_isMouseLocked = true;
	ShowCursor(FALSE);

	const POINT CenterScreen = Get_ActiveMouseLockCenterScreen();
	SetCursorPos(CenterScreen.x, CenterScreen.y);

	const RECT lockRectScreen = Get_ActiveMouseLockRectScreen();
	ClipCursor(&lockRectScreen);

	POINT centerClient = CenterScreen;
	ScreenToClient(_windowHandle, &centerClient);
	_mousePosition = centerClient;
}

void Input_Manager::Unlock_Mouse()
{
	if (!_isMouseLocked)
		return;

	_isMouseLocked = false;
	ShowCursor(TRUE);
	ClipCursor(nullptr);

    SetCursorPos(_mouseRestorePosition.x, _mouseRestorePosition.y);

    POINT currentPosition = _mouseRestorePosition;
    ScreenToClient(_windowHandle, &currentPosition);

    _mousePosition = currentPosition;
    _mouseDelta = Vec2::Zero;
}

void Input_Manager::Begin_EditorMouseLockSession()
{
	_isEditorMouseLockSessionActive = true;
}

void Input_Manager::End_EditorMouseLockSession()
{
	_isEditorMouseLockSessionActive = false;
	Clear_MouseLockOverrideRect();
}

void Input_Manager::Set_MouseLockOverrideRect(const RECT& lockRectScreen)
{
	_mouseLockOverrideRect = lockRectScreen;
	_hasMouseLockOverrideRect = true;
}

void Input_Manager::Clear_MouseLockOverrideRect()
{
	_hasMouseLockOverrideRect = false;
	_mouseLockOverrideRect = {};
}

KEY_STATE Input_Manager::Get_State(KEY_TYPE key) const
{
	const uint32 index = static_cast<uint32>(key);
	if (index >= _keyStates.size())
		return KEY_STATE::NONE;

	return _keyStates[index];
}

bool Input_Manager::Is_WindowOwnedByCurrentProcess(HWND hWnd)
{
	if (nullptr == hWnd)
		return false;

	DWORD processId = 0;
	GetWindowThreadProcessId(hWnd, &processId);
	return processId == GetCurrentProcessId();
}

RECT Input_Manager::Get_ActiveMouseLockRectScreen() const
{
	if (_hasMouseLockOverrideRect)
		return _mouseLockOverrideRect;

	RECT clientRect = {};
	GetClientRect(_windowHandle, &clientRect);
	ClientToScreen(_windowHandle, reinterpret_cast<POINT*>(&clientRect.left));
	ClientToScreen(_windowHandle, reinterpret_cast<POINT*>(&clientRect.right));
	return clientRect;
}

POINT Input_Manager::Get_ActiveMouseLockCenterScreen() const
{
	const RECT lockRectScreen = Get_ActiveMouseLockRectScreen();
	POINT centerScreen = {};
	centerScreen.x = (lockRectScreen.left + lockRectScreen.right) / 2;
	centerScreen.y = (lockRectScreen.top + lockRectScreen.bottom) / 2;
	return centerScreen;
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

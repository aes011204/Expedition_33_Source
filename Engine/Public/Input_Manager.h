#pragma once

#include "Base.h"

NS_BEGIN(Engine)

enum class KEY_TYPE
{
	UP  = VK_UP,
	DOWN = VK_DOWN,
	LEFT = VK_LEFT,
	RIGHT = VK_RIGHT,
	SHIFT = VK_SHIFT,
	SPACE = VK_SPACE,
	DEL = VK_DELETE,
	CTRL = VK_CONTROL,
	ENTER = VK_RETURN,
	ESCAPE = VK_ESCAPE,
	BACK = VK_BACK,
	TAB = VK_TAB,
    ALT = VK_MENU,

	F1 = VK_F1,
	F2 = VK_F2,
	F3 = VK_F3,
	F4 = VK_F4,
	F5 = VK_F5,
	F8 = VK_F8,
	F9 = VK_F9,
	F11 = VK_F11,

	W = 'W',
	A = 'A',
	S = 'S',
	D = 'D',
	Q = 'Q',
	E = 'E',
    F = 'F',
	Z = 'Z',
	C = 'C',
	V = 'V',
	R = 'R',

	KEY_0 = '0',
	KEY_1 = '1',
	KEY_2 = '2',
	KEY_3 = '3',
	KEY_4 = '4',

	LBUTTON = VK_LBUTTON,
	RBUTTON = VK_RBUTTON,
	LCTRL = VK_LCONTROL,
};

enum class KEY_STATE
{
	NONE,
	PRESS,
	DOWN,
	UP,
	END
};

class ENGINE_DLL Input_Manager final :   public Base
{
private:
	Input_Manager();

public:
	virtual ~Input_Manager() = default;
	HRESULT Initialize(HWND hWnd);
	void Update(float timeDelta);

	void Reset();
	bool KeyPress(KEY_TYPE key) const;

	bool KeyDown(KEY_TYPE key) const;
	bool KeyUp(KEY_TYPE key) const;

	const POINT& Get_MousePosition() const { return _mousePosition; }

	Vec2  Get_MouseDelta() const { return _mouseDelta; }
	float Get_MouseWheel();

	void Set_MouseWheel(float fDelta) { _mouseWheelDelta = fDelta; }

	void Lock_Mouse();

	void Unlock_Mouse();
	
	// 에디터 뷰 전환 중 포커스가 흔들려도 마우스 락을 유지할 때 시작한다.
	void Begin_EditorMouseLockSession();
	// 에디터 뷰 전환용 마우스 락 유지 상태를 끝낸다.
	void End_EditorMouseLockSession();
	// Scene View 같은 부분 뷰 기준으로 마우스 락 영역을 임시 지정한다.
	void Set_MouseLockOverrideRect(const RECT& lockRectScreen);
	// 임시 마우스 락 영역을 해제하고 기본 창 영역으로 되돌린다.
	void Clear_MouseLockOverrideRect();

	bool Is_MouseLocked() const { return _isMouseLocked; }
	bool Is_InputBlocked() const { return _isInputBlocked; }
	void Set_InputBlocked(bool isBlocked) { _isInputBlocked = isBlocked; }
    
private:
	KEY_STATE Get_State(KEY_TYPE eKey) const;
	static bool Is_WindowOwnedByCurrentProcess(HWND hWnd);
	RECT Get_ActiveMouseLockRectScreen() const;
	POINT Get_ActiveMouseLockCenterScreen() const;

private:
	HWND _windowHandle = {};					
	vector<KEY_STATE> _keyStates = {};			
	POINT _mousePosition = {};
	POINT _mouseRestorePosition = {};
	POINT _screenCenter = {};					
	Vec2 _mouseDelta = Vec2::Zero;				
	float _mouseWheelDelta = 0.f;				
	bool _isMouseLocked = false;
	bool _isInputBlocked = false;
	bool _isEditorMouseLockSessionActive = false;
	bool _hasMouseLockOverrideRect = false;
	RECT _mouseLockOverrideRect = {};

public:
	static Unique<Input_Manager> Create(HWND hWnd);
	virtual void Free() override;
};

NS_END

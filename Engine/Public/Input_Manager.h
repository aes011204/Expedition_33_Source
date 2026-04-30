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

class ENGINE_DLL Input_Manager final : public Base
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

	/* ?꾩옱 client 醫뚰몴怨?留덉슦???꾩튂瑜??살쓣 ???몄텧?쒕떎. */
	const POINT& Get_MousePosition() const { return _mousePosition; }

	/* ?댁쟾 ?꾨젅???鍮?留덉슦???대룞?됱쓣 ?살쓣 ???몄텧?쒕떎. */
	Vec2 Get_MouseDelta() const { return _mouseDelta; }

	/* 留덉슦?????낅젰?됱쓣 ?살쓣 ???몄텧?쒕떎. */
	float Get_MouseWheel() const { return _mouseWheelDelta; }

	/* WndProc?먯꽌 WM_MOUSEWHEEL??諛쏆븯??????媛믪쓣 ?꾨떖?????몄텧?쒕떎. */
	void Set_MouseWheel(float delta) { _mouseWheelDelta = delta; }

	/* 寃뚯엫 移대찓??議곗옉泥섎읆 而ㅼ꽌瑜?李?以묒븰??怨좎젙?댁빞 ?????몄텧?쒕떎. */
	void Lock_Mouse();

	/* 留덉슦??怨좎젙???댁젣?섍퀬 OS 而ㅼ꽌瑜??ㅼ떆 蹂댁뿬以????몄텧?쒕떎. */
	void Unlock_Mouse();

	/* ?꾩옱 留덉슦?ㅺ? 李쎌뿉 怨좎젙?섏뼱 ?덈뒗吏 ?뺤씤?????몄텧?쒕떎. */
	bool Is_MouseLocked() const { return _isMouseLocked; }

	/* ImGui/?먮뵒??UI媛 ?낅젰???뚮퉬?섎뒗 ?숈븞 寃뚯엫 ?낅젰??留됱쓣 ???몄텧?쒕떎. */
	void Set_InputBlocked(bool isBlocked) { _isInputBlocked = isBlocked; }

	/* ?꾩옱 寃뚯엫 ?낅젰??李⑤떒?섏뼱 ?덈뒗吏 ?뺤씤?????몄텧?쒕떎. */
	bool Is_InputBlocked() const { return _isInputBlocked; }

private:
	/* KEY_TYPE enum???몃뜳?ㅻ줈 諛붽퓭 ?꾩옱 ???곹깭瑜??쎌쓣 ???몄텧?쒕떎. */
	KEY_STATE Get_State(KEY_TYPE key) const;

private:
	HWND _windowHandle = {};								/* ?낅젰??諛쏆쓣 Win32 window handle */
	vector<KEY_STATE> _keyStates = {};				/* 0~255 virtual key ?곹깭 ???踰꾪띁 */
	POINT _mousePosition = {};
	POINT _screenCenter = {};						/* 留덉슦??lock ??而ㅼ꽌瑜??섎룎由?client 以묒븰 醫뚰몴 */
	Vec2 _mouseDelta = Vec2::Zero;				/* ?댁쟾 ?꾨젅???鍮?留덉슦???대룞??*/
	float _mouseWheelDelta = 0.f;				/* ?대쾲 ?꾨젅??留덉슦?????대룞??*/
	bool _isMouseLocked = false;
	bool _isInputBlocked = false;

public:
	/* ?붿쭊 留ㅻ땲? ?앹꽦 ?⑦꽩??留욎떠 unique_ptr 湲곕컲 ?낅젰 留ㅻ땲?瑜?留뚮뱾 ???몄텧?쒕떎. */
	static Unique<Input_Manager> Create(HWND hWnd);

	/* GameInstance媛 ?낅젰 留ㅻ땲?瑜??뺣━????留덉슦??clip ?곹깭瑜??댁젣?쒕떎. */
	virtual void Free() override;
};

NS_END

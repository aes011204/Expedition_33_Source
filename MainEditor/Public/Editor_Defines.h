#pragma once

#include <Windows.h>

#include "Engine_Define.h"

NS_BEGIN(MainEditor)

typedef struct tagEditorDesc
{
	HWND hWnd = nullptr;                    /* 메인 에디터 호스트 윈도우 핸들. */
	WinMode eWinMode = WinMode::WIN;        /* 에디터 렌더 타겟에 사용할 창 모드. */
	uint32 iViewportWidth = {};             /* 기본 에디터 뷰포트 너비. */
	uint32 iViewportHeight = {};            /* 기본 에디터 뷰포트 높이. */
} EDITOR_DESC;

enum class EDITOR_RUNTIME_STATE
{
	EDIT,                                   /* 기본 편집 상태. */
	PLAY,                                   /* 런타임 시뮬레이션 실행 상태. */
	PAUSE,                                  /* 런타임 시뮬레이션 일시정지 상태. */
};

NS_END

using namespace MainEditor;

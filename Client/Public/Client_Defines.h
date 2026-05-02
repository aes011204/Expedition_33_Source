#pragma once

#include <Windows.h>
#include <process.h>

#include "Engine_Define.h"

#include "Client_Macro.h"
#include "Client_Enum.h"
#include "Client_Struct.h"

extern HWND g_hWnd;
extern HINSTANCE g_hInst;

namespace Client 
{
    const unsigned int		g_iWinSizeX = { 1600 };
    const unsigned int		g_iWinSizeY = { 900 };

    enum class LevelType { Static, Loading, Logo, GamePlay, END };
}

using namespace Client;

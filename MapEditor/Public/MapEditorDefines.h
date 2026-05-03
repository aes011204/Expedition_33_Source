#pragma once
#include "Engine_Define.h"
#include "Helper_String.h"

#ifdef new
#pragma push_macro("new")
#undef new
#define MAPEDITOR_RESTORE_NEW
#endif

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "imgui_internal.h"
#include "implot.h"
#include "ImGuizmo.h"

#ifdef MAPEDITOR_RESTORE_NEW
#pragma pop_macro("new")
#undef MAPEDITOR_RESTORE_NEW
#endif

namespace MapEditor
{
    inline constexpr uint32 g_iWinSizeX = 1600u;
    inline constexpr uint32 g_iWinSizeY = 900u;

    inline const char* g_MapSavePath = "../../../Client/Bin/Resources/Map/SaveJson/";
    inline const char* g_MapOriginPath = "../../../Client/Bin/Resources/Map/OriginJson/";
};

using namespace MapEditor;

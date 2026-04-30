#pragma once

#ifdef new
#pragma push_macro("new")
#undef new
#define MAINEDITOR_RESTORE_NEW_MACRO
#endif

#include "imgui.h"
#include "ImGuizmo.h"

#ifdef MAINEDITOR_RESTORE_NEW_MACRO
#pragma pop_macro("new")
#undef MAINEDITOR_RESTORE_NEW_MACRO
#endif

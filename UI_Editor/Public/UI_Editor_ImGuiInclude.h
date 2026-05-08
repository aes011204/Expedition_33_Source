#pragma once

#ifdef new
#pragma push_macro("new")
#undef new
#define UIEDITOR_RESTORE_NEW_MACRO
#endif

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "imgui_internal.h"
#include "implot.h"
#include <imgui-node-editor/imgui_node_editor.h>
#include <ImGuizmo.h>

#ifdef UIEDITOR_RESTORE_NEW_MACRO
#pragma pop_macro("new")
#undef UIEDITOR_RESTORE_NEW_MACRO
#endif

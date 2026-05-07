#pragma once

#include <Windows.h>

#include "Engine_Define.h"

#include "EffectEditor_Enum.h"
#include "EffectEditor_Struct.h"
#include "EffectEditor_Macro.h"
#include "EffectEditor_ImGuiInclude.h"

NS_BEGIN(EffectEditor)

struct EffectEditorDesc
{
    HWND hWnd{};
    WinMode winMode{ WinMode::Win };
    uint32 viewportWidth{};
    uint32 viewportHeight{};
};

enum class EffectEditorRuntimeState : uint8
{
    Edit,
    Play,
    Pause,
};

enum class EditorViewportTarget : uint8
{
    Scene,
    Game,
};

enum class PreviewViewportMode : uint8
{
    Auto,
    Manual,
};

NS_END

using namespace EffectEditor;

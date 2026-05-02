#pragma once

#include "Editor_Enum.h"
#include "Editor_Macro.h"
#include "Editor_Struct.h"
#include "Editor_ImGuiInclude.h"

NS_BEGIN(EffectEditor)

struct EffectEditorDesc
{
    // 메인 에디터 호스트 윈도우 핸들.
    HWND hWnd{};
    // 에디터 렌더 타겟에 사용할 창 모드.
    WinMode eWinMode{ WinMode::Win };
    // 기본 에디터 뷰포트 너비.
    uint32 iViewportWidth{};
    // 기본 에디터 뷰포트 높이.
    uint32 iViewportHeight{};
};

enum class EffectEditorRuntimeState : uint8
{
    Edit,  // 기본 편집 상태.
    Play,  // 런타임 시뮬레이션 실행 상태.
    Pause, // 런타임 시뮬레이션 일시정지 상태.
};

enum class EditorViewportTarget : uint8
{
    Scene,
    Game,
};

NS_END

using namespace EffectEditor;

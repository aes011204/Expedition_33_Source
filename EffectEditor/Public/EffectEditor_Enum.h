#pragma once

namespace EffectEditor
{
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

enum class NotifyType : uint8
{
    Info,
    Success,
    Warning,
    Error,
};
}

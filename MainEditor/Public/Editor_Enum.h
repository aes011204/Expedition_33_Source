#pragma once

namespace MainEditor
{
    enum class NotifyType { Info, Success, Warning, Error };

    enum class PreviewViewportMode : uint8
    {
        Auto,
        Manual,
    };

    enum class PreviewOwner : uint8
    {
        None,
        Prefab,
        Animation,
        AnimState,
    };

}

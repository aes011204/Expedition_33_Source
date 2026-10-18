#pragma once

#include "Engine_Define.h"

NS_BEGIN(UIEditor)

enum class UI_ObjectType
{
    Image,
    Text,
    Panel,
    Button,
    UIVfx,
};

enum class UI_TextAlign
{
    Left,
    Center,
    Right,
};

enum class UI_ImageRenderMode
{
    Normal,
    PatternOverlay,
    MaterialVfx,
    ButtonSheen,
};

enum class UI_MaterialVfxMode
{
    Energy,
    BrushFlow,
};

enum class UI_MaskTextureMode
{
    Alpha,
    BlackWhite,
};

enum class UI_FillOrigin
{
    LeftToRight,
    RightToLeft,
    TopToBottom,
    BottomToTop,
};

enum class UI_AlignmentCommand
{
    Left,
    Right,
    Top,
    Bottom,
    CenterX,
    CenterY,
    DistributeHorizontal,
    DistributeVertical,
};

NS_END

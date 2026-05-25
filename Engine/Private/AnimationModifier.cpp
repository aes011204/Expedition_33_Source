#include "pch.h"
#include "AnimationModifier.h"

HRESULT AnimationModifier::Initialize(AnimationModifierDesc* desc)
{
    _strengthAlpha = desc->strengthAlpha;
    _modifierTag = desc->modifierTag;

    return S_OK;
}

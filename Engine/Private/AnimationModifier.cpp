#include "pch.h"
#include "AnimationModifier.h"

HRESULT AnimationModifier::Initialize(AnimationModifierDesc* desc)
{
    if (desc == nullptr)
        return E_FAIL;

    _strengthAlpha = desc->strengthAlpha;
    _modifierTag = desc->modifierTag;
    _evaluateOnLeaderPose = desc->evaluateOnLeaderPose;

    return S_OK;
}

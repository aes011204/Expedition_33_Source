#include "pch.h"
#include "AnimationModifier.h"

void AnimationModifier::Set_Active(bool isActive)
{
    if (_isActive == isActive)
        return;

    _isActive = isActive;
    On_ActiveChanged(_ownerModel, _isActive);
}

HRESULT AnimationModifier::Initialize(AnimationModifierDesc* desc)
{
    if (desc == nullptr)
        return E_FAIL;

    _strengthAlpha = desc->strengthAlpha;
    _modifierTag = desc->modifierTag;
    _evaluateOnLeaderPose = desc->evaluateOnLeaderPose;
    _blendMode = desc->blendMode;

    return S_OK;
}

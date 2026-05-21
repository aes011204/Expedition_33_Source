#pragma once

#include "EffectAuthoring_Types.h"

NS_BEGIN(EffectEditor)

inline const char* Resolve_RendererType(const AuthoringTypeData& typeData)
{
    return typeData.kind == AuthoringTypeDataKind::Trail ? "Trail" : "Sprite";
}

NS_END

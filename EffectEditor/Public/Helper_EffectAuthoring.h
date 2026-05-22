#pragma once

#include "EffectAuthoring_Types.h"

namespace EffectEditor::Authoring
{
inline const char* Resolve_RendererType(const AuthoringTypeData& typeData)
{
    return typeData.kind == AuthoringTypeDataKind::Trail ? "Trail" : "Sprite";
}
}

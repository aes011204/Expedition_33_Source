#pragma once

#include "EffectAuthoring_Types.h"
#include "EffectRuntime_Types.h"

namespace EffectEditor::PreviewDefinition
{
Shared<const EffectDefinition> Build_EffectDefinition(const vector<AuthoringEmitter>& emitters, bool includeTrail);
Shared<const EffectDefinition> Build_TrailEffectDefinition(const vector<AuthoringEmitter>& emitters);
}

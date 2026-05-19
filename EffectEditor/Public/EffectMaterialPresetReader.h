#pragma once

#include "EffectMaterialAuthoring_Types.h"

namespace EffectEditor::EffectMaterialPresetReader
{
// 실패하면 outMaterial을 기본값 상태로 되돌리고 false를 반환한다.
bool Read(const fs::path& filePath, EffectMaterialInstanceData& outMaterial);

string ResolveDisplayName(const fs::path& filePath);
}

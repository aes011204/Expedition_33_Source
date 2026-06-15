#pragma once
#include "EffectAuthoring_Types.h"

NS_BEGIN(EffectEditor)

struct AuthoringModuleMetadata
{
    string key{};
    wstring displayName{};
    wstring summary{};
    bool movable{ true };
    bool removable{ true };
    bool supportsSprite{ true };
    bool supportsTrail{ false };
    bool supportsBeam{ false };
};

// AuthoringModuleType에서 파생되는 editor-local 표시/정책 metadata를 제공한다.
const AuthoringModuleMetadata& Get_AuthoringModuleMetadata(AuthoringModuleType type);
// 저장 데이터 복원 이후 editor-local metadata를 module에 다시 보강한다.
void Apply_AuthoringModuleMetadata(AuthoringModule& module);

NS_END

#pragma once
#include "EffectAuthoring_Types.h"

NS_BEGIN(EffectEditor)

class EffectAuthoringJsonSerializer final
{
public:
    // EffectEditor authoring document를 .effect.json schema로 변환한다.
    static json To_Json(const EffectAuthoringDocument& document);
    // .effect.json schema를 EffectEditor authoring document로 복원한다.
    static bool From_Json(const json& root, EffectAuthoringDocument& outDocument);
    // 복원된 문서에서 다음 editor-local authoring id를 계산한다.
    static uint32 Find_NextAuthoringId(const EffectAuthoringDocument& document);

private: //## Static Values
    static constexpr int kEffectAuthoringJsonVersion{ 2 };

private: //## Helper::AuthoringAggregate
    static json To_Json(const AuthoringModule& module);
    static json To_Json(const AuthoringEmitter& emitter);
    static bool From_Json(const json& root, AuthoringModule& outModule);
    static bool From_Json(const json& root, AuthoringEmitter& outEmitter);
    static uint32 Find_NextAuthoringId(const vector<AuthoringEmitter>& emitters);
    static AuthoringModuleData Make_DefaultModuleDataForRestore(AuthoringModuleType type);
};

NS_END

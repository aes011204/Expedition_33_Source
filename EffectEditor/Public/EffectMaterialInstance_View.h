#pragma once

#include "Editor_Window.h"
#include "EffectAuthoring_Types.h"

NS_BEGIN(EffectEditor)

class Emitter_View;
class EffectMaterial_View;

class EffectMaterialInstance_View final : public Editor_Window
{
public:
    EffectMaterialInstance_View();
    ~EffectMaterialInstance_View() override = default;

public:
    void Render() override;

public: //## Behavior::Selection
    void Open_Instance(uint32 emitterId, uint32 moduleId);

private: //## Data
    uint32 _targetEmitterId{ 0 };
    uint32 _targetModuleId{ 0 };

private: //## Helpers
    bool Resolve_Target(Shared<Emitter_View>& outEmitterView, AuthoringEmitter*& outEmitter, RequiredModuleData*& outRequiredData) const;
    bool Draw_InstanceProperties(EffectMaterialInstanceData& material);
    void Apply_MaterialChanged(const Shared<Emitter_View>& emitterView, AuthoringEmitter& emitter, const RequiredModuleData& requiredData);
    void Open_DefinitionWindow(const string& definitionName);

public:
    static Shared<EffectMaterialInstance_View> Create();
};

NS_END

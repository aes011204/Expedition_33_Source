#pragma once

#include "Editor_Window.h"
#include "EffectAuthoring_Types.h"

NS_BEGIN(EffectEditor)

class Detail_View final : public Editor_Window
{
public:
    // 선택된 오브젝트와 effect authoring 속성을 보여줄 인스펙터 창을 생성한다.
    Detail_View();
    ~Detail_View() override;

public:
    void Update(float timeDelta) override;
    void Render() override;

private: //## Helper::EffectAuthoring
    void Render_EffectAuthoringDetail();
    void Render_EmitterDetail(AuthoringEmitter& emitter);
    void Render_ModuleDetail(AuthoringEmitter& emitter, AuthoringModule& module);
    void Render_RequiredDetail(AuthoringEmitter& emitter, AuthoringModule& module, RequiredModuleData& data);
    void Render_SpawnDetail(AuthoringEmitter& emitter, AuthoringModule& module, SpawnModuleData& data);
    void Render_LifetimeDetail(AuthoringEmitter& emitter, AuthoringModule& module, LifetimeModuleData& data);
    void Render_InitialSizeDetail(AuthoringEmitter& emitter, AuthoringModule& module, InitialSizeModuleData& data);
    void Render_InitialVelocityDetail(AuthoringEmitter& emitter, AuthoringModule& module, InitialVelocityModuleData& data);
    void Render_ColorOverLifeDetail(AuthoringEmitter& emitter, AuthoringModule& module, ColorOverLifeModuleData& data);

    bool Draw_StringProperty(const char* label, string& value);
    bool Draw_BoolProperty(const char* label, bool& value);
    bool Draw_UintProperty(const char* label, uint32& value);
    bool Draw_Vec2Property(const char* label, Vec2& value, float speed = 0.01f);
    bool Draw_Vec3Property(const char* label, Vec3& value, float speed = 0.01f);
    bool Draw_ColorProperty(const char* label, float value[4]);
    void Mark_Changed(AuthoringEmitter& emitter, bool changed);

public:
    static Shared<Detail_View> Create();
    void Free() override;
};

NS_END

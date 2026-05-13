#pragma once

#include "EffectAuthoring_Types.h"
#include "Helper_DetailProperty.h"

NS_BEGIN(EffectEditor)

struct ModuleDetailDrawContext
{
    bool Begin_PropertyTable(const char* id);
    void End_PropertyTable();
    bool Draw_StringProperty(const char* label, string& value, const string& defaultValue);
    bool Draw_BoolProperty(const char* label, bool& value, bool defaultValue);
    bool Draw_FloatProperty(const char* label, float& value, float defaultValue, float speed = 0.01f);
    bool Draw_ColorProperty(const char* label, Color& value, const Color& defaultValue);
    bool Draw_FloatDistributionGroup(const char* label, FloatDistributionData& value, const FloatDistributionData& defaultValue, float speed = 0.01f);
    bool Draw_Vector2DistributionGroup(
        const char* label,
        Vector2DistributionData& value,
        const Vector2DistributionData& defaultValue,
        float speed = 0.01f);
    bool Draw_Vector3DistributionGroup(
        const char* label,
        Vector3DistributionData& value,
        const Vector3DistributionData& defaultValue,
        float speed = 0.01f);
    bool Draw_ColorDistributionGroup(
        const char* label,
        ColorDistributionData& value,
        const ColorDistributionData& defaultValue);
    bool Draw_BurstListProperty(
        vector<SpawnModuleData::ParticleBurstData>& burstList,
        const vector<SpawnModuleData::ParticleBurstData>& defaultValue);
    bool Draw_ScreenAlignmentProperty(const char* label, EmitterScreenAlignment& value, EmitterScreenAlignment defaultValue);
    bool Draw_SortModeProperty(const char* label, EmitterSortMode& value, EmitterSortMode defaultValue);
    bool Draw_UintProperty(const char* label, uint32& value, uint32 defaultValue);
    bool Draw_Vec3Property(const char* label, Vec3& value, const Vec3& defaultValue, float speed = 0.01f);
};

class RequiredModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        const AuthoringModule& module,
        RequiredModuleData& data,
        ModuleDetailDrawContext& drawContext);
};

class SpawnModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        SpawnModuleData& data,
        ModuleDetailDrawContext& drawContext);
};

class LifetimeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        LifetimeModuleData& data,
        ModuleDetailDrawContext& drawContext);
};

class InitialSizeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        InitialSizeModuleData& data,
        ModuleDetailDrawContext& drawContext);
};

class InitialVelocityModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        InitialVelocityModuleData& data,
        ModuleDetailDrawContext& drawContext);
};

class InitialColorModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        InitialColorModuleData& data,
        ModuleDetailDrawContext& drawContext);
};

class ColorOverLifeModuleDetail
{
public:
    bool Draw(
        AuthoringEmitter& emitter,
        AuthoringModule& module,
        ColorOverLifeModuleData& data,
        ModuleDetailDrawContext& drawContext);
};

NS_END

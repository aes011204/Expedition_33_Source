#include "ModuleDetail_Drawers.h"

NS_BEGIN(EffectEditor)

bool ModuleDetailDrawContext::Begin_PropertyTable(const char* id)
{
    return DetailProperty::Begin_PropertyTable(id);
}

void ModuleDetailDrawContext::End_PropertyTable()
{
    DetailProperty::End_PropertyTable();
}

bool ModuleDetailDrawContext::Draw_StringProperty(const char* label, string& value, const string& defaultValue)
{
    return DetailProperty::Draw_StringProperty(label, value, defaultValue);
}

bool ModuleDetailDrawContext::Draw_BoolProperty(const char* label, bool& value, bool defaultValue)
{
    return DetailProperty::Draw_BoolProperty(label, value, defaultValue);
}

bool ModuleDetailDrawContext::Draw_FloatProperty(const char* label, float& value, float defaultValue, float speed)
{
    return DetailProperty::Draw_FloatProperty(label, value, defaultValue, speed);
}

bool ModuleDetailDrawContext::Draw_ColorProperty(const char* label, Color& value, const Color& defaultValue)
{
    return DetailProperty::Draw_ColorProperty(label, value, defaultValue);
}

bool ModuleDetailDrawContext::Draw_FloatDistributionGroup(
    const char* label,
    FloatDistributionData& value,
    const FloatDistributionData& defaultValue,
    float speed)
{
    return DetailProperty::Draw_FloatDistributionGroup(label, value, defaultValue, speed);
}

bool ModuleDetailDrawContext::Draw_Vector2DistributionGroup(
    const char* label,
    Vector2DistributionData& value,
    const Vector2DistributionData& defaultValue,
    float speed)
{
    return DetailProperty::Draw_Vector2DistributionGroup(label, value, defaultValue, speed);
}

bool ModuleDetailDrawContext::Draw_Vector3DistributionGroup(
    const char* label,
    Vector3DistributionData& value,
    const Vector3DistributionData& defaultValue,
    float speed)
{
    return DetailProperty::Draw_Vector3DistributionGroup(label, value, defaultValue, speed);
}

bool ModuleDetailDrawContext::Draw_ColorDistributionGroup(
    const char* label,
    ColorDistributionData& value,
    const ColorDistributionData& defaultValue)
{
    return DetailProperty::Draw_ColorDistributionGroup(label, value, defaultValue);
}

bool ModuleDetailDrawContext::Draw_BurstListProperty(
    vector<SpawnModuleData::ParticleBurstData>& burstList,
    const vector<SpawnModuleData::ParticleBurstData>& defaultValue)
{
    return DetailProperty::Draw_BurstListProperty(burstList, defaultValue);
}

bool ModuleDetailDrawContext::Draw_ScreenAlignmentProperty(const char* label, EmitterScreenAlignment& value, EmitterScreenAlignment defaultValue)
{
    return DetailProperty::Draw_ScreenAlignmentProperty(label, value, defaultValue);
}

bool ModuleDetailDrawContext::Draw_SortModeProperty(const char* label, EmitterSortMode& value, EmitterSortMode defaultValue)
{
    return DetailProperty::Draw_SortModeProperty(label, value, defaultValue);
}

bool ModuleDetailDrawContext::Draw_UintProperty(const char* label, uint32& value, uint32 defaultValue)
{
    return DetailProperty::Draw_UintProperty(label, value, defaultValue);
}

bool ModuleDetailDrawContext::Draw_Vec3Property(const char* label, Vec3& value, const Vec3& defaultValue, float speed)
{
    return DetailProperty::Draw_Vec3Property(label, value, defaultValue, speed);
}

bool RequiredModuleDetail::Draw(AuthoringEmitter& emitter, AuthoringModule&, RequiredModuleData& data, ModuleDetailDrawContext& drawContext)
{
    const RequiredModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("머티리얼", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (drawContext.Begin_PropertyTable("RequiredMaterial"))
        {
            changed |= drawContext.Draw_StringProperty("머티리얼", data.materialName, defaultData.materialName);
            changed |= drawContext.Draw_StringProperty("텍스처 슬롯 (임시)", data.textureId, defaultData.textureId);
            changed |= drawContext.Draw_StringProperty("블렌드 모드", data.blendMode, defaultData.blendMode);
            changed |= drawContext.Draw_StringProperty("투명도 소스", data.opacitySource, defaultData.opacitySource);
            changed |= drawContext.Draw_ColorProperty("틴트", data.tint, defaultData.tint);
            changed |= drawContext.Draw_FloatProperty("강도", data.intensity, defaultData.intensity);
            changed |= drawContext.Draw_FloatProperty("투명도 파워", data.opacityPower, defaultData.opacityPower);
            changed |= drawContext.Draw_StringProperty("노이즈 텍스처 슬롯", data.noiseTextureId, defaultData.noiseTextureId);
            changed |= drawContext.Draw_FloatProperty("노이즈 강도", data.noiseStrength, defaultData.noiseStrength);
            changed |= drawContext.Draw_UintProperty("SubUV 행", data.subUvRows, defaultData.subUvRows);
            changed |= drawContext.Draw_UintProperty("SubUV 열", data.subUvCols, defaultData.subUvCols);
            drawContext.End_PropertyTable();
        }
    }

    if (ImGui::CollapsingHeader("이미터 좌표", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (drawContext.Begin_PropertyTable("RequiredTransform"))
        {
            changed |= drawContext.Draw_Vec3Property("원점", data.emitterOrigin, defaultData.emitterOrigin);
            changed |= drawContext.Draw_Vec3Property("회전 (도)", data.emitterRotationDegrees, defaultData.emitterRotationDegrees);
            drawContext.End_PropertyTable();
        }
    }

    if (ImGui::CollapsingHeader("공간 / 정렬", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (drawContext.Begin_PropertyTable("RequiredSpaceSort"))
        {
            changed |= drawContext.Draw_ScreenAlignmentProperty("화면 정렬", data.screenAlignment, defaultData.screenAlignment);
            changed |= drawContext.Draw_FloatProperty("카메라 블렌드 거리 최소", data.minCameraBlendDistance, defaultData.minCameraBlendDistance);
            changed |= drawContext.Draw_FloatProperty("카메라 블렌드 거리 최대", data.maxCameraBlendDistance, defaultData.maxCameraBlendDistance);
            changed |= drawContext.Draw_BoolProperty("로컬 스페이스 사용", data.useLocalSpace, defaultData.useLocalSpace);
            changed |= drawContext.Draw_SortModeProperty("정렬 모드", data.sortMode, defaultData.sortMode);
            drawContext.End_PropertyTable();
        }
    }

    if (ImGui::CollapsingHeader("시간", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (drawContext.Begin_PropertyTable("RequiredTime"))
        {
            changed |= drawContext.Draw_UintProperty("루프 횟수", data.loopCount, defaultData.loopCount);
            changed |= drawContext.Draw_FloatProperty("지속 시간", data.duration, defaultData.duration);
            changed |= drawContext.Draw_FloatProperty("지속 시간 최소값", data.durationLow, defaultData.durationLow);
            changed |= drawContext.Draw_BoolProperty("지속 시간 범위 사용", data.useDurationRange, defaultData.useDurationRange);
            changed |= drawContext.Draw_BoolProperty("루프마다 기간 재계산", data.recalculateDurationEachLoop, defaultData.recalculateDurationEachLoop);
            changed |= drawContext.Draw_FloatProperty("딜레이", data.delay, defaultData.delay);
            changed |= drawContext.Draw_FloatProperty("딜레이 최소값", data.delayLow, defaultData.delayLow);
            changed |= drawContext.Draw_BoolProperty("딜레이 범위 사용", data.useDelayRange, defaultData.useDelayRange);
            changed |= drawContext.Draw_BoolProperty("첫 루프만 딜레이", data.delayFirstLoopOnly, defaultData.delayFirstLoopOnly);
            drawContext.End_PropertyTable();
        }
    }

    if (ImGui::CollapsingHeader("종료 / 킬", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (drawContext.Begin_PropertyTable("RequiredKill"))
        {
            changed |= drawContext.Draw_BoolProperty("비활성화 시 킬", data.killOnDeactivate, defaultData.killOnDeactivate);
            changed |= drawContext.Draw_BoolProperty("완료 시 킬", data.killOnCompleted, defaultData.killOnCompleted);
            drawContext.End_PropertyTable();
        }
    }

    if (ImGui::CollapsingHeader("렌더링 제한", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (drawContext.Begin_PropertyTable("RequiredRenderLimit"))
        {
            changed |= drawContext.Draw_BoolProperty("최대 드로우 수 사용", data.useMaxDrawCount, defaultData.useMaxDrawCount);
            changed |= drawContext.Draw_UintProperty("최대 드로우 수", data.maxDrawCount, defaultData.maxDrawCount);
            drawContext.End_PropertyTable();
        }
    }

    if (changed)
    {
        emitter.rendererType = data.rendererType;
        emitter.textureId = data.textureId;
        emitter.materialName = data.materialName;
        emitter.resourceSummary = data.textureId;
    }

    return changed;
}

bool SpawnModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, SpawnModuleData& data, ModuleDetailDrawContext& drawContext)
{
    const SpawnModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("연속 스폰", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (drawContext.Begin_PropertyTable("SpawnRate"))
        {
            changed |= drawContext.Draw_BoolProperty("연속 스폰 사용", data.processSpawnRate, defaultData.processSpawnRate);
            drawContext.End_PropertyTable();
        }

        changed |= drawContext.Draw_FloatDistributionGroup("초당 생성 수", data.spawnRate, defaultData.spawnRate);
        changed |= drawContext.Draw_FloatDistributionGroup("스폰 속도 스케일", data.spawnRateScale, defaultData.spawnRateScale);
    }

    if (ImGui::CollapsingHeader("버스트", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (drawContext.Begin_PropertyTable("SpawnBurst"))
        {
            changed |= drawContext.Draw_BoolProperty("버스트 사용", data.processBurstList, defaultData.processBurstList);
            drawContext.End_PropertyTable();
        }

        changed |= drawContext.Draw_FloatDistributionGroup("버스트 스케일", data.burstScale, defaultData.burstScale);
        changed |= drawContext.Draw_BurstListProperty(data.burstList, defaultData.burstList);
    }

    if (ImGui::CollapsingHeader("제한", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (drawContext.Begin_PropertyTable("SpawnLimit"))
        {
            changed |= drawContext.Draw_UintProperty("최대 활성 파티클 수", data.maxParticleCount, defaultData.maxParticleCount);
            drawContext.End_PropertyTable();
        }
    }

    return changed;
}

bool LifetimeModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, LifetimeModuleData& data, ModuleDetailDrawContext& drawContext)
{
    const LifetimeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= drawContext.Draw_FloatDistributionGroup("수명", data.lifeTime, defaultData.lifeTime);

    return changed;
}

bool InitialSizeModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, InitialSizeModuleData& data, ModuleDetailDrawContext& drawContext)
{
    const InitialSizeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("크기", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= drawContext.Draw_Vector2DistributionGroup("크기", data.size, defaultData.size);

    return changed;
}

bool InitialVelocityModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, InitialVelocityModuleData& data, ModuleDetailDrawContext& drawContext)
{
    const InitialVelocityModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("속도", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (drawContext.Begin_PropertyTable("InitialVelocity"))
        {
            changed |= drawContext.Draw_Vec3Property("방사 기준점", data.radialPivot, defaultData.radialPivot);
            drawContext.End_PropertyTable();
        }

        changed |= drawContext.Draw_FloatDistributionGroup("속도", data.speed, defaultData.speed);
    }

    return changed;
}

bool InitialColorModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, InitialColorModuleData& data, ModuleDetailDrawContext& drawContext)
{
    const InitialColorModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("초기 컬러", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= drawContext.Draw_ColorDistributionGroup("컬러", data.color, defaultData.color);

    return changed;
}

bool ColorOverLifeModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, ColorOverLifeModuleData& data, ModuleDetailDrawContext& drawContext)
{
    const ColorOverLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명에 따른 색상", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= drawContext.Draw_ColorDistributionGroup("색상", data.colorOverLife, defaultData.colorOverLife);

    return changed;
}

NS_END

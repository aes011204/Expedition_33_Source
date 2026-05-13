#include "ModuleDetail_Drawers.h"

#include "EffectEditorInstance.h"
#include "EffectMaterial_View.h"
#include "EffectMaterialInstance_View.h"

NS_BEGIN(EffectEditor)

namespace Internal
{
    Shared<EffectMaterial_View> Get_MaterialView()
    {
        if (nullptr == EDITOR)
            return nullptr;

        const Shared<Editor_Window> materialWindow = EDITOR->Get_Window(L"Effect Material");
        return dynamic_pointer_cast<EffectMaterial_View>(materialWindow);
    }

    void Open_MaterialInstanceWindow(uint32 emitterId, uint32 moduleId)
    {
        if (nullptr == EDITOR)
            return;

        const Shared<Editor_Window> materialInstanceWindow = EDITOR->Get_Window(L"Effect Material Instance");
        const Shared<EffectMaterialInstance_View> materialInstanceView =
            dynamic_pointer_cast<EffectMaterialInstance_View>(materialInstanceWindow);
        if (nullptr == materialInstanceView)
            return;

        materialInstanceView->Open_Instance(emitterId, moduleId);
    }

    void Open_PresetBrowserWindow(uint32 emitterId, uint32 moduleId)
    {
        const Shared<EffectMaterial_View> materialView = Get_MaterialView();
        if (nullptr == materialView)
            return;

        materialView->Open_PresetBrowser();
    }

    bool Try_GetSelectedPresetMaterial(EffectMaterialInstanceData& outMaterial, string& outFileName)
    {
        const Shared<EffectMaterial_View> materialView = Get_MaterialView();
        if (materialView == nullptr || !materialView->Has_SelectedPreset())
            return false;

        outMaterial = materialView->Get_SelectedPresetMaterial();
        outFileName = materialView->Get_SelectedMaterialFileName();
        return true;
    }
}

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

bool RequiredModuleDetail::Draw(AuthoringEmitter& emitter, const AuthoringModule& module, RequiredModuleData& data, ModuleDetailDrawContext& drawContext)
{
    const RequiredModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("머티리얼", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (drawContext.Begin_PropertyTable("RequiredMaterial"))
        {
            ImGui::PushID("MaterialDefinition");
            DetailProperty::Draw_PropertyLabel("머티리얼");
            ImGui::TextUnformatted(data.material.definitionName.c_str());
            DetailProperty::Draw_ResetButton(false);
            ImGui::PopID();

            ImGui::PushID("MaterialTexture");
            DetailProperty::Draw_PropertyLabel("텍스처");
            ImGui::TextUnformatted(data.material.mainTextureId.c_str());
            DetailProperty::Draw_ResetButton(false);
            ImGui::PopID();

            ImGui::PushID("MaterialSummary");
            DetailProperty::Draw_PropertyLabel("Blend / Opacity");
            ImGui::Text("%s / %s", data.material.blendMode.c_str(), data.material.opacitySource.c_str());
            DetailProperty::Draw_ResetButton(false);
            ImGui::PopID();

            ImGui::PushID("MaterialSubUV");
            DetailProperty::Draw_PropertyLabel("SubUV");
            ImGui::Text("%u x %u", data.material.subUvRows, data.material.subUvCols);
            DetailProperty::Draw_ResetButton(false);
            ImGui::PopID();

            ImGui::PushID("MaterialEdit");
            DetailProperty::Draw_PropertyLabel("작업");
            if (ImGui::Button("인스턴스 편집"))
                Internal::Open_MaterialInstanceWindow(emitter.id, module.id);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("현재 머티리얼 인스턴스 편집");
            ImGui::SameLine();
            if (ImGui::Button("원본 보기"))
                Internal::Open_PresetBrowserWindow(emitter.id, module.id);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("원본 머티리얼을 확인");
            ImGui::SameLine();

            EffectMaterialInstanceData selectedPresetMaterial{};
            string selectedPresetName{};
            const bool hasSelectedPreset =
                Internal::Try_GetSelectedPresetMaterial(selectedPresetMaterial, selectedPresetName);

            ImGui::BeginDisabled(!hasSelectedPreset);
            if (ImGui::Button("선택 원본 장착"))
            {
                data.material = selectedPresetMaterial;
                changed = true;
            }
            ImGui::EndDisabled();

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled))
            {
                if (hasSelectedPreset)
                {
                    ImGui::SetTooltip(
                        "선택된 원본 preset '%s'을 현재 Required embedded instance로 복사합니다. 파일은 저장하지 않습니다.",
                        selectedPresetName.c_str()
                    );
                }
                else
                    ImGui::SetTooltip("Effect Material 창에서 원본 preset을 먼저 선택하세요.");
            }

            DetailProperty::Draw_ResetButton(false);
            ImGui::PopID();

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
        emitter.textureId = data.material.mainTextureId;
        emitter.materialName = data.material.definitionName;
        emitter.resourceSummary = data.material.mainTextureId;
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

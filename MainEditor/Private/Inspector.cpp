#include "Inspector.h"

#include "Component.h"
#include "Component_Inspector.h"
#include "Editor_Context.h"
#include "EditorInstance.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "Helper_String.h"
#include "Inspector_Factory.h"
#include "ModelCom.h"
#include "ClientInstance.h"
#include "GameplayUI.h"
#include "IRuntimeUI_EditorTarget.h"
#include "MapStaticMesh.h"
#include "Monster_WorldBase.h"
#include "Reflection_Types.h"
#include "TransformCom.h"
#include "UI_Base.h"
#include "UI_RuntimePanelBase.h"
#include "UI_Transform.h"

const vector<Engine::BTBlackboardKeyView>* Inspector::_btBlackboardKeys = nullptr;
vector<Inspector::FGameObjectClassPickerEntry> Inspector::_gameObjectClassPickerEntries{};
char Inspector::_spawnActorClassSearchBuffer[256]{};

namespace
{
constexpr float CascadePresetLabelWidth = 110.f;

const char* Get_CascadeLevelLabel(Engine::CascadeLevel level)
{
    switch (level)
    {
    case Engine::CascadeLevel::Close:
        return "Close";
    case Engine::CascadeLevel::Far:
        return "Far";
    case Engine::CascadeLevel::Normal:
    default:
        return "Normal";
    }
}

bool Draw_CascadePresetControls(const char* headerLabel, const char* idPrefix, Engine::ShadowCascadePreset& preset)
{
    bool changed = false;

    if (ImGui::CollapsingHeader(headerLabel, ImGuiTreeNodeFlags_DefaultOpen))
    {
        const string focusNearId = string("##") + idPrefix + "FocusNear";
        const string focusFarId = string("##") + idPrefix + "FocusFar";
        const string lambdaId = string("##") + idPrefix + "SplitLambda";
        const string blendId = string("##") + idPrefix + "BlendDistance";

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Focus Near");
        ImGui::SameLine(CascadePresetLabelWidth);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        changed |= ImGui::DragFloat(focusNearId.c_str(), &preset.focusNear, 0.01f, 0.001f, 10000.f, "%.3f");

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Focus Far");
        ImGui::SameLine(CascadePresetLabelWidth);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        changed |= ImGui::DragFloat(focusFarId.c_str(), &preset.focusFar, 0.1f, 0.001f, 10000.f, "%.3f");

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Split Lambda");
        ImGui::SameLine(CascadePresetLabelWidth);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        changed |= ImGui::DragFloat(lambdaId.c_str(), &preset.cascadeSplitLambda, 0.01f, 0.f, 1.f, "%.3f");

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Blend Distance");
        ImGui::SameLine(CascadePresetLabelWidth);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        changed |= ImGui::DragFloat(blendId.c_str(), &preset.cascadeBlendDistance, 0.1f, 0.f, 10000.f, "%.3f");
    }

    return changed;
}
}

    struct FEffectAssetPickerEntry
    {
        string relativePath{};
        string displayLabel{};
        string searchText{};
    };

    vector<FEffectAssetPickerEntry> g_effectAssetPickerEntries{};
    char g_effectAssetSearchBuffer[256]{};

    fs::path Resolve_ResourceRootForEffectPicker()
    {
        const fs::path currentPath = fs::current_path();
        const vector<fs::path> candidates =
        {
            currentPath / L"../../../Client/Bin/Resources",
            currentPath / L"../../Client/Bin/Resources",
            currentPath / L"Client/Bin/Resources",
        };

        for (const fs::path& candidate : candidates)
        {
            const fs::path normalized = fs::absolute(candidate).lexically_normal();
            if (fs::exists(normalized) && fs::is_directory(normalized))
                return normalized;
        }

        return {};
    }

    bool Is_EffectAssetJsonPath(const fs::path& path)
    {
        const string lowerPath = String::ToLowerCopy(path.generic_string());
        return lowerPath.ends_with(".effect.json");
    }

    void Rebuild_EffectAssetPickerEntries()
    {
        g_effectAssetPickerEntries.clear();

        const fs::path resourceRoot = Resolve_ResourceRootForEffectPicker();
        if (resourceRoot.empty())
            return;

        const fs::path effectRoot = resourceRoot / L"Effects";
        if (!fs::exists(effectRoot) || !fs::is_directory(effectRoot))
            return;

        std::error_code errorCode{};
        for (const fs::directory_entry& entry : fs::recursive_directory_iterator(effectRoot, errorCode))
        {
            if (errorCode)
                break;

            if (!entry.is_regular_file(errorCode))
                continue;

            const fs::path filePath = entry.path();
            if (!Is_EffectAssetJsonPath(filePath))
                continue;

            const fs::path relativePath = fs::relative(filePath, resourceRoot, errorCode);
            if (errorCode)
                continue;

            string storedPath = relativePath.generic_string();
            replace(storedPath.begin(), storedPath.end(), '/', '\\');

            FEffectAssetPickerEntry pickerEntry{};
            pickerEntry.relativePath = storedPath;
            pickerEntry.displayLabel = storedPath;
            pickerEntry.searchText = String::ToLowerCopy(storedPath + " " + filePath.filename().generic_string());
            g_effectAssetPickerEntries.push_back(std::move(pickerEntry));
        }

        sort(
            g_effectAssetPickerEntries.begin(),
            g_effectAssetPickerEntries.end(),
            [](const FEffectAssetPickerEntry& left, const FEffectAssetPickerEntry& right)
            {
                return left.displayLabel < right.displayLabel;
            });
    }

    bool Draw_EffectAssetPickerRow(const string& hiddenId, wstring& value)
    {
        if (g_effectAssetPickerEntries.empty())
            Rebuild_EffectAssetPickerEntries();

        string preview = String::ToString(value);
        if (preview.empty())
            preview = "(select effect)";

        bool changed = false;

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::BeginCombo(hiddenId.c_str(), preview.c_str()))
        {
            ImGui::InputTextWithHint(
                ("##EffectAssetSearch" + hiddenId).c_str(),
                "Search effect...",
                g_effectAssetSearchBuffer,
                IM_ARRAYSIZE(g_effectAssetSearchBuffer));

            ImGui::Separator();

            if (ImGui::Selectable("(None)", value.empty()))
            {
                value.clear();
                changed = true;
            }

            const string searchText = String::ToLowerCopy(g_effectAssetSearchBuffer);
            bool hasVisibleEntry = false;

            for (const FEffectAssetPickerEntry& entry : g_effectAssetPickerEntries)
            {
                if (!searchText.empty() && entry.searchText.find(searchText) == string::npos)
                    continue;

                hasVisibleEntry = true;

                const bool isSelected = (String::ToString(value) == entry.relativePath);
                const string selectableLabel = entry.displayLabel + "##" + entry.relativePath;
                if (ImGui::Selectable(selectableLabel.c_str(), isSelected))
                {
                    value = String::ToWString(entry.relativePath);
                    changed = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            if (!hasVisibleEntry)
                ImGui::TextDisabled("No effect assets.");

            ImGui::EndCombo();
        }

        return changed;
    }

    struct FMonsterBattlePrefabPickerEntry
    {
        string prefabName{};
        string className{};
    };

    vector<FMonsterBattlePrefabPickerEntry> g_monsterBattlePrefabPickerEntries{};

    fs::path Resolve_PrefabRootForMonsterBattlePicker()
    {
        const fs::path currentPath = fs::current_path();
        const vector<fs::path> candidates =
        {
            currentPath / L"../../../Client/Bin/Resources/Data/json/Prefabs",
            currentPath / L"../../Client/Bin/Resources/Data/json/Prefabs",
            currentPath / L"Client/Bin/Resources/Data/json/Prefabs",
        };

        for (const fs::path& candidate : candidates)
        {
            const fs::path normalized = fs::absolute(candidate).lexically_normal();
            if (fs::exists(normalized) && fs::is_directory(normalized))
                return normalized;
        }

        return {};
    }

    void Rebuild_MonsterBattlePrefabPickerEntries()
    {
        g_monsterBattlePrefabPickerEntries.clear();

        const fs::path prefabsRoot = Resolve_PrefabRootForMonsterBattlePicker();
        if (prefabsRoot.empty())
            return;

        static const string kPrefabSuffix = ".prefab.json";

        std::error_code errorCode{};
        for (const fs::directory_entry& entry : fs::recursive_directory_iterator(prefabsRoot, errorCode))
        {
            if (errorCode)
                break;

            if (!entry.is_regular_file(errorCode))
                continue;

            const string fileName = String::ToString(entry.path().filename().wstring());
            if (fileName.size() <= kPrefabSuffix.size() ||
                fileName.compare(fileName.size() - kPrefabSuffix.size(), kPrefabSuffix.size(), kPrefabSuffix) != 0)
            {
                continue;
            }

            const json root = GAME->Load_JsonFile(entry.path());
            if (!root.is_object())
                continue;

            const string staticClass = root.value("static_class", string{});
            if (staticClass.empty())
                continue;

            const auto* typeInfo = GAME->Find_GameObjectType(staticClass);
            if (typeInfo == nullptr || typeInfo->reflectionInfo == nullptr)
                continue;

            if (typeInfo->reflectionInfo->category != "Monster")
                continue;

            const string prefabName = fileName.substr(0, fileName.size() - kPrefabSuffix.size());
            g_monsterBattlePrefabPickerEntries.push_back({ prefabName, staticClass });
        }

        sort(
            g_monsterBattlePrefabPickerEntries.begin(),
            g_monsterBattlePrefabPickerEntries.end(),
            [](const FMonsterBattlePrefabPickerEntry& left, const FMonsterBattlePrefabPickerEntry& right)
            {
                return left.prefabName < right.prefabName;
            });
    }


Inspector::Inspector()
    : Editor_Window{ L"Inspector" }
{
}

void Inspector::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    if (ImGui::Begin("Inspector", &isOpen))
    {
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);

        Shared<GameObject> selectedObject = EDITOR->Selected_Object();
        if (_isPrimaryShadowLightTarget && selectedObject != nullptr)
            _isPrimaryShadowLightTarget = false;

        if (_isPrimaryShadowLightTarget)
        {
            Draw_PrimaryShadowLightInspector();
            Set_Open(isOpen);
            ImGui::End();
            return;
        }

        if (selectedObject != nullptr)
        {
            Draw_Components(selectedObject);
        }
        else if (Shared<UI_Base> selectedUI = EDITOR->Selected_UI(); selectedUI != nullptr)
        {
            Draw_UIInspector(
                selectedUI,
                EDITOR->Selected_UIGroup(),
                EDITOR->Selected_UIDisplayName(),
                EDITOR->Selected_UIPositionDrivenByCode());
        }
        else
        {
            ImGui::TextUnformatted("No Selected");
            ImGui::Separator();
            ImGui::TextDisabled("Inspector");
            Set_Open(isOpen);
            ImGui::End();
            return;
        }
    }

    Set_Open(isOpen);
    ImGui::End();
}

void Inspector::Set_PrimaryShadowLightTarget(bool enabled)
{
    _isPrimaryShadowLightTarget = enabled;

    if (enabled)
    {
        EDITOR->Clear_SelectedObject();
        EDITOR->Clear_SelectedUI();
    }
}

void Inspector::Draw_UIInspector(
    const Shared<UI_Base>& ui,
    const string& groupName,
    const string& displayName,
    bool positionDrivenByCode)
{
    if (ui == nullptr)
    {
        ImGui::TextDisabled("UI selection is no longer valid");
        return;
    }

    UI_Transform* transform = ui->Get_Transform();
    if (transform == nullptr)
    {
        ImGui::TextDisabled("UI Transform 없음");
        return;
    }

    const string resolvedName = displayName.empty() ? "RuntimeUI" : displayName;

    ImGui::Text("이름: %s", resolvedName.c_str());
    ImGui::TextDisabled("그룹: %s", groupName.empty() ? "UI" : groupName.c_str());
    ImGui::Separator();

    if (positionDrivenByCode)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.78f, 0.25f, 1.f));
        ImGui::TextWrapped("현재 Position 값은 런타임 코드에서 계속 갱신되어 다음 프레임에 덮어써질 수 있습니다. 아래 강제 테스트 오프셋으로 위치를 맞춰보세요.");
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    Draw_RuntimeUITestOffsetSection(ui); // Battle UI만 공통 테스트 오프셋 편집기를 노출한다.
    Draw_RuntimeUIOwnerReflectionSection(ui);

    bool isOpen = ui->Is_Open();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("열림");
    ImGui::SameLine(InspectorLabelWidth);
    ImGui::BeginDisabled();
    ImGui::Checkbox("##UIOpenState", &isOpen);
    ImGui::EndDisabled();

    Vec2 position = transform->Get_Position();
    if (Draw_UIVec2Row("Position", "##UIPosition", position, 1.f))
        transform->Set_Position(position);

    Vec2 size = transform->Get_Size();
    if (Draw_UIVec2Row("Size", "##UISize", size, 1.f))
        transform->Set_Size(size);

    Vec2 scale = transform->Get_Scale();
    if (Draw_UIVec2Row("Scale", "##UIScale", scale, 0.01f))
        transform->Set_Scale(scale);

    Vec2 pivot = transform->Get_Pivot();
    if (Draw_UIVec2Row("Pivot", "##UIPivot", pivot, 0.01f))
        transform->Set_Pivot(pivot);

    Vec2 anchorMin = transform->Get_AnchorMin();
    if (Draw_UIVec2Row("Anchor Min", "##UIAnchorMin", anchorMin, 0.01f))
        transform->Set_AnchorMin(anchorMin);

    Vec2 anchorMax = transform->Get_AnchorMax();
    if (Draw_UIVec2Row("Anchor Max", "##UIAnchorMax", anchorMax, 0.01f))
        transform->Set_AnchorMax(anchorMax);

    float rotationRadians = transform->Get_RotationRadians();
    if (Draw_UIFloatRow("Rotation", "##UIRotation", rotationRadians, 0.01f))
        transform->Set_RotationRadians(rotationRadians);
}

void Inspector::Draw_RuntimeUITestOffsetSection(const Shared<UI_Base>& ui)
{
    Shared<GameplayUI> gameplayUI = CLIENT->Get_GameplayUI();
    if (gameplayUI == nullptr)
        return;

    IRuntimeUI_EditorTarget* editorTarget = gameplayUI->Find_RuntimeUIEditorTarget(ui);
    if (editorTarget == nullptr)
        return; // World UI는 여기로 들어오지 않게 Battle 쪽만 연결한다.

    ImGui::Separator();
    ImGui::TextDisabled("강제 테스트 오프셋");

    Vec2 defaultOffset = editorTarget->Get_EditorDefaultTestOffset();
    if (Draw_UIVec2Row("기본 오프셋", "##RuntimeUIDefaultOffset", defaultOffset, 1.f))
        editorTarget->Set_EditorDefaultTestOffset(defaultOffset); // 모든 Battle Runtime UI 공통 기본 오프셋

    if (!editorTarget->Supports_EditorHeroTestOffset())
    {
        ImGui::Spacing();
        return; // Hero 문맥이 없는 Battle UI는 기본 Offset만 조절한다.
    }

    const HeroId activeHeroId = editorTarget->Get_EditorActiveHeroId();
    if (activeHeroId == HeroId::END)
    {
        ImGui::TextDisabled("현재 플레이어 턴이 아니라 영웅별 오프셋을 조절할 수 없습니다.");
        ImGui::Spacing();
        return;
    }

    const char* heroLabel = "Unknown";
    switch (activeHeroId)
    {
    case HeroId::Gustave:
        heroLabel = "Gustave";
        break;

    case HeroId::Lune:
        heroLabel = "Lune";
        break;

    case HeroId::Maelle:
        heroLabel = "Maelle";
        break;

    case HeroId::END:
    default:
        break;
    }

    ImGui::Text("현재 영웅: %s", heroLabel);

    Vec2 heroOffset = editorTarget->Get_EditorHeroTestOffset(activeHeroId);
    if (Draw_UIVec2Row("영웅별 오프셋", "##RuntimeUIHeroOffset", heroOffset, 1.f))
        editorTarget->Set_EditorHeroTestOffset(activeHeroId, heroOffset); // BattleCommandHUD만 Hero별 추가 오프셋을 갖는다.

    ImGui::Spacing();
}

void Inspector::Draw_RuntimeUIOwnerReflectionSection(const Shared<UI_Base>& ui)
{
    Shared<GameplayUI> gameplayUI = CLIENT->Get_GameplayUI();
    if (gameplayUI == nullptr)
        return;

    IRuntimeUI_EditorTarget* editorTarget = gameplayUI->Find_RuntimeUIEditorTarget(ui);
    auto* runtimePanel = dynamic_cast<Client::UI_RuntimePanelBase*>(editorTarget);
    if (runtimePanel == nullptr)
        return;

    ClassReflectionInfo& info = runtimePanel->Get_ReflectionInfo();
    if (info.properties.empty())
        return;

    ImGui::Separator();
    Draw_ReflectionSection(info.className, runtimePanel, info);
    runtimePanel->Apply_EditorReflectionRuntimeTuning();
}

bool Inspector::Draw_UIVec2Row(const char* label, const string& id, Vec2& value, float speed)
{
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", label);
    ImGui::SameLine(InspectorLabelWidth);

    PropertyInfo property{};
    property.dragSpeed = speed;
    return Draw_Vec2PropertyRow(id, property, value);
}

bool Inspector::Draw_UIFloatRow(const char* label, const string& id, float& value, float speed)
{
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", label);
    ImGui::SameLine(InspectorLabelWidth);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    return ImGui::DragFloat(id.c_str(), &value, speed);
}

void Inspector::Draw_PrimaryShadowLightInspector()
{
    const LIGHT_DESC* sourceLight = GAME->Get_PrimaryShadowLightDesc();
    if (sourceLight == nullptr && GAME->Get_LightCount() > 0)
        sourceLight = GAME->Get_LightDesc(0);

    const ShadowLightDesc* sourceShadow = GAME->Get_ShadowLightDesc();
    const ShadowCascadePresetSet* sourceCascadePresets = GAME->Get_ShadowCascadePresets();

    ImGui::TextUnformatted("Name: Primary Shadow Light");
    ImGui::Separator();

    if (sourceLight == nullptr)
    {
        ImGui::TextDisabled("현재 레벨에 Light가 없음");
        return;
    }

    if (sourceShadow == nullptr)
    {
        ImGui::TextDisabled("Shadow 시스템이 초기화되지 않음");
        return;
    }

    if (sourceCascadePresets == nullptr)
    {
        ImGui::TextDisabled("Cascade preset 시스템이 초기화되지 않음");
        return;
    }

    LIGHT_DESC lightDesc = *sourceLight;
    ShadowLightDesc shadowDesc = *sourceShadow;
    ShadowCascadePresetSet cascadePresets = *sourceCascadePresets;
    CascadeLevel activeCascadeLevel = GAME->Get_ShadowCascadeLevel();

    bool lightChanged = false;
    bool shadowChanged = false;
    bool cascadePresetChanged = false;
    bool cascadeLevelChanged = false;

    if (ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Type");
        ImGui::SameLine(InspectorLabelWidth);
        ImGui::TextUnformatted(lightDesc.type == LightType::Directional ? "Directional" : "Non-Directional");

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Cast Shadow");
        ImGui::SameLine(InspectorLabelWidth);
        lightChanged |= ImGui::Checkbox("##CastShadow", &lightDesc.castShadow);

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Shadow Enabled");
        ImGui::SameLine(InspectorLabelWidth);
        shadowChanged |= ImGui::Checkbox("##ShadowEnabled", &shadowDesc.enabled);

        float direction[3] = { lightDesc.direction.x, lightDesc.direction.y, lightDesc.direction.z };
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Direction");
        ImGui::SameLine(InspectorLabelWidth);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::DragFloat3("##LightDirection", direction, 0.01f))
        {
            lightDesc.direction.x = direction[0];
            lightDesc.direction.y = direction[1];
            lightDesc.direction.z = direction[2];
            lightChanged = true;
        }

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Diffuse");
        ImGui::SameLine(InspectorLabelWidth);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        lightChanged |= ImGui::ColorEdit4("##LightDiffuse", &lightDesc.diffuse.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_AlphaBar);

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Ambient");
        ImGui::SameLine(InspectorLabelWidth);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        lightChanged |= ImGui::ColorEdit4("##LightAmbient", &lightDesc.ambient.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_AlphaBar);

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Specular");
        ImGui::SameLine(InspectorLabelWidth);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        lightChanged |= ImGui::ColorEdit4("##LightSpecular", &lightDesc.specular.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_AlphaBar);
    }

    if (ImGui::CollapsingHeader("Shadow", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Eye");
        ImGui::SameLine(InspectorLabelWidth);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        shadowChanged |= ImGui::DragFloat3("##ShadowEye", &shadowDesc.eye.x, 0.1f);

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Target");
        ImGui::SameLine(InspectorLabelWidth);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        shadowChanged |= ImGui::DragFloat3("##ShadowTarget", &shadowDesc.at.x, 0.1f);

        float fovyDegree = DirectX::XMConvertToDegrees(shadowDesc.fovy);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("FOV Y");
        ImGui::SameLine(InspectorLabelWidth);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::DragFloat("##ShadowFovY", &fovyDegree, 0.1f, 1.f, 179.f, "%.2f deg"))
        {
            shadowDesc.fovy = DirectX::XMConvertToRadians(fovyDegree);
            shadowChanged = true;
        }

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Aspect");
        ImGui::SameLine(InspectorLabelWidth);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        shadowChanged |= ImGui::DragFloat("##ShadowAspect", &shadowDesc.aspect, 0.01f, 0.1f, 10.f);

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Near / Far");
        ImGui::SameLine(InspectorLabelWidth);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (shadowDesc.followCameraFrustum)
        {
            ImGui::Text("%.3f / %.3f", shadowDesc.zn, shadowDesc.zf);
        }
        else
        {
            float nearFar[2] = { shadowDesc.zn, shadowDesc.zf };
            if (ImGui::DragFloat2("##ShadowNearFar", nearFar, 0.1f, 0.001f, 10000.f))
            {
                shadowDesc.zn = max(0.001f, nearFar[0]);
                shadowDesc.zf = max(shadowDesc.zn + 0.001f, nearFar[1]);
                shadowChanged = true;
            }
        }

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Bias");
        ImGui::SameLine(InspectorLabelWidth);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        shadowChanged |= ImGui::DragFloat("##ShadowBias", &shadowDesc.bias, 0.00005f, 0.f, 0.01f, "%.6f");

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Strength");
        ImGui::SameLine(InspectorLabelWidth);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        shadowChanged |= ImGui::DragFloat("##ShadowIntensity", &shadowDesc.intensity, 0.01f, 0.f, 1.f);

        if (shadowDesc.followCameraFrustum)
        {
            static const char* cascadeLevelLabels[] = { "Close", "Normal", "Far" };
            int activeCascadeLevelIndex = static_cast<int>(activeCascadeLevel);

            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("Active Level");
            ImGui::SameLine(InspectorLabelWidth);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::Combo("##ShadowCascadeLevel", &activeCascadeLevelIndex, cascadeLevelLabels, IM_ARRAYSIZE(cascadeLevelLabels)))
            {
                activeCascadeLevel = static_cast<CascadeLevel>(activeCascadeLevelIndex);
                cascadeLevelChanged = true;
            }

            ImGui::TextDisabled("Current: %s", Get_CascadeLevelLabel(activeCascadeLevel));
            ImGui::Separator();
            ImGui::TextDisabled("Cascade Presets");

            cascadePresetChanged |= Draw_CascadePresetControls("Close", "ShadowCascadeClose", cascadePresets.close);
            cascadePresetChanged |= Draw_CascadePresetControls("Normal", "ShadowCascadeNormal", cascadePresets.normal);
            cascadePresetChanged |= Draw_CascadePresetControls("Far", "ShadowCascadeFar", cascadePresets.farRange);
        }
    }

    if (lightChanged)
        GAME->Update_PrimaryShadowLightDesc(lightDesc);

    if (shadowChanged)
        GAME->Update_ShadowLight(shadowDesc);

    if (cascadePresetChanged)
        GAME->Set_ShadowCascadePresets(cascadePresets);

    if (cascadeLevelChanged)
        GAME->Set_ShadowCascadeLevel(activeCascadeLevel);
}

bool Inspector::Draw_ComponentOverride(const Shared<Component>& component)
{
    if (component == nullptr)
        return false;

    const ClassReflectionInfo& info = component->Get_ReflectionInfo();

    string inspectorKey = info.className;
    if (dynamic_pointer_cast<ModelCom>(component))
        inspectorKey = "ModelCom";

    if (inspectorKey.empty())
        inspectorKey = String::ToString(component->Get_Name());

    Shared<Component_Inspector> inspector = EDITOR->Find_Inspector(inspectorKey);
    if (inspector == nullptr)
        return false;

    inspector->Draw_Inspector(component);
    return true;
}

void Inspector::Draw_Components(const Shared<GameObject>& target)
{
    CHECK_NULL(target);

    Shared<TransformCom> transform = target->Get_Transform();
    if (transform == nullptr)
    {
        ImGui::TextDisabled("Transform Component 없음");
        return;
    }

    Draw_ComponentOverride(static_pointer_cast<Component>(transform));
    Draw_MonsterBattlePrefabList(target);
    Draw_ObjectReflection(target);

    target->Visit_Components([&](const wstring&, const Shared<Component>& component)
        {
            if (component == nullptr)
                return;

            // 트랜스폼은 무조건 맨위에 그릴거라 여기서 스킵
            if (component == transform)
                return;

            if (Draw_ComponentOverride(component))
                return;

            const ClassReflectionInfo& info = component->Get_ReflectionInfo();
            if (info.properties.empty())
                return;

        Draw_ReflectionSection(info.className, component.get(), info);
    });

    if (const Shared<Client::MapStaticMesh> mapStaticMesh = dynamic_pointer_cast<Client::MapStaticMesh>(target))
    {
        if (const Shared<ModelCom> mapModel = mapStaticMesh->Get_Model())
            Draw_ComponentOverride(static_pointer_cast<Component>(mapModel));
    }
}

bool Inspector::Draw_MonsterBattlePrefabList(const Shared<GameObject>& target)
{
    Shared<Client::Monster_WorldBase> monster = dynamic_pointer_cast<Client::Monster_WorldBase>(target);
    if (monster == nullptr)
        return false;

    if (g_monsterBattlePrefabPickerEntries.empty())
        Rebuild_MonsterBattlePrefabPickerEntries();

    vector<Client::BattleMonsterSpawnDesc>& spawnDescs = monster->Get_BattleMonsterSpawnDescs_ForEdit();

    // 프리팹/맵 인스턴스가 실제 전투 진입 때 넘기는 소환 목록이다.
    if (!ImGui::CollapsingHeader("Battle Monster Prefabs", ImGuiTreeNodeFlags_DefaultOpen))
        return false;

    bool changed = false;

    ImGui::PushID("InspectorBattleMonsterPrefabs");

    constexpr float BattlePrefabLabelWidth = 110.f;

    int32 removeIndex = -1;
    for (int32 i = 0; i < static_cast<int32>(spawnDescs.size()); ++i)
    {
        ImGui::PushID(i);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Slot %d", i);
        ImGui::SameLine(BattlePrefabLabelWidth);

        ImGui::SetNextItemWidth(max(80.f, ImGui::GetContentRegionAvail().x - 74.f));
        const char* previewLabel =
            spawnDescs[i].prefabName.empty() ? "(select prefab)" : spawnDescs[i].prefabName.c_str();
        if (ImGui::BeginCombo("##BattleMonsterPrefabName", previewLabel))
        {
            bool hasCurrentPrefab = spawnDescs[i].prefabName.empty();
            for (const FMonsterBattlePrefabPickerEntry& entry : g_monsterBattlePrefabPickerEntries)
            {
                const bool isSelected = (spawnDescs[i].prefabName == entry.prefabName);
                if (isSelected)
                    hasCurrentPrefab = true;

                const string label = entry.prefabName + "##" + entry.className;
                if (ImGui::Selectable(label.c_str(), isSelected))
                {
                    if (spawnDescs[i].prefabName != entry.prefabName)
                    {
                        spawnDescs[i].prefabName = entry.prefabName; // 저장값은 런타임 로드에 쓰는 prefab 이름이다.
                        changed = true;
                    }
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            if (!hasCurrentPrefab && !spawnDescs[i].prefabName.empty())
            {
                ImGui::Separator();
                ImGui::TextDisabled("Missing: %s", spawnDescs[i].prefabName.c_str());
            }

            ImGui::EndCombo();
        }

        ImGui::SameLine();
        if (ImGui::Button("Remove", ImVec2(68.f, 0.f)))
            removeIndex = i;

        Vec3 offset = spawnDescs[i].offset;
        ImGui::Text("Offset");
        ImGui::SameLine(BattlePrefabLabelWidth);
        ImGui::SetNextItemWidth(max(80.f, ImGui::GetContentRegionAvail().x));
        if (ImGui::DragFloat3("##BattleMonsterOffset", &offset.x, 0.05f))
        {
            spawnDescs[i].offset = offset;
            changed = true;
        }

        Vec3 rotation = spawnDescs[i].rotation;
        ImGui::Text("Rotation");
        ImGui::SameLine(BattlePrefabLabelWidth);
        ImGui::SetNextItemWidth(max(80.f, ImGui::GetContentRegionAvail().x));
        if (ImGui::DragFloat3("##BattleMonsterRotation", &rotation.x, 0.25f))
        {
            spawnDescs[i].rotation = rotation;
            changed = true;
        }

        float wideBackOffset = spawnDescs[i].wideBackOffset;
        ImGui::Text("Wide Back");
        ImGui::SameLine(BattlePrefabLabelWidth);
        ImGui::SetNextItemWidth(max(80.f, ImGui::GetContentRegionAvail().x));
        if (ImGui::DragFloat("##BattleMonsterWideBackOffset", &wideBackOffset, 0.05f))
        {
            spawnDescs[i].wideBackOffset = wideBackOffset;
            changed = true;
        }

        float wideHeightOffset = spawnDescs[i].wideHeightOffset;
        ImGui::Text("Wide Height");
        ImGui::SameLine(BattlePrefabLabelWidth);
        ImGui::SetNextItemWidth(max(80.f, ImGui::GetContentRegionAvail().x));
        if (ImGui::DragFloat("##BattleMonsterWideHeightOffset", &wideHeightOffset, 0.05f))
        {
            spawnDescs[i].wideHeightOffset = wideHeightOffset;
            changed = true;
        }

        float wideLookHeightOffset = spawnDescs[i].wideLookHeightOffset;
        ImGui::Text("Wide Look Height");
        ImGui::SameLine(BattlePrefabLabelWidth);
        ImGui::SetNextItemWidth(max(80.f, ImGui::GetContentRegionAvail().x));
        if (ImGui::DragFloat("##BattleMonsterWideLookHeightOffset", &wideLookHeightOffset, 0.05f))
        {
            spawnDescs[i].wideLookHeightOffset = wideLookHeightOffset;
            changed = true;
        }

        ImGui::PopID();
    }

    if (removeIndex >= 0 && removeIndex < static_cast<int32>(spawnDescs.size()))
    {
        spawnDescs.erase(spawnDescs.begin() + removeIndex);
        changed = true;
    }

    if (ImGui::Button("Add", ImVec2(70.f, 0.f)))
    {
        if (!g_monsterBattlePrefabPickerEntries.empty())
            spawnDescs.push_back({ g_monsterBattlePrefabPickerEntries.front().prefabName });
        else
            spawnDescs.emplace_back();
        changed = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear", ImVec2(70.f, 0.f)))
    {
        spawnDescs.clear();
        changed = true;
    }

    ImGui::PopID();
    ImGui::Spacing();

    return changed;
}

void Inspector::Draw_ObjectReflection(const Shared<GameObject>& target)
{
    CHECK_NULL(target);

    const ClassReflectionInfo& info = target->Get_ReflectionInfo();
    if (info.properties.empty())
        return;

    Draw_ReflectionSection(info.className, target.get(), info);
}

bool Inspector::Draw_Vec2PropertyRow(const string& idPrefix, const PropertyInfo& property, Vec2& value)
{
    bool changed = false;

    ImGui::PushID(idPrefix.c_str());

    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float columnWidth = (availableWidth - 4.f) / 2.f;
    const float dragWidth = columnWidth - 22.f;

    static const ImVec4 colorX(0.75f, 0.18f, 0.18f, 1.f);
    static const ImVec4 colorY(0.25f, 0.60f, 0.25f, 1.f);

    changed |= Draw_VecAxis("X", colorX, value.x, property.dragSpeed, idPrefix + "_X", dragWidth);
    ImGui::SameLine(0.f, 4.f);
    changed |= Draw_VecAxis("Y", colorY, value.y, property.dragSpeed, idPrefix + "_Y", dragWidth);

    ImGui::PopID();
    return changed;
}

void Inspector::Draw_ReflectionSection(
    const string& headerId,
    void* instance,
    const ClassReflectionInfo& info)
{
    if (instance == nullptr || info.properties.empty())
        return;

    const char* sectionLabel =
        info.displayName.empty() ? info.className.c_str() : info.displayName.c_str();

    if (!ImGui::CollapsingHeader(sectionLabel, ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::PushID(headerId.c_str());
    Draw_ReflectionProperties(instance, info);
    ImGui::PopID();
}

void Inspector::Draw_ReflectionProperties(
    void* instance,
    const ClassReflectionInfo& info)
{
    for (const PropertyInfo& property : info.properties)
    {
        Draw_ReflectionProperty(info.className, instance, property);
        ImGui::Spacing();
    }
}

void Inspector::Draw_ReflectionProperty(
    const string& idPrefix,
    void* instance,
    const PropertyInfo& property)
{
    char* base = reinterpret_cast<char*>(instance);
    void* address = base + property.offset;

    const string visibleLabel =
        property.displayName.empty() ? property.name : property.displayName;
    const string hiddenId = "##" + idPrefix + "_" + property.name;

    switch (property.type)
    {
    case PropertyType::Bool:
    {
        bool* value = static_cast<bool*>(address);

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", visibleLabel.c_str());
        ImGui::SameLine(InspectorLabelWidth);
        ImGui::Checkbox(hiddenId.c_str(), value);
        break;
    }

    case PropertyType::Int:
    {
        int* value = static_cast<int*>(address);
        const bool useClamp = !(property.minVal == 0.f && property.maxVal == 0.f);

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", visibleLabel.c_str());
        ImGui::SameLine(InspectorLabelWidth);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

        if (useClamp)
        {
            const int minValue = static_cast<int>(property.minVal);
            const int maxValue = static_cast<int>(property.maxVal);
            ImGui::DragInt(hiddenId.c_str(), value, property.dragSpeed, minValue, maxValue);
        }
        else
        {
            ImGui::DragInt(hiddenId.c_str(), value, property.dragSpeed);
        }
        break;
    }

    case PropertyType::Float:
    {
        float* value = static_cast<float*>(address);
        const bool useClamp = !(property.minVal == 0.f && property.maxVal == 0.f);

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", visibleLabel.c_str());
        ImGui::SameLine(InspectorLabelWidth);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

        if (useClamp)
            ImGui::DragFloat(hiddenId.c_str(), value, property.dragSpeed, property.minVal, property.maxVal);
        else
            ImGui::DragFloat(hiddenId.c_str(), value, property.dragSpeed);

        break;
    }

    case PropertyType::Vec2:
    {
        Vec2* value = static_cast<Vec2*>(address);

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", visibleLabel.c_str());
        ImGui::SameLine(InspectorLabelWidth);

        Draw_Vec2PropertyRow(idPrefix + "_" + property.name, property, *value);
        break;
    }

    case PropertyType::Vec3:
    {
        Vec3* value = static_cast<Vec3*>(address);

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", visibleLabel.c_str());
        ImGui::SameLine(InspectorLabelWidth);

        Draw_Vec3PropertyRow(idPrefix + "_" + property.name, property, *value);
        break;
    }

    case PropertyType::Vec4:
    {
        Vec4* value = static_cast<Vec4*>(address);

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", visibleLabel.c_str());
        ImGui::SameLine(InspectorLabelWidth);

        Draw_Vec4PropertyRow(idPrefix + "_" + property.name, property, *value);
        break;
    }

    case PropertyType::Color:
    {
        Vec4* value = static_cast<Vec4*>(address);

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", visibleLabel.c_str());
        ImGui::SameLine(InspectorLabelWidth);

        Draw_ColorPropertyRow(idPrefix + "_" + property.name, *value);
        break;
    }

    case PropertyType::String:
    {
        std::string* value = static_cast<std::string*>(address);

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", visibleLabel.c_str());
        ImGui::SameLine(InspectorLabelWidth);

        if (property.isBlackboardKey && _btBlackboardKeys != nullptr)
        {
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

            const char* preview = value->empty() ? "(None)" : value->c_str();

            if (ImGui::BeginCombo(hiddenId.c_str(), preview))
            {
                for (const auto& key : *_btBlackboardKeys)
                {
                    if (key.type != property.blackboardValueType)
                        continue;

                    const bool isSelected = (*value == key.name);

                    if (ImGui::Selectable(key.name.c_str(), isSelected))
                        *value = key.name;

                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }
        }
        else if (idPrefix == "AN_SpawnActor" && property.name == "_staticClass")
        {
            Draw_SpawnActorClassPropertyRow(hiddenId, *value);
        }
        else
        {
            char buffer[512]{};
            strncpy_s(buffer, value->c_str(), _TRUNCATE);

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::InputText(hiddenId.c_str(), buffer, IM_ARRAYSIZE(buffer)))
                *value = buffer;
        }
        break;
    }

    case PropertyType::WString:
    {
        wstring* value = static_cast<wstring*>(address);

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", visibleLabel.c_str());
        ImGui::SameLine(InspectorLabelWidth);

        if (idPrefix == "EffectCom" && property.name == "_inspectorEffectName")
        {
            Draw_EffectAssetPickerRow(hiddenId, *value);
            break;
        }

        string valueText = String::ToString(*value);
        char buffer[512]{};
        strncpy_s(buffer, valueText.c_str(), _TRUNCATE);

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::InputText(hiddenId.c_str(), buffer, IM_ARRAYSIZE(buffer)))
            *value = String::ToWString(buffer);

        break;
    }

    case PropertyType::Enum:
    {
        int* value = static_cast<int*>(address);

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", visibleLabel.c_str());
        ImGui::SameLine(InspectorLabelWidth);

        Draw_EnumPropertyRow(idPrefix + "_" + property.name, property, *value);
        break;
    }

    case PropertyType::ReadOnly:
    default:
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", visibleLabel.c_str());
        ImGui::SameLine(InspectorLabelWidth);
        ImGui::BeginDisabled();
        ImGui::TextUnformatted("Unsupported");
        ImGui::EndDisabled();
        break;
    }
    }
}

bool Inspector::Draw_SpawnActorClassPropertyRow(
    const string& hiddenId,
    std::string& value)
{
    if (_gameObjectClassPickerEntries.empty())
        Rebuild_GameObjectClassPickerEntries();

    string previewLabel = "(select class)";
    for (const FGameObjectClassPickerEntry& entry : _gameObjectClassPickerEntries)
    {
        if (entry.className != value)
            continue;

        previewLabel = entry.displayName.empty() ? entry.className : entry.displayName;
        break;
    }

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (!ImGui::BeginCombo(hiddenId.c_str(), previewLabel.c_str()))
        return false;

    ImGui::InputTextWithHint(
        ("##Search" + hiddenId).c_str(),
        "Search class...",
        _spawnActorClassSearchBuffer,
        IM_ARRAYSIZE(_spawnActorClassSearchBuffer));
    ImGui::Separator();

    const string searchText = _spawnActorClassSearchBuffer;
    string currentCategory{};
    bool hasVisibleEntry = false;

    for (const FGameObjectClassPickerEntry& entry : _gameObjectClassPickerEntries)
    {
        if (!Matches_GameObjectClassPickerSearch(entry, searchText))
            continue;

        hasVisibleEntry = true;

        if (currentCategory != entry.category)
        {
            currentCategory = entry.category;
            ImGui::Separator();
            ImGui::TextDisabled("%s", currentCategory.c_str());
            ImGui::Separator();
        }

        const bool isSelected = (value == entry.className);
        const string selectableLabel = entry.displayName + "##" + entry.className;
        if (ImGui::Selectable(selectableLabel.c_str(), isSelected))
            value = entry.className; // 저장값은 runtime이 쓰는 실제 className으로 유지한다.

        if (isSelected)
            ImGui::SetItemDefaultFocus();
    }

    if (!hasVisibleEntry)
        ImGui::TextDisabled("No registered GameObject classes.");

    ImGui::EndCombo();
    return true;
}

void Inspector::Rebuild_GameObjectClassPickerEntries()
{
    _gameObjectClassPickerEntries.clear();

    GAME->Visit_GameObjectTypes([&](const Engine::TypeRegistry::GameObjectTypeInfo& info)
    {
        if (info.className.empty() || info.reflectionInfo == nullptr)
            return;

        const string category = info.reflectionInfo->category;
        if (category.empty())
            return;

        const string displayName =
            info.reflectionInfo->displayName.empty() ? info.className : info.reflectionInfo->displayName;

        _gameObjectClassPickerEntries.push_back({
            info.className,
            displayName,
            category,
        });
    });

    sort(_gameObjectClassPickerEntries.begin(), _gameObjectClassPickerEntries.end(),
        [](const FGameObjectClassPickerEntry& a, const FGameObjectClassPickerEntry& b)
        {
            if (a.category != b.category)
                return a.category < b.category;

            return a.displayName < b.displayName;
        });
}

bool Inspector::Matches_GameObjectClassPickerSearch(const FGameObjectClassPickerEntry& entry, const string& searchText)
{
    if (searchText.empty())
        return true;

    string loweredSearch = searchText;
    string loweredClass = entry.className;
    string loweredDisplay = entry.displayName;
    string loweredCategory = entry.category;

    transform(loweredSearch.begin(), loweredSearch.end(), loweredSearch.begin(), ::tolower);
    transform(loweredClass.begin(), loweredClass.end(), loweredClass.begin(), ::tolower);
    transform(loweredDisplay.begin(), loweredDisplay.end(), loweredDisplay.begin(), ::tolower);
    transform(loweredCategory.begin(), loweredCategory.end(), loweredCategory.begin(), ::tolower);

    return loweredClass.find(loweredSearch) != string::npos ||
        loweredDisplay.find(loweredSearch) != string::npos ||
        loweredCategory.find(loweredSearch) != string::npos;
}


bool Inspector::Draw_Vec3PropertyRow(
    const string& idPrefix,
    const PropertyInfo& property,
    Vec3& value)
{
    bool changed = false;

    ImGui::PushID(idPrefix.c_str());

    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float columnWidth = (availableWidth - 8.f) / 3.f;
    const float dragWidth = columnWidth - 22.f;

    static const ImVec4 colorX(0.75f, 0.18f, 0.18f, 1.f);
    static const ImVec4 colorY(0.25f, 0.60f, 0.25f, 1.f);
    static const ImVec4 colorZ(0.20f, 0.40f, 0.75f, 1.f);

    changed |= Draw_VecAxis("X", colorX, value.x, property.dragSpeed, idPrefix + "_X", dragWidth);
    ImGui::SameLine(0.f, 4.f);

    changed |= Draw_VecAxis("Y", colorY, value.y, property.dragSpeed, idPrefix + "_Y", dragWidth);
    ImGui::SameLine(0.f, 4.f);

    changed |= Draw_VecAxis("Z", colorZ, value.z, property.dragSpeed, idPrefix + "_Z", dragWidth);

    ImGui::PopID();

    return changed;
}

bool Inspector::Draw_Vec4PropertyRow(const string& idPrefix, const PropertyInfo& property, Vec4& value)
{
    ImGui::PushID(idPrefix.c_str());

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    const bool changed = ImGui::DragFloat4("##Vec4", &value.x, property.dragSpeed);

    ImGui::PopID();
    return changed;
}

bool Inspector::Draw_ColorPropertyRow(const string& idPrefix, Vec4& value)
{
    ImGui::PushID(idPrefix.c_str());

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

    const bool changed = ImGui::ColorEdit4(
        "##Color",
        &value.x,
        ImGuiColorEditFlags_Float | ImGuiColorEditFlags_AlphaBar);

    ImGui::PopID();
    return changed;
}

bool Inspector::Draw_EnumPropertyRow(const string& idPrefix, const PropertyInfo& property, int& value)
{
    ImGui::PushID(idPrefix.c_str());

    if (property.enumNames.empty() || property.enumValues.empty() || property.enumNames.size() != property.enumValues.size())
    {
        ImGui::BeginDisabled();
        ImGui::TextUnformatted("Invalid Enum Metadata");
        ImGui::EndDisabled();
        ImGui::PopID();
        return false;
    }

    int currentIndex = -1;
    for (int i = 0; i < static_cast<int>(property.enumValues.size()); ++i)
    {
        if (property.enumValues[i] == value)
        {
            currentIndex = i;
            break;
        }
    }

    const char* preview =
        (currentIndex >= 0 && currentIndex < static_cast<int>(property.enumNames.size()))
        ? property.enumNames[currentIndex].c_str()
        : "Unknown";

    bool changed = false;

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::BeginCombo("##Enum", preview))
    {
        for (int i = 0; i < static_cast<int>(property.enumNames.size()); ++i)
        {
            const bool isSelected = (currentIndex == i);

            if (ImGui::Selectable(property.enumNames[i].c_str(), isSelected))
            {
                value = property.enumValues[i];
                changed = true;
            }

            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    ImGui::PopID();

    return changed;
}

bool Inspector::Draw_VecAxis(
    const char* axisLabel,
    const ImVec4& color,
    float& value,
    float speed,
    const string& uniqueId,
    float width)
{
    ImGui::PushStyleColor(ImGuiCol_Button, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);

    ImGui::Button(axisLabel, ImVec2(20.f, ImGui::GetFrameHeight()));

    ImGui::PopStyleColor(3);
    ImGui::SameLine(0.f, 2.f);

    ImGui::SetNextItemWidth(width);
    const string dragId = "##" + uniqueId;

    return ImGui::DragFloat(dragId.c_str(), &value, speed);
}

void Inspector::Set_BTBlackboardKeys(const vector<Engine::BTBlackboardKeyView>* keys)
{
    _btBlackboardKeys = keys;
}

Shared<Inspector> Inspector::Create()
{
    return make_shared<Inspector>();
}

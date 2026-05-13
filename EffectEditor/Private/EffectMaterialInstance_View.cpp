#include "EffectMaterialInstance_View.h"

#include "EffectEditorInstance.h"
#include "EffectMaterial_View.h"
#include "Emitter_View.h"
#include "Helper_DetailProperty.h"

NS_BEGIN(EffectEditor)

EffectMaterialInstance_View::EffectMaterialInstance_View()
    : Editor_Window{ L"Effect Material Instance", ICON_FA_PALETTE }
{
}

void EffectMaterialInstance_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string& windowName = Get_ImGuiWindowName();

    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

        Shared<Emitter_View> emitterView{};
        AuthoringEmitter* emitter{ nullptr };
        RequiredModuleData* requiredData{ nullptr };
        if (!Resolve_Target(emitterView, emitter, requiredData))
        {
            ImGui::TextDisabled("Material instance target is not selected.");
        }
        else
        {
            ImGui::TextUnformatted("Embedded Effect Material Instance");
            ImGui::Separator();

            if (ImGui::Button("Open Definition"))
                Open_DefinitionWindow(requiredData->material.definitionName);

            ImGui::SameLine();
            ImGui::TextDisabled("%s", requiredData->material.definitionName.c_str());

            bool changed = false;
            if (ImGui::Button("Reset to Definition Defaults"))
            {
                requiredData->material = Resolve_EffectMaterialDefinition(requiredData->material.definitionName).defaultInstance;
                changed = true;
            }

            changed |= Draw_InstanceProperties(requiredData->material);
            if (changed)
                Apply_MaterialChanged(emitterView, *emitter, *requiredData);
        }
    }

    Set_Open(isOpen);
    ImGui::End();
}

void EffectMaterialInstance_View::Open_Instance(uint32 emitterId, uint32 moduleId)
{
    _targetEmitterId = emitterId;
    _targetModuleId = moduleId;
    Set_Open(true);
}

bool EffectMaterialInstance_View::Resolve_Target(
    Shared<Emitter_View>& outEmitterView,
    AuthoringEmitter*& outEmitter,
    RequiredModuleData*& outRequiredData) const
{
    outEmitterView.reset();
    outEmitter = nullptr;
    outRequiredData = nullptr;

    if (EDITOR == nullptr)
        return false;

    const Shared<Editor_Window> emitterWindow = EDITOR->Get_Window(L"Emitter");
    outEmitterView = dynamic_pointer_cast<Emitter_View>(emitterWindow);
    if (outEmitterView == nullptr)
        return false;

    outEmitter = outEmitterView->Find_Emitter(_targetEmitterId);
    if (outEmitter == nullptr)
        return false;

    AuthoringModule* module = outEmitterView->Find_Module(_targetEmitterId, _targetModuleId);
    if (module == nullptr || module->type != AuthoringModuleType::Required)
        return false;

    outRequiredData = get_if<RequiredModuleData>(&module->data);
    return outRequiredData != nullptr;
}

bool EffectMaterialInstance_View::Draw_InstanceProperties(EffectMaterialInstanceData& material)
{
    const EffectMaterialInstanceData defaultData = Resolve_EffectMaterialDefinition(material.definitionName).defaultInstance;
    bool changed = false;

    if (ImGui::CollapsingHeader("Texture", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (DetailProperty::Begin_PropertyTable("EffectMaterialInstanceTexture"))
        {
            changed |= DetailProperty::Draw_StringProperty("Definition", material.definitionName, defaultData.definitionName);
            changed |= DetailProperty::Draw_StringProperty("Main Texture", material.mainTextureId, defaultData.mainTextureId);
            changed |= DetailProperty::Draw_StringProperty("Noise Texture", material.noiseTextureId, defaultData.noiseTextureId);
            DetailProperty::End_PropertyTable();
        }
    }

    if (ImGui::CollapsingHeader("Color / Scalar", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (DetailProperty::Begin_PropertyTable("EffectMaterialInstanceColorScalar"))
        {
            changed |= DetailProperty::Draw_ColorProperty("Tint", material.tint, defaultData.tint);
            changed |= DetailProperty::Draw_FloatProperty("Intensity", material.intensity, defaultData.intensity);
            changed |= DetailProperty::Draw_FloatProperty("Opacity Power", material.opacityPower, defaultData.opacityPower);
            changed |= DetailProperty::Draw_FloatProperty("Noise Strength", material.noiseStrength, defaultData.noiseStrength);
            DetailProperty::End_PropertyTable();
        }
    }

    if (ImGui::CollapsingHeader("Options", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (DetailProperty::Begin_PropertyTable("EffectMaterialInstanceOptions"))
        {
            changed |= DetailProperty::Draw_StringProperty("Blend Mode", material.blendMode, defaultData.blendMode);
            changed |= DetailProperty::Draw_StringProperty("Opacity Source", material.opacitySource, defaultData.opacitySource);
            changed |= DetailProperty::Draw_UintProperty("SubUV Rows", material.subUvRows, defaultData.subUvRows);
            changed |= DetailProperty::Draw_UintProperty("SubUV Cols", material.subUvCols, defaultData.subUvCols);
            DetailProperty::End_PropertyTable();
        }
    }

    return changed;
}

void EffectMaterialInstance_View::Apply_MaterialChanged(
    const Shared<Emitter_View>& emitterView,
    AuthoringEmitter& emitter,
    const RequiredModuleData& requiredData)
{
    emitter.previewDirty = true;
    emitter.rendererType = requiredData.rendererType;
    emitter.textureId = requiredData.material.mainTextureId;
    emitter.materialName = requiredData.material.definitionName;
    emitter.resourceSummary = requiredData.material.mainTextureId;

    if (emitterView)
        emitterView->MarkDirty();

    MarkDirty();
}

void EffectMaterialInstance_View::Open_DefinitionWindow(const string& definitionName)
{
    if (EDITOR == nullptr)
        return;

    const Shared<Editor_Window> materialWindow = EDITOR->Get_Window(L"Effect Material");
    const Shared<EffectMaterial_View> materialView = dynamic_pointer_cast<EffectMaterial_View>(materialWindow);
    if (materialView == nullptr)
        return;

    materialView->Open_Definition(definitionName);
}

Shared<EffectMaterialInstance_View> EffectMaterialInstance_View::Create()
{
    return make_shared<EffectMaterialInstance_View>();
}

NS_END

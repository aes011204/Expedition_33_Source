#include "EffectMaterial_View.h"

NS_BEGIN(EffectEditor)

EffectMaterial_View::EffectMaterial_View()
    : Editor_Window{ L"Effect Material", ICON_FA_CIRCLE_INFO }
{
}

void EffectMaterial_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string& windowName = Get_ImGuiWindowName();

    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

        Render_DefinitionInfo(Get_CurrentDefinition());
    }

    Set_Open(isOpen);
    ImGui::End();
}

void EffectMaterial_View::Open_Definition(const string& definitionName)
{
    _definitionName = definitionName;
    Set_Open(true);
}

const EffectMaterialDefinitionData& EffectMaterial_View::Get_CurrentDefinition() const
{
    return Resolve_EffectMaterialDefinition(_definitionName);
}

void EffectMaterial_View::Render_DefinitionInfo(const EffectMaterialDefinitionData& definition)
{
    ImGui::TextUnformatted("Effect Material Definition");
    ImGui::Separator();

    if (ImGui::BeginTable("EffectMaterialDefinitionInfo", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
    {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch, 0.35f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.65f);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("Definition");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(definition.name.c_str());

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("Renderer Family");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(definition.rendererFamily.c_str());

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("Default Texture");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(definition.defaultInstance.mainTextureId.c_str());

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("Blend / Opacity");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%s / %s", definition.defaultInstance.blendMode.c_str(), definition.defaultInstance.opacitySource.c_str());

        ImGui::EndTable();
    }
}

Shared<EffectMaterial_View> EffectMaterial_View::Create()
{
    return make_shared<EffectMaterial_View>();
}

NS_END

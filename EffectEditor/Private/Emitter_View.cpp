#include "Emitter_View.h"

NS_BEGIN(EffectEditor)

Emitter_View::Emitter_View()
    : Editor_Window{ L"Emitter", ICON_FA_FIRE }
{
    // TODO : 임시. 나중에 Initialize 함수 등을 만들고, 제대로 구조를 잡을 것
    _emitters = {
        Emitter_Row{
            "Flame_1",
            "Fx_Unstable_Fire",
            "Sprite",
            "Supported",
            "Texture_Effect_PyroSmoke_Density",
            "Pyro1Smoke_AdvancedFlowPlayer_v3",
            "Texture_Effect_PyroSmoke_Density",
            ImVec4{ 1.0f, 0.42f, 0.10f, 1.0f },
            {
                { "Required", "texture, duration, alignment", true, true },
                { "Spawn", "rate / burst frame", true, true },
                { "Lifetime", "particle life range", false, true },
                { "Initial Size", "spawn-time size", false, true },
                { "Initial Velocity", "spawn-time velocity", false, true },
                { "Color Over Life", "life-normalized tint", false, true },
                { "Alpha Over Life", "life-normalized opacity", false, true },
                { "SubUV Frame", "flipbook placeholder", false, true },
            },
        },
        Emitter_Row{
            "Fountain002_1",
            "Fx_Unstable_Fire",
            "Sprite",
            "Supported",
            "Texture_Effect_FireSymbol / Texture_Effect_Aura",
            "M_Symbol",
            "Texture_Effect_FireSymbol",
            ImVec4{ 0.95f, 0.78f, 0.18f, 1.0f },
            {
                { "Required", "symbol texture, additive-like tint", true, true },
                { "Spawn", "short burst placeholder", true, true },
                { "Lifetime", "short particle life", false, true },
                { "Initial Size", "symbol scale", false, true },
                { "Color Over Life", "emissive tint placeholder", false, true },
                { "Alpha Over Life", "fade curve placeholder", false, true },
            },
        },
        Emitter_Row{
            "Ribbon_4",
            "Fx_Unstable_Fire",
            "Ribbon",
            "Later",
            "Unsupported ribbon renderer",
            "DefaultRibbonMaterial",
            "-",
            ImVec4{ 0.38f, 0.55f, 1.0f, 1.0f },
            {
                { "Required", "renderer contract placeholder", true, false },
                { "Spawn", "deferred", true, false },
                { "Ribbon TypeData", "unsupported in v1", false, false },
            },
        },
    };
}

void Emitter_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string& windowName = Get_ImGuiWindowName();
    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

        Draw_Toolbar();
        ImGui::Separator();
        Draw_EmitterBoard();
        ImGui::Separator();
        Draw_SelectionSummary();
    }

    Set_Open(isOpen);
    ImGui::End();
}

Shared<Emitter_View> Emitter_View::Create()
{
    return make_shared<Emitter_View>();
}

void Emitter_View::Draw_Toolbar()
{
    ImGui::TextUnformatted("Emitter Stack");
    ImGui::SameLine();
    ImGui::TextDisabled("Unstable_Fire authoring sample");
}

void Emitter_View::Draw_EmitterBoard()
{
    constexpr float columnWidth = 230.f;
    constexpr float boardHeight = 430.f;

    if (ImGui::BeginChild("##EmitterBoard", ImVec2(0.f, boardHeight), true, ImGuiWindowFlags_HorizontalScrollbar))
    {
        for (size_t emitterIndex = 0; emitterIndex < _emitters.size(); ++emitterIndex)
        {
            ImGui::PushID(static_cast<int>(emitterIndex));
            Draw_EmitterColumn(emitterIndex, _emitters[emitterIndex]);
            ImGui::PopID();

            if (emitterIndex + 1 < _emitters.size())
                ImGui::SameLine(0.f, 8.f);
        }

        ImGui::Dummy(ImVec2(columnWidth, 1.f));
    }
    ImGui::EndChild();
}

void Emitter_View::Draw_EmitterColumn(size_t emitterIndex, const Emitter_Row& emitter)
{
    constexpr float columnWidth = 230.f;
    constexpr float headerHeight = 88.f;

    const bool isSelected = _selectedEmitterIndex == emitterIndex;
    const ImVec4 headerColor = isSelected ? emitter.accentColor : ImVec4{ 0.36f, 0.36f, 0.36f, 1.0f };
    const ImVec4 textColor = isSelected ? ImVec4{ 0.05f, 0.05f, 0.05f, 1.0f } : ImGui::GetStyleColorVec4(ImGuiCol_Text);

    ImGui::BeginGroup();
    ImGui::PushStyleColor(ImGuiCol_Button, headerColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, emitter.accentColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, emitter.accentColor);
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    if (ImGui::Button(emitter.name.c_str(), ImVec2(columnWidth, headerHeight)))
        _selectedEmitterIndex = emitterIndex;
    ImGui::PopStyleColor(4);

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - headerHeight + 28.f);
    ImGui::Indent(8.f);
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    ImGui::Text("%s", emitter.sourceSystem.c_str());
    ImGui::Text("%s / %s", emitter.rendererType.c_str(), emitter.supportState.c_str());
    ImGui::PopStyleColor();
    ImGui::Unindent(8.f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 18.f);

    for (const Module_Row& module : emitter.modules)
        Draw_ModuleRow(module);

    ImGui::EndGroup();
}

void Emitter_View::Draw_ModuleRow(const Module_Row& module)
{
    constexpr float rowWidth = 230.f;
    constexpr float rowHeight = 28.f;

    const ImVec4 rowColor = module.required
                            ? ImVec4{ 0.74f, 0.68f, 0.26f, 1.0f }
                            : ImVec4{ 0.18f, 0.19f, 0.24f, 1.0f };
    const ImVec4 disabledColor{ 0.16f, 0.16f, 0.17f, 1.0f };
    const ImVec4 textColor = module.required
                             ? ImVec4{ 0.08f, 0.08f, 0.08f, 1.0f }
                             : ImGui::GetStyleColorVec4(ImGuiCol_Text);

    ImGui::PushStyleColor(ImGuiCol_Button, module.enabled ? rowColor : disabledColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, rowColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, rowColor);
    ImGui::PushStyleColor(ImGuiCol_Text, module.enabled ? textColor : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::Button(module.name.c_str(), ImVec2(rowWidth, rowHeight));
    ImGui::PopStyleColor(4);

    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", module.summary.c_str());
}

void Emitter_View::Draw_SelectionSummary() const
{
    if (_emitters.empty() || _selectedEmitterIndex >= _emitters.size())
    {
        ImGui::TextDisabled("No emitter selected.");
        return;
    }

    const Emitter_Row& emitter = _emitters[_selectedEmitterIndex];

    ImGui::TextUnformatted("Selection");
    ImGui::Spacing();
    ImGui::Text("Emitter: %s", emitter.name.c_str());
    ImGui::Text("Source: %s", emitter.sourceSystem.c_str());
    ImGui::Text("Renderer: %s", emitter.rendererType.c_str());
    ImGui::Text("Resource: %s", emitter.resourceSummary.c_str());
    ImGui::Text("Material: %s", emitter.materialName.c_str());
    ImGui::Text("Texture: %s", emitter.textureId.c_str());
    ImGui::TextDisabled("Details editing will reuse Inspector after the selection contract is added.");
}

NS_END

#include "Emitter_View.h"

#include "Editor_Context.h"
#include "EffectEditorInstance.h"

NS_BEGIN(EffectEditor)

Emitter_View::Emitter_View()
    : Editor_Window{ L"Emitter", ICON_FA_FIRE }
{
}

HRESULT Emitter_View::Initialize()
{
    CHECK_FAILED(Editor_Window::Initialize(), E_FAIL);

    _emitters.clear();
    _emitters.push_back(Make_DefaultSpriteEmitter());
    _selectedEmitterIndex = 0;
    _selectedModuleIndex.reset();

    if (EDITOR != nullptr && EDITOR->Get_EditorContext() != nullptr)
        EDITOR->Get_EditorContext()->Set_SelectEffectEmitter(_emitters.front().id);

    return S_OK;
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

        Draw_EmitterBoard();
    }

    Set_Open(isOpen);
    ImGui::End();
}

AuthoringEmitter* Emitter_View::Find_Emitter(uint32 emitterId)
{
    for (AuthoringEmitter& emitter : _emitters)
    {
        if (emitter.id == emitterId)
            return &emitter;
    }

    return nullptr;
}

AuthoringModule* Emitter_View::Find_Module(uint32 emitterId, uint32 moduleId)
{
    AuthoringEmitter* emitter = Find_Emitter(emitterId);
    if (nullptr == emitter)
        return nullptr;

    for (AuthoringModule& module : emitter->modules)
    {
        if (module.id == moduleId)
            return &module;
    }

    return nullptr;
}

uint32 Emitter_View::Issue_AuthoringId()
{
    return _nextAuthoringId++;
}

AuthoringEmitter Emitter_View::Make_DefaultSpriteEmitter()
{
    AuthoringEmitter emitter{};
    emitter.id = Issue_AuthoringId();
    emitter.name = "Particle Emitter";
    emitter.rendererType = "Sprite";
    emitter.supportState = "Supported";
    emitter.resourceSummary = "Texture_Snow";
    emitter.materialName = "DefaultSpriteMaterial";
    emitter.textureId = "Texture_Snow";
    emitter.enabled = true;
    emitter.previewDirty = true;

    emitter.modules.push_back(Make_RequiredModule());
    emitter.modules.push_back(Make_SpawnModule());
    emitter.modules.push_back(Make_LifetimeModule());
    emitter.modules.push_back(Make_InitialSizeModule());
    emitter.modules.push_back(Make_InitialVelocityModule());
    emitter.modules.push_back(Make_ColorOverLifeModule());

    return emitter;
}

AuthoringModule Emitter_View::Make_RequiredModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::Required;
    module.key = "Required";
    module.displayName = L"필수";
    module.summary = L"텍스처, 머티리얼, 에디터 컬러";
    module.enabled = true;
    module.movable = false;
    module.removable = false;
    module.data = RequiredModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_SpawnModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::Spawn;
    module.key = "Spawn";
    module.displayName = L"스폰";
    module.summary = L"생성량 / 루프 기준";
    module.enabled = true;
    module.movable = false;
    module.removable = false;
    module.data = SpawnModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_LifetimeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::Lifetime;
    module.key = "Lifetime";
    module.displayName = L"수명";
    module.summary = L"파티클 수명 범위";
    module.enabled = true;
    module.movable = true;
    module.removable = true;
    module.data = LifetimeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_InitialSizeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::InitialSize;
    module.key = "Initial Size";
    module.displayName = L"초기 크기";
    module.summary = L"생성 시점 크기";
    module.enabled = true;
    module.movable = true;
    module.removable = true;
    module.data = InitialSizeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_InitialVelocityModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::InitialVelocity;
    module.key = "Initial Velocity";
    module.displayName = L"초기 속도";
    module.summary = L"생성 위치와 퍼짐 속도";
    module.enabled = true;
    module.movable = true;
    module.removable = true;
    module.data = InitialVelocityModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_ColorOverLifeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::ColorOverLife;
    module.key = "Color Over Life";
    module.displayName = L"컬러 오버 라이프";
    module.summary = L"수명 진행률 기준 색 변화";
    module.enabled = true;
    module.movable = true;
    module.removable = true;
    module.data = ColorOverLifeModuleData{};
    return module;
}

void Emitter_View::Clamp_Selection()
{
    if (_emitters.empty())
    {
        _selectedEmitterIndex.reset();
        _selectedModuleIndex.reset();
        return;
    }

    if (!_selectedEmitterIndex.has_value())
    {
        _selectedModuleIndex.reset();
        return;
    }

    _selectedEmitterIndex = min(_selectedEmitterIndex.value(), _emitters.size() - 1);

    const auto& modules = _emitters[_selectedEmitterIndex.value()].modules;
    if (_selectedModuleIndex.has_value() && _selectedModuleIndex.value() >= modules.size())
        _selectedModuleIndex.reset();
}

void Emitter_View::Draw_EmitterBoard()
{
    if (ImGui::BeginChild("##EmitterBoard", ImVec2(0.f, 0.f), true, ImGuiWindowFlags_HorizontalScrollbar))
    {
        const ImVec2 boardMin = ImGui::GetWindowPos();
        const ImVec2 boardMax{
            boardMin.x + ImGui::GetWindowWidth(),
            boardMin.y + ImGui::GetWindowHeight()
        };
        const float columnHeight = max(1.f, ImGui::GetContentRegionAvail().y);
        float lastColumnRight = ImGui::GetCursorScreenPos().x;

        for (size_t emitterIndex = 0; emitterIndex < _emitters.size(); ++emitterIndex)
        {
            ImGui::PushID(static_cast<int>(emitterIndex));
            Draw_EmitterColumn(emitterIndex, _emitters[emitterIndex], columnHeight);
            lastColumnRight = ImGui::GetItemRectMax().x;
            ImGui::PopID();

            if (emitterIndex + 1 < _emitters.size())
                ImGui::SameLine(0.f, 0.f);
        }

        const ImVec2 mousePos = ImGui::GetMousePos();
        const bool isRightBlankHovered =
            ImGui::IsWindowHovered() &&
            !ImGui::IsAnyItemHovered() &&
            mousePos.x > lastColumnRight &&
            mousePos.x < boardMax.x &&
            mousePos.y >= boardMin.y &&
            mousePos.y < boardMax.y;

        if (isRightBlankHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            _selectedEmitterIndex.reset();
            _selectedModuleIndex.reset();
            if (EDITOR != nullptr && EDITOR->Get_EditorContext() != nullptr)
                EDITOR->Get_EditorContext()->Clear_EffectSelection();
        }
        else if (isRightBlankHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            _selectedEmitterIndex.reset();
            _selectedModuleIndex.reset();
            if (EDITOR != nullptr && EDITOR->Get_EditorContext() != nullptr)
                EDITOR->Get_EditorContext()->Clear_EffectSelection();
            ImGui::OpenPopup("##EmitterBoardBlankPopup");
        }

        ImGui::SetNextWindowPos(ImGui::GetMousePos(), ImGuiCond_Appearing);
        if (ImGui::BeginPopup("##EmitterBoardBlankPopup"))
        {
            if (ImGui::Selectable("새 이미터"))
            {
                _emitters.push_back(Make_DefaultSpriteEmitter());
                _selectedEmitterIndex = _emitters.size() - 1;
                _selectedModuleIndex.reset();
                if (EDITOR != nullptr && EDITOR->Get_EditorContext() != nullptr)
                    EDITOR->Get_EditorContext()->Set_SelectEffectEmitter(_emitters.back().id);
            }

            ImGui::EndPopup();
        }
    }
    ImGui::EndChild();
}

void Emitter_View::Draw_EmitterColumn(size_t emitterIndex, const AuthoringEmitter& emitter, float columnHeight)
{
    constexpr float headerPaddingX = 10.f;
    constexpr float headerPaddingY = 12.f;

    const bool isSelected = _selectedEmitterIndex.has_value() && _selectedEmitterIndex.value() == emitterIndex;
    const ImVec4 accentColor{
        kEmitterAccentOrange[0],
        kEmitterAccentOrange[1],
        kEmitterAccentOrange[2],
        kEmitterAccentOrange[3]
    };
    const ImVec4 headerColor = isSelected ? accentColor : ImVec4{ 0.36f, 0.36f, 0.36f, 1.0f };
    const ImVec4 textColor = isSelected ? ImVec4{ 0.05f, 0.05f, 0.05f, 1.0f } : ImGui::GetStyleColorVec4(ImGuiCol_Text);

    ImGui::BeginGroup();
    const ImVec2 columnMin = ImGui::GetCursorScreenPos();
    const ImVec2 columnMax{
        columnMin.x + kEmitterColumnWidth,
        columnMin.y + columnHeight
    };
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(
        columnMin,
        columnMax,
        ImGui::ColorConvertFloat4ToU32(ImVec4{
            kEmitterColumnBackgroundColor[0],
            kEmitterColumnBackgroundColor[1],
            kEmitterColumnBackgroundColor[2],
            kEmitterColumnBackgroundColor[3]
        })
    );
    drawList->AddRectFilled(
        ImVec2(columnMax.x - kEmitterDividerWidth, columnMin.y),
        columnMax,
        ImGui::ColorConvertFloat4ToU32(ImVec4{
            kEmitterDividerColor[0],
            kEmitterDividerColor[1],
            kEmitterDividerColor[2],
            kEmitterDividerColor[3]
        })
    );

    const ImVec2 headerCursor = ImGui::GetCursorPos();
    ImGui::PushStyleColor(ImGuiCol_Button, headerColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, accentColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, accentColor);
    if (ImGui::Button("##EmitterHeader", ImVec2(kEmitterColumnWidth, kEmitterHeaderHeight)))
    {
        _selectedEmitterIndex = emitterIndex;
        _selectedModuleIndex.reset();
        if (EDITOR != nullptr && EDITOR->Get_EditorContext() != nullptr)
            EDITOR->Get_EditorContext()->Set_SelectEffectEmitter(emitter.id);
    }
    const bool isHeaderHovered = ImGui::IsItemHovered();
    ImGui::PopStyleColor(3);

    const ImVec2 afterHeaderCursor = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(headerCursor.x + headerPaddingX, headerCursor.y + headerPaddingY));
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    ImGui::PushTextWrapPos(headerCursor.x + kEmitterColumnWidth - headerPaddingX);
    ImGui::BeginGroup();
    ImGui::TextUnformatted(emitter.name.c_str());
    ImGui::EndGroup();
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    ImGui::SetCursorPos(afterHeaderCursor);

    if (isHeaderHovered)
        ImGui::SetTooltip("%s\n%s\n%s", emitter.resourceSummary.c_str(), emitter.materialName.c_str(), emitter.textureId.c_str());

    const ImVec2 typeDataReserveMin = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(kEmitterColumnWidth, kEmitterTypeDataReserveHeight));
    const ImVec2 typeDataReserveMax = ImGui::GetItemRectMax();
    drawList->AddRectFilled(
        typeDataReserveMin,
        typeDataReserveMax,
        ImGui::ColorConvertFloat4ToU32(ImVec4{
            kEmitterTypeDataReserveColor[0],
            kEmitterTypeDataReserveColor[1],
            kEmitterTypeDataReserveColor[2],
            kEmitterTypeDataReserveColor[3]
        })
    );

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.f));
    for (size_t moduleIndex = 0; moduleIndex < emitter.modules.size(); ++moduleIndex)
    {
        ImGui::PushID(static_cast<int>(moduleIndex));
        Draw_ModuleRow(emitterIndex, moduleIndex, emitter.modules[moduleIndex]);
        ImGui::PopID();
    }
    ImGui::PopStyleVar();

    const float reservedTailHeight = columnMax.y - ImGui::GetCursorScreenPos().y;
    if (reservedTailHeight > 0.f)
        ImGui::InvisibleButton("##EmitterReservedTail", ImVec2(kEmitterColumnWidth, reservedTailHeight));

    ImGui::EndGroup();
}

void Emitter_View::Draw_ModuleRow(size_t emitterIndex, size_t moduleIndex, const AuthoringModule& module)
{
    constexpr float rowHeight = 28.f;
    constexpr float rowPaddingX = 10.f;

    const bool isRequiredRow = module.type == AuthoringModuleType::Required;
    const bool isSpawnRow = module.type == AuthoringModuleType::Spawn;
    const ImVec4 rowColor = isRequiredRow
                            ? ImVec4{ 0.78f, 0.75f, 0.34f, 1.0f }
                            : isSpawnRow
                              ? ImVec4{ 0.78f, 0.36f, 0.38f, 1.0f }
                              : ImVec4{ 0.18f, 0.19f, 0.24f, 1.0f };
    const ImVec4 disabledColor{ 0.16f, 0.16f, 0.17f, 1.0f };
    const bool usesCategoryColor = isRequiredRow || isSpawnRow;
    const ImVec4 textColor = usesCategoryColor
                             ? ImVec4{ 0.08f, 0.08f, 0.08f, 1.0f }
                             : ImGui::GetStyleColorVec4(ImGuiCol_Text);

    const ImVec2 rowCursor = ImGui::GetCursorPos();
    ImGui::PushStyleColor(ImGuiCol_Button, module.enabled || usesCategoryColor ? rowColor : disabledColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, rowColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, rowColor);
    ImGui::PushStyleColor(ImGuiCol_Text, module.enabled ? textColor : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::PushID(module.key.c_str());
    ImGui::Button("##ModuleRow", ImVec2(kEmitterColumnWidth, rowHeight));
    if (ImGui::IsItemClicked())
    {
        _selectedEmitterIndex = emitterIndex;
        _selectedModuleIndex = moduleIndex;
        if (EDITOR != nullptr && EDITOR->Get_EditorContext() != nullptr)
            EDITOR->Get_EditorContext()->Set_SelectEffectModule(_emitters[emitterIndex].id, module.id);
    }
    ImGui::PopID();
    const bool isRowHovered = ImGui::IsItemHovered();
    ImGui::PopStyleColor(4);

    const ImVec2 afterRowCursor = ImGui::GetCursorPos();
    const float textOffsetY = (rowHeight - ImGui::GetTextLineHeight()) * 0.5f;
    ImGui::SetCursorPos(ImVec2(rowCursor.x + rowPaddingX, rowCursor.y + textOffsetY));
    ImGui::PushStyleColor(ImGuiCol_Text, module.enabled ? textColor : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextUnformatted(String::ToString(module.displayName).c_str());
    ImGui::PopStyleColor();
    ImGui::SetCursorPos(afterRowCursor);

    if (isRowHovered)
    {
        const string summary = String::ToString(module.summary);
        ImGui::SetTooltip("%s", summary.c_str());
    }
}

Shared<Emitter_View> Emitter_View::Create()
{
    return make_shared<Emitter_View>();
}

NS_END

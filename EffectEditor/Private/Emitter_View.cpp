#include "Emitter_View.h"

#include "Editor_Context.h"
#include "EffectEditorInstance.h"
#include "Helper_ImGui.h"

NS_BEGIN(EffectEditor)

Emitter_View::Emitter_View()
    : Editor_Window{ L"Emitter", ICON_FA_FIRE }
{
}

HRESULT Emitter_View::Initialize()
{
    CHECK_FAILED(Editor_Window::Initialize(), E_FAIL);

    _particleSystemData = {};
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

AuthoringEmitter* Emitter_View::Get_SelectedOrFirstEmitter()
{
    Clamp_Selection();

    if (_selectedEmitterIndex.has_value())
        return &_emitters[_selectedEmitterIndex.value()];

    if (!_emitters.empty())
        return &_emitters.front();

    return nullptr;
}

void Emitter_View::Clear_PreviewDirty(uint32 emitterId)
{
    AuthoringEmitter* emitter = Find_Emitter(emitterId);
    if (nullptr == emitter)
        return;

    emitter->previewDirty = false;
}

bool Emitter_View::Has_PreviewDirtyEmitter() const
{
    for (const AuthoringEmitter& emitter : _emitters)
    {
        if (emitter.previewDirty)
            return true;
    }

    return false;
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
    emitter.enabled = true;
    emitter.previewDirty = true;

    AuthoringModule requiredModule = Make_RequiredModule();
    if (const auto* requiredData = get_if<RequiredModuleData>(&requiredModule.data))
    {
        emitter.rendererType = requiredData->rendererType;
        emitter.materialName = requiredData->materialName;
        emitter.textureId = requiredData->textureId;
        emitter.resourceSummary = requiredData->textureId;
    }

    emitter.modules.push_back(move(requiredModule));
    emitter.modules.push_back(Make_SpawnModule());
    emitter.modules.push_back(Make_LifetimeModule());
    emitter.modules.push_back(Make_InitialSizeModule());
    emitter.modules.push_back(Make_InitialVelocityModule());
    emitter.modules.push_back(Make_ColorOverLifeModule());

    return emitter;
}

AuthoringEmitter Emitter_View::Clone_Emitter(const AuthoringEmitter& sourceEmitter)
{
    AuthoringEmitter clonedEmitter = sourceEmitter;
    clonedEmitter.id = Issue_AuthoringId();
    clonedEmitter.previewDirty = true;

    for (AuthoringModule& module : clonedEmitter.modules)
        module.id = Issue_AuthoringId();

    return clonedEmitter;
}

AuthoringModule Emitter_View::Make_RequiredModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::Required;
    module.key = "Required";
    module.displayName = L"필수";
    module.summary = L"텍스처, 머티리얼";
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
    module.displayName = L"수명에 따른 색상";
    module.summary = L"수명 진행률 기준 색 변화";
    module.enabled = true;
    module.movable = true;
    module.removable = true;
    module.data = ColorOverLifeModuleData{};
    return module;
}

void Emitter_View::Clear_Selection()
{
    _selectedEmitterIndex.reset();
    _selectedModuleIndex.reset();

    if (EDITOR != nullptr && EDITOR->Get_EditorContext() != nullptr)
        EDITOR->Get_EditorContext()->Clear_EffectSelection();
}

void Emitter_View::Select_Emitter(size_t emitterIndex)
{
    if (emitterIndex >= _emitters.size())
    {
        Clear_Selection();
        return;
    }

    _selectedEmitterIndex = emitterIndex;
    _selectedModuleIndex.reset();

    if (EDITOR != nullptr && EDITOR->Get_EditorContext() != nullptr)
        EDITOR->Get_EditorContext()->Set_SelectEffectEmitter(_emitters[emitterIndex].id);
}

void Emitter_View::Insert_NewEmitter(size_t insertIndex)
{
    const size_t clampedIndex = min(insertIndex, _emitters.size());
    _emitters.insert(_emitters.begin() + static_cast<ptrdiff_t>(clampedIndex), Make_DefaultSpriteEmitter());
    Select_Emitter(clampedIndex);
    MarkDirty();
}

void Emitter_View::Duplicate_Emitter(size_t emitterIndex)
{
    if (emitterIndex >= _emitters.size())
        return;

    const size_t insertIndex = emitterIndex + 1;
    _emitters.insert(
        _emitters.begin() + static_cast<ptrdiff_t>(insertIndex),
        Clone_Emitter(_emitters[emitterIndex]));
    Select_Emitter(insertIndex);
    MarkDirty();
}

void Emitter_View::Delete_Emitter(size_t emitterIndex)
{
    if (emitterIndex >= _emitters.size())
        return;

    _emitters.erase(_emitters.begin() + static_cast<ptrdiff_t>(emitterIndex));
    Clamp_Selection();

    if (_emitters.empty())
    {
        Clear_Selection();
    }
    else
    {
        const size_t nextIndex = min(emitterIndex, _emitters.size() - 1);
        Select_Emitter(nextIndex);
    }

    MarkDirty();
}

void Emitter_View::Queue_EmitterAction(PendingEmitterAction action, size_t emitterIndex)
{
    _pendingEmitterAction = action;
    _pendingEmitterActionIndex = emitterIndex;
}

void Emitter_View::Process_PendingEmitterAction()
{
    if (_pendingEmitterAction == PendingEmitterAction::None || !_pendingEmitterActionIndex.has_value())
        return;

    const size_t emitterIndex = _pendingEmitterActionIndex.value();
    switch (_pendingEmitterAction)
    {
    case PendingEmitterAction::Duplicate:
        Duplicate_Emitter(emitterIndex);
        break;
    case PendingEmitterAction::Delete:
        Delete_Emitter(emitterIndex);
        break;
    case PendingEmitterAction::InsertBefore:
        Insert_NewEmitter(emitterIndex);
        break;
    case PendingEmitterAction::InsertAfter:
        Insert_NewEmitter(emitterIndex + 1);
        break;
    case PendingEmitterAction::None:
    default:
        break;
    }

    _pendingEmitterAction = PendingEmitterAction::None;
    _pendingEmitterActionIndex.reset();
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

string Emitter_View::Build_ModuleWarningTooltip(const AuthoringEmitter& emitter, const AuthoringModule& module) const
{
    if (!emitter.enabled)
        return "비활성화된 이미터입니다.";

    if (!module.enabled && module.type != AuthoringModuleType::Required)
        return "비활성화된 모듈입니다.";

    // TypeData/module compatibility 경고는 후속 단계에서 이 경로에 추가한다.
    return {};
}

void Emitter_View::Draw_EmitterContextMenu(size_t emitterIndex)
{
    if (ImGui::BeginMenu("이미터"))
    {
        if (ImGui::MenuItem("이미터 복제"))
            Queue_EmitterAction(PendingEmitterAction::Duplicate, emitterIndex);

        if (ImGui::MenuItem("이미터 삭제"))
            Queue_EmitterAction(PendingEmitterAction::Delete, emitterIndex);

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("파티클 시스템"))
    {
        if (ImGui::MenuItem("앞에 새 이미터 추가"))
            Queue_EmitterAction(PendingEmitterAction::InsertBefore, emitterIndex);

        if (ImGui::MenuItem("뒤에 새 이미터 추가"))
            Queue_EmitterAction(PendingEmitterAction::InsertAfter, emitterIndex);

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("타입 데이터"))
    {
        ImGui::BeginDisabled();
        ImGui::MenuItem("새 애님 트레일 데이터");
        ImGui::MenuItem("새 빔 데이터");
        ImGui::MenuItem("새 GPU 스프라이트");
        ImGui::MenuItem("새 메시 데이터");
        ImGui::MenuItem("새 리본 데이터");
        ImGui::EndDisabled();
        ImGui::EndMenu();
    }
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
            Clear_Selection();
        }
        else if (isRightBlankHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            Clear_Selection();
            ImGui::OpenPopup("##EmitterBoardBlankPopup");
        }

        ImGui::SetNextWindowPos(ImGui::GetMousePos(), ImGuiCond_Appearing);
        if (ImGui::BeginPopup("##EmitterBoardBlankPopup"))
        {
            if (ImGui::Selectable("새 이미터"))
                Insert_NewEmitter(_emitters.size());

            ImGui::EndPopup();
        }

        Process_PendingEmitterAction();
    }
    ImGui::EndChild();
}

void Emitter_View::Draw_EmitterColumn(size_t emitterIndex, AuthoringEmitter& emitter, float columnHeight)
{
    constexpr float headerPaddingX = 10.f;
    constexpr float headerPaddingY = 12.f;

    const bool isSelected = _selectedEmitterIndex.has_value() && _selectedEmitterIndex.value() == emitterIndex;
    const ImVec4 accentColor = To_ImVec4(kEmitterAccentColor);
    const ImVec4 headerColor = isSelected ? accentColor : To_ImVec4(Color{ 0.36f, 0.36f, 0.36f, 1.0f });
    const ImVec4 textColor = isSelected ? To_ImVec4(Color{ 0.05f, 0.05f, 0.05f, 1.0f }) : ImGui::GetStyleColorVec4(ImGuiCol_Text);

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
        ImGui::ColorConvertFloat4ToU32(To_ImVec4(kEmitterColumnBackgroundColor))
    );
    drawList->AddRectFilled(
        ImVec2(columnMax.x - kEmitterDividerWidth, columnMin.y),
        columnMax,
        ImGui::ColorConvertFloat4ToU32(To_ImVec4(kEmitterDividerColor))
    );

    const ImVec2 headerCursor = ImGui::GetCursorPos();
    ImGui::PushStyleColor(ImGuiCol_Button, headerColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, headerColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, headerColor);
    ImGui::SetNextItemAllowOverlap();
    if (ImGui::Button("##EmitterHeader", ImVec2(kEmitterColumnWidth, kEmitterHeaderHeight)))
        Select_Emitter(emitterIndex);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
        Select_Emitter(emitterIndex);
    if (ImGui::BeginPopupContextItem("##EmitterHeaderPopup"))
    {
        Draw_EmitterContextMenu(emitterIndex);
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(3);

    const ImVec2 afterHeaderCursor = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(headerCursor.x + headerPaddingX, headerCursor.y + headerPaddingY));
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    ImGui::PushTextWrapPos(headerCursor.x + kEmitterColumnWidth - headerPaddingX);
    ImGui::TextUnformatted(emitter.name.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();

    ImGui::SetCursorPos(
        ImVec2(
            headerCursor.x + headerPaddingX,
            headerCursor.y + kEmitterHeaderHeight - ImGui::GetFrameHeight() - headerPaddingY
        )
    );
    ImGui::PushID("EmitterEnabled");
    if (ImGui::Checkbox("##Enabled", &emitter.enabled))
    {
        emitter.previewDirty = true;
        MarkDirty();
    }
    ImGui::PopID();

    ImGui::SetCursorPos(afterHeaderCursor);

    ImGui::InvisibleButton("##EmitterTypeDataSlot", ImVec2(kEmitterColumnWidth, kEmitterTypeDataReserveHeight));
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
        Select_Emitter(emitterIndex);
    if (ImGui::BeginPopupContextItem("##EmitterTypeDataPopup"))
    {
        Draw_EmitterContextMenu(emitterIndex);
        ImGui::EndPopup();
    }

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
    {
        ImGui::InvisibleButton("##EmitterReservedTail", ImVec2(kEmitterColumnWidth, reservedTailHeight));
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            Select_Emitter(emitterIndex);
        if (ImGui::BeginPopupContextItem("##EmitterReservedTailPopup"))
        {
            Draw_EmitterContextMenu(emitterIndex);
            ImGui::EndPopup();
        }
    }

    ImGui::EndGroup();
}

void Emitter_View::Draw_ModuleRow(size_t emitterIndex, size_t moduleIndex, AuthoringModule& module)
{
    constexpr float rowHeight = 28.f;
    constexpr float rowPaddingX = 10.f;
    constexpr float moduleCheckboxRightPadding = 34.f;

    const bool isSelected =
        _selectedEmitterIndex.has_value() &&
        _selectedEmitterIndex.value() == emitterIndex &&
        _selectedModuleIndex.has_value() &&
        _selectedModuleIndex.value() == moduleIndex;
    const bool isRequiredRow = module.type == AuthoringModuleType::Required;
    const bool isSpawnRow = module.type == AuthoringModuleType::Spawn;
    const ImVec4 accentColor = To_ImVec4(kEmitterAccentColor);
    const ImVec4 categoryColor = To_ImVec4(
        isRequiredRow
        ? Color{ 0.78f, 0.75f, 0.34f, 1.0f }
        : isSpawnRow
          ? Color{ 0.78f, 0.36f, 0.38f, 1.0f }
          : Color{ 0.18f, 0.19f, 0.24f, 1.0f }
    );
    const ImVec4 disabledColor = To_ImVec4(Color{ 0.16f, 0.16f, 0.17f, 1.0f });
    const bool usesCategoryColor = isRequiredRow || isSpawnRow;
    const ImVec4 rowColor = isSelected ? accentColor : categoryColor;
    const ImVec4 textColor = isSelected
                             ? To_ImVec4(Color{ 0.05f, 0.05f, 0.05f, 1.0f })
                             : usesCategoryColor
                               ? To_ImVec4(Color{ 0.08f, 0.08f, 0.08f, 1.0f })
                               : ImGui::GetStyleColorVec4(ImGuiCol_Text);

    const ImVec2 rowCursor = ImGui::GetCursorPos();
    const ImVec4 buttonColor = module.enabled || usesCategoryColor ? rowColor : disabledColor;
    ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, buttonColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, buttonColor);
    ImGui::PushStyleColor(ImGuiCol_Text, module.enabled ? textColor : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::PushID(module.key.c_str());
    ImGui::SetNextItemAllowOverlap();
    ImGui::Button("##ModuleRow", ImVec2(kEmitterColumnWidth, rowHeight));
    if (ImGui::IsItemClicked())
    {
        Select_Emitter(emitterIndex);
        _selectedModuleIndex = moduleIndex;
        if (EDITOR != nullptr && EDITOR->Get_EditorContext() != nullptr)
            EDITOR->Get_EditorContext()->Set_SelectEffectModule(_emitters[emitterIndex].id, module.id);
    }
    ImGui::PopID();
    const bool isRowHovered = ImGui::IsItemHovered();
    ImGui::PopStyleColor(4);
    const string warningTooltip = Build_ModuleWarningTooltip(_emitters[emitterIndex], module);

    const ImVec2 afterRowCursor = ImGui::GetCursorPos();
    const float textOffsetY = (rowHeight - ImGui::GetTextLineHeight()) * 0.5f;
    ImGui::SetCursorPos(ImVec2(rowCursor.x + rowPaddingX, rowCursor.y + textOffsetY));
    ImGui::PushStyleColor(ImGuiCol_Text, module.enabled ? textColor : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextUnformatted(String::ToString(module.displayName).c_str());
    ImGui::PopStyleColor();

    if (!isRequiredRow)
    {
        ImGui::SetCursorPos(
            ImVec2(
                rowCursor.x + kEmitterColumnWidth - moduleCheckboxRightPadding,
                rowCursor.y + (rowHeight - ImGui::GetFrameHeight()) * 0.5f
            )
        );
        ImGui::PushID("ModuleEnabled");
        if (ImGui::Checkbox("##Enabled", &module.enabled))
        {
            _emitters[emitterIndex].previewDirty = true;
            MarkDirty();
        }
        ImGui::PopID();
    }

    ImGui::SetCursorPos(afterRowCursor);

    if (isRowHovered && !warningTooltip.empty())
        ImGui::SetTooltip("%s", warningTooltip.c_str());
}

Shared<Emitter_View> Emitter_View::Create()
{
    return make_shared<Emitter_View>();
}

NS_END

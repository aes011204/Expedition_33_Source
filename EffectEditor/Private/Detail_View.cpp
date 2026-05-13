#include "Detail_View.h"

#include "Editor_Context.h"
#include "EffectEditorInstance.h"
#include "Emitter_View.h"
#include "Helper_DetailProperty.h"

NS_BEGIN(EffectEditor)

Detail_View::Detail_View()
    : Editor_Window{ L"Detail", ICON_FA_PEN_TO_SQUARE }
{
}

Detail_View::~Detail_View()
{
    Free();
}

void Detail_View::Update(float)
{
}

void Detail_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string& windowName = Get_ImGuiWindowName();

    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

        Render_EffectAuthoringDetail();
    }

    Set_Open(isOpen);
    ImGui::End();
}

void Detail_View::Render_EffectAuthoringDetail()
{
    if (EDITOR == nullptr || EDITOR->Get_EditorContext() == nullptr)
    {
        ImGui::TextDisabled("Editor context is not available.");
        return;
    }

    const EffectAuthoringSelection& selection = EDITOR->Get_EditorContext()->Get_EffectSelection();
    const Shared<Editor_Window> emitterWindow = EDITOR->Get_Window(L"Emitter");
    const Shared<Emitter_View> emitterView = dynamic_pointer_cast<Emitter_View>(emitterWindow);
    if (emitterView == nullptr)
    {
        ImGui::TextDisabled("Emitter view is not available.");
        return;
    }

    if (selection.kind == EffectAuthoringSelectionKind::None)
    {
        Render_ParticleSystemDetail(emitterView->Get_ParticleSystemData());
        return;
    }

    AuthoringEmitter* emitter = emitterView->Find_Emitter(selection.emitterId);
    if (nullptr == emitter)
    {
        ImGui::TextDisabled("Selected emitter is not available.");
        return;
    }

    if (selection.kind == EffectAuthoringSelectionKind::Emitter)
    {
        Render_EmitterDetail(*emitter);
        return;
    }

    AuthoringModule* module = emitterView->Find_Module(selection.emitterId, selection.moduleId);
    if (nullptr == module)
    {
        ImGui::TextDisabled("Selected module is not available.");
        return;
    }

    Render_ModuleDetail(*emitter, *module);
}

void Detail_View::Render_ParticleSystemDetail(ParticleSystemAuthoringData& data)
{
    ImGui::TextUnformatted("파티클 시스템");
    ImGui::Separator();

    const ParticleSystemAuthoringData defaultData{};
    bool changed = false;
    if (DetailProperty::Begin_PropertyTable("ParticleSystem"))
    {
        changed |= DetailProperty::Draw_BoolProperty("자동 비활성화", data.autoDeactivate, defaultData.autoDeactivate);
        DetailProperty::End_PropertyTable();
    }
    if (changed)
        MarkDirty();
}

void Detail_View::Render_EmitterDetail(AuthoringEmitter& emitter)
{
    ImGui::TextUnformatted("파티클");
    ImGui::Separator();

    const AuthoringEmitter defaultData{};
    const RequiredModuleData* requiredData = Find_RequiredModuleData(emitter);
    bool changed = false;

    if (DetailProperty::Begin_PropertyTable("Emitter"))
    {
        changed |= DetailProperty::Draw_StringProperty("이미터 이름", emitter.name, defaultData.name);

        ImGui::PushID("RendererType");
        DetailProperty::Draw_PropertyLabel("렌더러 타입");
        ImGui::TextDisabled("%s", requiredData != nullptr ? requiredData->rendererType.c_str() : emitter.rendererType.c_str());
        DetailProperty::Draw_ResetButton(false);
        ImGui::PopID();

        DetailProperty::End_PropertyTable();
    }

    Mark_Changed(emitter, changed);
}

void Detail_View::Render_ModuleDetail(AuthoringEmitter& emitter, AuthoringModule& module)
{
    ImGui::TextUnformatted(String::ToString(module.displayName).c_str());
    ImGui::Separator();

    ModuleDetailDrawContext context{};
    bool changed = false;

    switch (module.type)
    {
    case AuthoringModuleType::Required:
        if (auto* data = get_if<RequiredModuleData>(&module.data))
            changed = _requiredModuleDetail.Draw(emitter, module, *data, context);
        break;
    case AuthoringModuleType::Spawn:
        if (auto* data = get_if<SpawnModuleData>(&module.data))
            changed = _spawnModuleDetail.Draw(emitter, module, *data, context);
        break;
    case AuthoringModuleType::Lifetime:
        if (auto* data = get_if<LifetimeModuleData>(&module.data))
            changed = _lifetimeModuleDetail.Draw(emitter, module, *data, context);
        break;
    case AuthoringModuleType::InitialSize:
        if (auto* data = get_if<InitialSizeModuleData>(&module.data))
            changed = _initialSizeModuleDetail.Draw(emitter, module, *data, context);
        break;
    case AuthoringModuleType::InitialVelocity:
        if (auto* data = get_if<InitialVelocityModuleData>(&module.data))
            changed = _initialVelocityModuleDetail.Draw(emitter, module, *data, context);
        break;
    case AuthoringModuleType::InitialColor:
        if (auto* data = get_if<InitialColorModuleData>(&module.data))
            changed = _initialColorModuleDetail.Draw(emitter, module, *data, context);
        break;
    case AuthoringModuleType::ColorOverLife:
        if (auto* data = get_if<ColorOverLifeModuleData>(&module.data))
            changed = _colorOverLifeModuleDetail.Draw(emitter, module, *data, context);
        break;
    default:
        ImGui::TextDisabled("Unsupported module type.");
        break;
    }

    Mark_Changed(emitter, changed);
}

void Detail_View::Mark_Changed(AuthoringEmitter& emitter, bool changed)
{
    if (!changed)
        return;

    emitter.previewDirty = true;
    MarkDirty();
}

RequiredModuleData* Detail_View::Find_RequiredModuleData(AuthoringEmitter& emitter)
{
    for (AuthoringModule& module : emitter.modules)
    {
        if (module.type != AuthoringModuleType::Required)
            continue;

        return get_if<RequiredModuleData>(&module.data);
    }

    return nullptr;
}

Shared<Detail_View> Detail_View::Create()
{
    return make_shared<Detail_View>();
}

void Detail_View::Free()
{
    __super::Free();
}

NS_END

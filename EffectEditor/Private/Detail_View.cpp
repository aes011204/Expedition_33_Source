#include "Detail_View.h"

#include <cstring>

#include "Editor_Context.h"
#include "EffectEditorInstance.h"
#include "Emitter_View.h"

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
        ImGui::TextUnformatted("No Selected");
        return;
    }

    const EffectAuthoringSelection& selection = EDITOR->Get_EditorContext()->Get_EffectSelection();
    if (selection.kind == EffectAuthoringSelectionKind::None)
    {
        ImGui::TextUnformatted("No Selected");
        ImGui::Separator();
        ImGui::TextDisabled("Add Component");
        return;
    }

    const Shared<Editor_Window> emitterWindow = EDITOR->Get_Window(L"Emitter");
    const Shared<Emitter_View> emitterView = dynamic_pointer_cast<Emitter_View>(emitterWindow);
    if (emitterView == nullptr)
    {
        ImGui::TextDisabled("Emitter view is not available.");
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

void Detail_View::Render_EmitterDetail(AuthoringEmitter& emitter)
{
    ImGui::TextUnformatted("파티클");
    ImGui::Separator();

    bool changed = false;
    changed |= Draw_StringProperty("이미터 이름", emitter.name);
    changed |= Draw_BoolProperty("활성화", emitter.enabled);
    changed |= Draw_StringProperty("렌더러 타입", emitter.rendererType);
    changed |= Draw_StringProperty("지원 상태", emitter.supportState);
    Mark_Changed(emitter, changed);
}

void Detail_View::Render_ModuleDetail(AuthoringEmitter& emitter, AuthoringModule& module)
{
    ImGui::TextUnformatted(String::ToString(module.displayName).c_str());
    ImGui::Separator();

    bool changed = false;
    changed |= Draw_BoolProperty("활성화", module.enabled);
    Mark_Changed(emitter, changed);

    ImGui::Spacing();

    switch (module.type)
    {
    case AuthoringModuleType::Required:
        if (auto* data = std::get_if<RequiredModuleData>(&module.data))
            Render_RequiredDetail(emitter, module, *data);
        break;
    case AuthoringModuleType::Spawn:
        if (auto* data = std::get_if<SpawnModuleData>(&module.data))
            Render_SpawnDetail(emitter, module, *data);
        break;
    case AuthoringModuleType::Lifetime:
        if (auto* data = std::get_if<LifetimeModuleData>(&module.data))
            Render_LifetimeDetail(emitter, module, *data);
        break;
    case AuthoringModuleType::InitialSize:
        if (auto* data = std::get_if<InitialSizeModuleData>(&module.data))
            Render_InitialSizeDetail(emitter, module, *data);
        break;
    case AuthoringModuleType::InitialVelocity:
        if (auto* data = std::get_if<InitialVelocityModuleData>(&module.data))
            Render_InitialVelocityDetail(emitter, module, *data);
        break;
    case AuthoringModuleType::ColorOverLife:
        if (auto* data = std::get_if<ColorOverLifeModuleData>(&module.data))
            Render_ColorOverLifeDetail(emitter, module, *data);
        break;
    default:
        ImGui::TextDisabled("Unsupported module type.");
        break;
    }
}

void Detail_View::Render_RequiredDetail(AuthoringEmitter& emitter, AuthoringModule&, RequiredModuleData& data)
{
    bool changed = false;
    changed |= Draw_StringProperty("Texture Id", data.textureId);
    changed |= Draw_StringProperty("Material", data.materialName);
    changed |= Draw_ColorProperty("Editor Color", data.editorColor);

    if (changed)
    {
        emitter.textureId = data.textureId;
        emitter.materialName = data.materialName;
        emitter.resourceSummary = data.textureId;
    }

    Mark_Changed(emitter, changed);
}

void Detail_View::Render_SpawnDetail(AuthoringEmitter& emitter, AuthoringModule&, SpawnModuleData& data)
{
    bool changed = false;
    changed |= Draw_UintProperty("Instance Count", data.instanceCount);
    changed |= Draw_BoolProperty("Loop", data.loop);
    Mark_Changed(emitter, changed);
}

void Detail_View::Render_LifetimeDetail(AuthoringEmitter& emitter, AuthoringModule&, LifetimeModuleData& data)
{
    const bool changed = Draw_Vec2Property("Life Time Min/Max", data.lifeTime);
    Mark_Changed(emitter, changed);
}

void Detail_View::Render_InitialSizeDetail(AuthoringEmitter& emitter, AuthoringModule&, InitialSizeModuleData& data)
{
    const bool changed = Draw_Vec2Property("Scale Min/Max", data.scale);
    Mark_Changed(emitter, changed);
}

void Detail_View::Render_InitialVelocityDetail(AuthoringEmitter& emitter, AuthoringModule&, InitialVelocityModuleData& data)
{
    bool changed = false;
    changed |= Draw_Vec3Property("Center", data.center);
    changed |= Draw_Vec3Property("Range", data.range);
    changed |= Draw_Vec3Property("Pivot", data.pivot);
    changed |= Draw_Vec2Property("Speed Min/Max", data.speed);
    Mark_Changed(emitter, changed);
}

void Detail_View::Render_ColorOverLifeDetail(AuthoringEmitter& emitter, AuthoringModule&, ColorOverLifeModuleData& data)
{
    bool changed = false;
    changed |= Draw_ColorProperty("Start Color", data.startColor);
    changed |= Draw_ColorProperty("End Color", data.endColor);
    Mark_Changed(emitter, changed);
}

bool Detail_View::Draw_StringProperty(const char* label, string& value)
{
    char buffer[256]{};
    const size_t copyLength = min(value.size(), sizeof(buffer) - 1);
    memcpy(buffer, value.c_str(), copyLength);

    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    ImGui::SameLine(160.f);
    ImGui::SetNextItemWidth(-1.f);
    const bool changed = ImGui::InputText("##Value", buffer, sizeof(buffer));
    ImGui::PopID();

    if (changed)
        value = buffer;

    return changed;
}

bool Detail_View::Draw_BoolProperty(const char* label, bool& value)
{
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    ImGui::SameLine(160.f);
    const bool changed = ImGui::Checkbox("##Value", &value);
    ImGui::PopID();
    return changed;
}

bool Detail_View::Draw_UintProperty(const char* label, uint32& value)
{
    int intValue = static_cast<int>(value);

    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    ImGui::SameLine(160.f);
    ImGui::SetNextItemWidth(-1.f);
    const bool changed = ImGui::DragInt("##Value", &intValue, 1.f, 0, 100000);
    ImGui::PopID();

    if (changed)
        value = static_cast<uint32>(max(0, intValue));

    return changed;
}

bool Detail_View::Draw_Vec2Property(const char* label, Vec2& value, float speed)
{
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    ImGui::SameLine(160.f);
    ImGui::SetNextItemWidth(-1.f);
    const bool changed = ImGui::DragFloat2("##Value", &value.x, speed);
    ImGui::PopID();
    return changed;
}

bool Detail_View::Draw_Vec3Property(const char* label, Vec3& value, float speed)
{
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    ImGui::SameLine(160.f);
    ImGui::SetNextItemWidth(-1.f);
    const bool changed = ImGui::DragFloat3("##Value", &value.x, speed);
    ImGui::PopID();
    return changed;
}

bool Detail_View::Draw_ColorProperty(const char* label, float value[4])
{
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    ImGui::SameLine(160.f);
    ImGui::SetNextItemWidth(-1.f);
    const bool changed = ImGui::ColorEdit4(
        "##Value",
        value,
        ImGuiColorEditFlags_Float | ImGuiColorEditFlags_AlphaBar);
    ImGui::PopID();
    return changed;
}

void Detail_View::Mark_Changed(AuthoringEmitter& emitter, bool changed)
{
    if (!changed)
        return;

    emitter.previewDirty = true;
    MarkDirty();
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

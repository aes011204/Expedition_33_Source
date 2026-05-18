#include "Detail_View.h"

#include "Editor_Context.h"
#include "EffectEditorInstance.h"
#include "DetailPropertyContext.h"

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

    Commit_PendingAuthoringEditOnTargetChange(emitterView, selection);

    if (selection.kind == EffectAuthoringSelectionKind::None)
    {
        Render_ParticleSystemDetail(emitterView, selection, emitterView->Get_ParticleSystemData());
        return;
    }

    AuthoringEmitter* emitter = emitterView->Find_Emitter(selection.emitterId);
    if (nullptr == emitter)
    {
        Commit_PendingAuthoringEdit(emitterView);
        ImGui::TextDisabled("Selected emitter is not available.");
        return;
    }

    if (selection.kind == EffectAuthoringSelectionKind::Emitter)
    {
        Render_EmitterDetail(emitterView, selection, *emitter);
        return;
    }

    if (selection.kind == EffectAuthoringSelectionKind::TypeData)
    {
        Render_TypeDataDetail(emitterView, selection, *emitter);
        return;
    }

    AuthoringModule* module = emitterView->Find_Module(selection.emitterId, selection.moduleId);
    if (nullptr == module)
    {
        Commit_PendingAuthoringEdit(emitterView);
        ImGui::TextDisabled("Selected module is not available.");
        return;
    }

    Render_ModuleDetail(emitterView, selection, *emitter, *module);
}

void Detail_View::Render_ParticleSystemDetail(
    const Shared<Emitter_View>& emitterView,
    const EffectAuthoringSelection& selection,
    ParticleSystemAuthoringData& data)
{
    ImGui::TextUnformatted("파티클 시스템");
    ImGui::Separator();

    const Emitter_View::AuthoringSnapshot beforeSnapshot = emitterView->Capture_AuthoringSnapshot();
    const ParticleSystemAuthoringData defaultData{};
    bool changed = false;
    if (_detailPropertyContext.Begin_PropertyTable("ParticleSystem"))
    {
        changed |= _detailPropertyContext.Draw_BoolProperty("자동 비활성화", data.autoDeactivate, defaultData.autoDeactivate);
        _detailPropertyContext.End_PropertyTable();
    }
    if (changed)
    {
        Begin_PendingAuthoringEdit(emitterView, selection, beforeSnapshot, "Edit Particle System Detail");
        MarkDirty();
    }

    Commit_PendingAuthoringEditIfIdle(emitterView);
}

void Detail_View::Render_EmitterDetail(
    const Shared<Emitter_View>& emitterView,
    const EffectAuthoringSelection& selection,
    AuthoringEmitter& emitter)
{
    ImGui::TextUnformatted("파티클");
    ImGui::Separator();

    const Emitter_View::AuthoringSnapshot beforeSnapshot = emitterView->Capture_AuthoringSnapshot();
    const AuthoringEmitter defaultData{};
    const RequiredModuleData* requiredData = Find_RequiredModuleData(emitter);
    bool changed = false;

    if (_detailPropertyContext.Begin_PropertyTable("Emitter"))
    {
        changed |= _detailPropertyContext.Draw_StringProperty("이미터 이름", emitter.name, defaultData.name);

        ImGui::PushID("RendererType");
        _detailPropertyContext.Draw_PropertyLabel("렌더러 타입");
        ImGui::TextDisabled("%s", requiredData != nullptr ? requiredData->rendererType.c_str() : emitter.rendererType.c_str());
        _detailPropertyContext.Draw_ResetButton(false);
        ImGui::PopID();

        _detailPropertyContext.End_PropertyTable();
    }

    if (changed)
        Begin_PendingAuthoringEdit(emitterView, selection, beforeSnapshot, "Edit Emitter Detail");

    Mark_Changed(emitter, changed);
    Commit_PendingAuthoringEditIfIdle(emitterView);
}

void Detail_View::Render_TypeDataDetail(
    const Shared<Emitter_View>& emitterView,
    const EffectAuthoringSelection& selection,
    AuthoringEmitter& emitter)
{
    ImGui::TextUnformatted("타입 데이터");
    ImGui::Separator();
    ImGui::TextDisabled("장착된 타입 데이터가 없습니다.");

    Mark_Changed(emitter, false);
    Commit_PendingAuthoringEditIfIdle(emitterView);
}

void Detail_View::Render_ModuleDetail(
    const Shared<Emitter_View>& emitterView,
    const EffectAuthoringSelection& selection,
    AuthoringEmitter& emitter,
    AuthoringModule& module)
{
    ImGui::TextUnformatted(String::ToString(module.displayName).c_str());
    ImGui::Separator();

    const Emitter_View::AuthoringSnapshot beforeSnapshot = emitterView->Capture_AuthoringSnapshot();
    bool changed = false;

    switch (module.type)
    {
    case AuthoringModuleType::Required:
        if (auto* data = get_if<RequiredModuleData>(&module.data))
            changed = _requiredModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::Spawn:
        if (auto* data = get_if<SpawnModuleData>(&module.data))
            changed = _spawnModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::Lifetime:
        if (auto* data = get_if<LifetimeModuleData>(&module.data))
            changed = _lifetimeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::InitialSize:
        if (auto* data = get_if<InitialSizeModuleData>(&module.data))
            changed = _initialSizeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::InitialLocation:
        if (auto* data = get_if<InitialLocationModuleData>(&module.data))
            changed = _initialLocationModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::SphereLocation:
        if (auto* data = get_if<SphereLocationModuleData>(&module.data))
            changed = _sphereLocationModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::InitialVelocity:
        if (auto* data = get_if<InitialVelocityModuleData>(&module.data))
            changed = _initialVelocityModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::InitialRadialVelocity:
        if (auto* data = get_if<InitialRadialVelocityModuleData>(&module.data))
            changed = _initialRadialVelocityModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::Acceleration:
        if (auto* data = get_if<AccelerationModuleData>(&module.data))
            changed = _accelerationModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::Drag:
        if (auto* data = get_if<DragModuleData>(&module.data))
            changed = _dragModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::VelocityOverLife:
        if (auto* data = get_if<VelocityOverLifeModuleData>(&module.data))
            changed = _velocityOverLifeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::InitialRotation:
        if (auto* data = get_if<InitialRotationModuleData>(&module.data))
            changed = _initialRotationModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::RotationOverLife:
        if (auto* data = get_if<RotationOverLifeModuleData>(&module.data))
            changed = _rotationOverLifeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::InitialRotationRate:
        if (auto* data = get_if<InitialRotationRateModuleData>(&module.data))
            changed = _initialRotationRateModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::RotationRateScaleByLife:
        if (auto* data = get_if<RotationRateScaleByLifeModuleData>(&module.data))
            changed = _rotationRateScaleByLifeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::InitialColor:
        if (auto* data = get_if<InitialColorModuleData>(&module.data))
            changed = _initialColorModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::ColorOverLife:
        if (auto* data = get_if<ColorOverLifeModuleData>(&module.data))
            changed = _colorOverLifeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::SubUVFrameOverLife:
        if (auto* data = get_if<SubUVFrameOverLifeModuleData>(&module.data))
            changed = _subUvFrameOverLifeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::SizeByLife:
        if (auto* data = get_if<SizeByLifeModuleData>(&module.data))
            changed = _sizeByLifeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    default:
        ImGui::TextDisabled("Unsupported module type.");
        break;
    }

    if (changed)
        Begin_PendingAuthoringEdit(emitterView, selection, beforeSnapshot, "Edit Module Detail");

    Mark_Changed(emitter, changed);
    Commit_PendingAuthoringEditIfIdle(emitterView);
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

void Detail_View::Commit_PendingAuthoringEditOnTargetChange(
    const Shared<Emitter_View>& emitterView,
    const EffectAuthoringSelection& selection)
{
    if (!_hasPendingAuthoringEdit || Is_SameSelection(_pendingAuthoringSelection, selection))
        return;

    Commit_PendingAuthoringEdit(emitterView);
}

void Detail_View::Begin_PendingAuthoringEdit(
    const Shared<Emitter_View>& emitterView,
    const EffectAuthoringSelection& selection,
    const Emitter_View::AuthoringSnapshot& beforeSnapshot,
    const string& description)
{
    if (emitterView == nullptr)
        return;

    if (!_hasPendingAuthoringEdit)
    {
        _hasPendingAuthoringEdit = true;
        _pendingAuthoringSelection = selection;
        _pendingAuthoringSnapshot = beforeSnapshot;
        _pendingAuthoringDescription = description;
    }
}

void Detail_View::Commit_PendingAuthoringEditIfIdle(const Shared<Emitter_View>& emitterView)
{
    if (!_hasPendingAuthoringEdit || ImGui::IsAnyItemActive())
        return;

    Commit_PendingAuthoringEdit(emitterView);
}

void Detail_View::Commit_PendingAuthoringEdit(const Shared<Emitter_View>& emitterView)
{
    if (!_hasPendingAuthoringEdit || emitterView == nullptr)
        return;

    Emitter_View::AuthoringSnapshot afterSnapshot = emitterView->Capture_AuthoringSnapshot();
    afterSnapshot.selectedEmitterIndex = _pendingAuthoringSnapshot.selectedEmitterIndex;
    afterSnapshot.selectedTypeData = _pendingAuthoringSnapshot.selectedTypeData;
    afterSnapshot.selectedModuleIndex = _pendingAuthoringSnapshot.selectedModuleIndex;
    emitterView->Execute_AuthoringSnapshotCommand(
        _pendingAuthoringSnapshot,
        afterSnapshot,
        _pendingAuthoringDescription
    );

    _hasPendingAuthoringEdit = false;
    _pendingAuthoringSelection = {};
    _pendingAuthoringSnapshot = {};
    _pendingAuthoringDescription.clear();
}

bool Detail_View::Is_SameSelection(const EffectAuthoringSelection& lhs, const EffectAuthoringSelection& rhs)
{
    return
        lhs.kind == rhs.kind &&
        lhs.emitterId == rhs.emitterId &&
        lhs.moduleId == rhs.moduleId;
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

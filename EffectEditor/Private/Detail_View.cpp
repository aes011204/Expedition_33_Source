#include "Detail_View.h"

#include "Editor_Context.h"
#include "EffectEditorInstance.h"
#include "DetailPropertyContext.h"

NS_BEGIN(EffectEditor)

namespace
{
    constexpr TrailCurveQuality kTrailCurveQualityValues[] = {
        TrailCurveQuality::Basic,
        TrailCurveQuality::Smooth,
        TrailCurveQuality::HighQuality,
        TrailCurveQuality::Custom,
    };

    const char* Get_TrailCurveQualityLabel(TrailCurveQuality value)
    {
        switch (value)
        {
        case TrailCurveQuality::Basic:
            return "기본";
        case TrailCurveQuality::Smooth:
            return "부드럽게";
        case TrailCurveQuality::HighQuality:
            return "고품질";
        case TrailCurveQuality::Custom:
            return "사용자 지정";
        default:
            return "기본";
        }
    }

    const char* Get_TrailCurveQualityTooltip(TrailCurveQuality value)
    {
        switch (value)
        {
        case TrailCurveQuality::Basic:
            return "기존 호환에 가까운 낮은 보간 비용 설정입니다.";
        case TrailCurveQuality::Smooth:
            return "샘플 간격과 보간 수를 늘려 swing arc의 각짐을 줄입니다.";
        case TrailCurveQuality::HighQuality:
            return "더 촘촘한 샘플로 곡선을 만들지만 preview 비용이 늘 수 있습니다.";
        case TrailCurveQuality::Custom:
            return "아래 세부 곡선 값을 직접 편집합니다.";
        default:
            return "";
        }
    }

    TrailTypeData Build_TrailCurvePreset(TrailCurveQuality quality)
    {
        TrailTypeData preset{};

        switch (quality)
        {
        case TrailCurveQuality::Smooth:
            preset.sampleSpacing = 0.04f;
            preset.curveSubdivision = 8u;
            preset.smoothTangent = true;
            preset.sideFade = 0.12f;
            break;
        case TrailCurveQuality::HighQuality:
            preset.sampleSpacing = 0.02f;
            preset.curveSubdivision = 12u;
            preset.smoothTangent = true;
            preset.sideFade = 0.16f;
            break;
        case TrailCurveQuality::Basic:
        case TrailCurveQuality::Custom:
        default:
            preset.sampleSpacing = 0.08f;
            preset.curveSubdivision = 4u;
            preset.smoothTangent = true;
            preset.sideFade = 0.08f;
            break;
        }

        return preset;
    }

    bool Apply_TrailCurvePreset(TrailTypeData& data)
    {
        if (data.curveQuality == TrailCurveQuality::Custom)
            return false;

        const TrailTypeData preset = Build_TrailCurvePreset(data.curveQuality);
        bool changed = false;

        if (data.sampleSpacing != preset.sampleSpacing)
        {
            data.sampleSpacing = preset.sampleSpacing;
            changed = true;
        }
        if (data.curveSubdivision != preset.curveSubdivision)
        {
            data.curveSubdivision = preset.curveSubdivision;
            changed = true;
        }
        if (data.smoothTangent != preset.smoothTangent)
        {
            data.smoothTangent = preset.smoothTangent;
            changed = true;
        }
        if (data.sideFade != preset.sideFade)
        {
            data.sideFade = preset.sideFade;
            changed = true;
        }

        return changed;
    }

    bool Draw_TrailCurveQualityProperty(DetailPropertyContext& detailContext, TrailTypeData& data, const TrailTypeData& defaultData)
    {
        bool changed = false;

        ImGui::PushID("TrailCurveQuality");
        detailContext.Draw_PropertyLabel("곡선 품질", "트레일 곡선 샘플 밀도와 가장자리 페이드 preset입니다.");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_TrailCurveQualityLabel(data.curveQuality)))
        {
            for (TrailCurveQuality value : kTrailCurveQualityValues)
            {
                const bool selected = data.curveQuality == value;
                if (ImGui::Selectable(Get_TrailCurveQualityLabel(value), selected))
                {
                    data.curveQuality = value;
                    changed = true;
                }

                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                    ImGui::SetTooltip("%s", Get_TrailCurveQualityTooltip(value));

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(data.curveQuality != defaultData.curveQuality))
        {
            data.curveQuality = defaultData.curveQuality;
            changed = true;
        }
        ImGui::PopID();

        if (changed)
            changed |= Apply_TrailCurvePreset(data);

        return changed;
    }
}

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
        emitterView->MarkDirty();
        MarkDirty();
    }

    Render_TrailPreviewDetail();
    Commit_PendingAuthoringEditIfIdle(emitterView);
}

void Detail_View::Render_TrailPreviewDetail()
{
    if (EDITOR == nullptr)
        return;

    if (!ImGui::CollapsingHeader("트레일 프리뷰", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    Vec3 baseLocalOffset = EDITOR->Get_TrailPreviewBaseLocalOffset();
    Vec3 tipLocalOffset = EDITOR->Get_TrailPreviewTipLocalOffset();
    const Vec3 defaultBaseLocalOffset = EDITOR->Get_DefaultTrailPreviewBaseLocalOffset();
    const Vec3 defaultTipLocalOffset = EDITOR->Get_DefaultTrailPreviewTipLocalOffset();
    bool visible = EDITOR->Is_TrailPreviewVisible();
    bool debugRenderEnabled = EDITOR->Is_TrailPreviewDebugRenderEnabled();

    bool changed = false;
    if (_detailPropertyContext.Begin_PropertyTable("TrailPreview"))
    {
        if (_detailPropertyContext.Draw_BoolProperty("트레일 프리뷰", visible, false))
            EDITOR->Set_TrailPreviewVisible(visible);
        if (_detailPropertyContext.Draw_BoolProperty("디버그 렌더", debugRenderEnabled, true))
            EDITOR->Set_TrailPreviewDebugRenderEnabled(debugRenderEnabled);
        changed |= _detailPropertyContext.Draw_Vec3Property("Base 로컬", baseLocalOffset, defaultBaseLocalOffset, 0.005f);
        changed |= _detailPropertyContext.Draw_Vec3Property("Tip 로컬", tipLocalOffset, defaultTipLocalOffset, 0.005f);
        _detailPropertyContext.End_PropertyTable();
    }

    if (changed)
        EDITOR->Set_TrailPreviewLocalOffsets(baseLocalOffset, tipLocalOffset);
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
    bool changed = false;

    if (_detailPropertyContext.Begin_PropertyTable("Emitter"))
    {
        changed |= _detailPropertyContext.Draw_StringProperty("이미터 이름", emitter.name, defaultData.name);

        ImGui::PushID("RendererType");
        _detailPropertyContext.Draw_PropertyLabel("렌더러 타입");
        ImGui::TextDisabled("%s", emitter.rendererType.c_str());
        _detailPropertyContext.Draw_ResetButton(false);
        ImGui::PopID();

        _detailPropertyContext.End_PropertyTable();
    }

    if (changed)
        Begin_PendingAuthoringEdit(emitterView, selection, beforeSnapshot, "Edit Emitter Detail");

    Mark_Changed(emitterView, emitter, changed);
    Commit_PendingAuthoringEditIfIdle(emitterView);
}

void Detail_View::Render_TypeDataDetail(
    const Shared<Emitter_View>& emitterView,
    const EffectAuthoringSelection& selection,
    AuthoringEmitter& emitter)
{
    ImGui::TextUnformatted("타입 데이터");
    ImGui::Separator();

    bool changed = false;
    const Emitter_View::AuthoringSnapshot beforeSnapshot = emitterView->Capture_AuthoringSnapshot();
    switch (emitter.typeData.kind)
    {
    case AuthoringTypeDataKind::Trail:
    {
        TrailTypeData* data = get_if<TrailTypeData>(&emitter.typeData.payload);
        if (data == nullptr)
        {
            ImGui::TextDisabled("Trail TypeData payload is invalid.");
            break;
        }

        const TrailTypeData defaultData{};
        if (ImGui::CollapsingHeader("기본", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (_detailPropertyContext.Begin_PropertyTable("TrailTypeDataBase"))
            {
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "너비",
                    data->width,
                    defaultData.width,
                    0.01f,
                    "1.0이면 preview/sample provider가 넘긴 Base부터 Tip까지의 길이를 그대로 사용합니다. 낮추면 중심 기준으로 폭이 줄어듭니다."
                );
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "세그먼트 수명",
                    data->segmentLifetime,
                    defaultData.segmentLifetime,
                    0.01f,
                    "trail sample이 유지되는 시간입니다. 길수록 잔상이 오래 남습니다."
                );
                changed |= _detailPropertyContext.Draw_UintProperty(
                    "히스토리 수",
                    data->historyCount,
                    defaultData.historyCount,
                    "저장할 trail sample 개수입니다. 많을수록 긴 궤적을 유지할 수 있습니다."
                );
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "UV 타일링",
                    data->uvTiling,
                    defaultData.uvTiling,
                    0.01f,
                    "trail 길이 방향 texture 반복 배율입니다."
                );
                _detailPropertyContext.End_PropertyTable();
            }
        }

        if (ImGui::CollapsingHeader("곡선 품질", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (_detailPropertyContext.Begin_PropertyTable("TrailTypeDataCurveQuality"))
            {
                changed |= Draw_TrailCurveQualityProperty(_detailPropertyContext, *data, defaultData);
                changed |= Apply_TrailCurvePreset(*data);

                const bool presetLocked = data->curveQuality != TrailCurveQuality::Custom;
                if (presetLocked)
                    ImGui::BeginDisabled();

                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "샘플 간격",
                    data->sampleSpacing,
                    defaultData.sampleSpacing,
                    0.005f,
                    "이동 거리 기준으로 중간 trail sample을 삽입하는 간격입니다. 낮을수록 곡선이 촘촘해집니다."
                );
                changed |= _detailPropertyContext.Draw_UintProperty(
                    "곡선 분할",
                    data->curveSubdivision,
                    defaultData.curveSubdivision,
                    "한 frame 안에서 보강할 수 있는 최대 중간 sample 수입니다."
                );
                changed |= _detailPropertyContext.Draw_BoolProperty(
                    "탄젠트 스무딩",
                    data->smoothTangent,
                    defaultData.smoothTangent,
                    "앞뒤 sample을 함께 보아 base/tip 위치를 완만하게 평균 보정합니다."
                );
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "폭 가장자리 페이드",
                    data->sideFade,
                    defaultData.sideFade,
                    0.005f,
                    "trail 폭 방향 양끝 alpha를 줄이는 강도입니다. 높일수록 두꺼운 벽 느낌이 줄어듭니다."
                );

                if (presetLocked)
                    ImGui::EndDisabled();

                _detailPropertyContext.End_PropertyTable();
            }
        }
        break;
    }
    case AuthoringTypeDataKind::None:
    default:
        ImGui::TextDisabled("장착된 타입 데이터가 없습니다.");
        break;
    }

    if (changed)
        Begin_PendingAuthoringEdit(emitterView, selection, beforeSnapshot, "Edit TypeData Detail");

    Mark_Changed(emitterView, emitter, changed);
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

    Mark_Changed(emitterView, emitter, changed);
    Commit_PendingAuthoringEditIfIdle(emitterView);
}

void Detail_View::Mark_Changed(const Shared<Emitter_View>& emitterView, AuthoringEmitter& emitter, bool changed)
{
    if (!changed)
        return;

    emitter.previewDirty = true;
    if (emitterView)
        emitterView->MarkDirty();
    MarkDirty();
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

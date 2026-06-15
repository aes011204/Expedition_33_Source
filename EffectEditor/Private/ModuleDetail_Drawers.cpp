#include "ModuleDetail_Drawers.h"

#include "EffectEditorInstance.h"
#include "EffectEditor_Macro.h"
#include "EffectMaterialInstance_View.h"
#include "EffectMaterialPreviewRenderer.h"
#include "EffectMaterialScalarModulationPreview.h"
#include "EffectMaterial_View.h"
#include "EffectMaterialPresetReader.h"
#include "Helper_EffectAuthoring.h"
#include "Notification_Manager.h"

NS_BEGIN(EffectEditor)

namespace Internal
{
    constexpr auto kEffectMaterialPayloadType = "CONTENT_BROWSER_EFFECT_MATERIAL";
    constexpr auto kMaterialDialogDefaultFolder = L"../../../Client/Bin/Resources/Effects/Materials";
    constexpr float kMaterialFloatEpsilon = 1e-4f;

    bool Uses_DirectionalAlignmentOptions(EmitterScreenAlignment alignment)
    {
        return alignment == EmitterScreenAlignment::AwayFromCenter ||
               alignment == EmitterScreenAlignment::Velocity;
    }

    bool Uses_SourceHistorySpriteTrailCardAxis(EmitterScreenAlignment alignment, EmitterDirectionalAlignmentMode mode)
    {
        return alignment == EmitterScreenAlignment::FacingCameraPosition ||
               alignment == EmitterScreenAlignment::Rectangle ||
               (alignment == EmitterScreenAlignment::Velocity && mode == EmitterDirectionalAlignmentMode::TextureAxis);
    }

    void Notify_DirectionalAlignmentModeChanged(EmitterScreenAlignment alignment, EmitterDirectionalAlignmentMode mode)
    {
        if (!Uses_DirectionalAlignmentOptions(alignment))
            return;

        const char* directionName = alignment == EmitterScreenAlignment::AwayFromCenter ? "Away from Center" : "Velocity";
        const char* modeHelp = mode == EmitterDirectionalAlignmentMode::LookDirection
                               ? "기준 방향을 sprite 카드의 바라보는 방향으로 사용합니다."
                               : "texture 축을 기준 방향에 맞추고 나머지 축은 카메라 가시성을 유지합니다.";

        NOTIFY("{}: {}", directionName, modeHelp);
    }

    void Notify_TextureAxisChanged(EmitterSpriteTextureAxis axis)
    {
        NOTIFY(
            "{}",
            axis == EmitterSpriteTextureAxis::X
            ? "Texture X: 정렬 방향에 texture의 가로축을 맞춥니다."
            : "Texture Y: 정렬 방향에 texture의 세로축을 맞춥니다."
        );
    }

    fs::path Normalize_MaterialDialogPath(const fs::path& filePath)
    {
        try
        {
            if (fs::exists(filePath))
                return fs::weakly_canonical(filePath);
        }
        catch (...)
        {}

        try
        {
            return fs::absolute(filePath).lexically_normal();
        }
        catch (...)
        {}

        return filePath.lexically_normal();
    }

    static bool NearlyEqual(float lhs, float rhs)
    {
        return abs(lhs - rhs) <= kMaterialFloatEpsilon;
    }

    static bool Is_SameColor(const Color& lhs, const Color& rhs)
    {
        return
            NearlyEqual(lhs.x, rhs.x) &&
            NearlyEqual(lhs.y, rhs.y) &&
            NearlyEqual(lhs.z, rhs.z) &&
            NearlyEqual(lhs.w, rhs.w);
    }

    static bool Is_SameAdditiveContribution(
        const EffectMaterialAdditiveContributionData& lhs,
        const EffectMaterialAdditiveContributionData& rhs)
    {
        return
            lhs.colorSource == rhs.colorSource &&
            lhs.amountSource == rhs.amountSource &&
            lhs.coveragePolicy == rhs.coveragePolicy &&
            NearlyEqual(lhs.intensityScale, rhs.intensityScale) &&
            lhs.blackNeutral == rhs.blackNeutral &&
            Is_SameColor(lhs.emissiveColor, rhs.emissiveColor) &&
            Is_SameColor(lhs.constantColor, rhs.constantColor);
    }

    static bool Is_SameVec2(const Vec2& lhs, const Vec2& rhs)
    {
        return NearlyEqual(lhs.x, rhs.x) && NearlyEqual(lhs.y, rhs.y);
    }

    static const char* Get_SubUVFramePlaybackModeLabel(SubUVFramePlaybackMode mode)
    {
        switch (mode)
        {
        case SubUVFramePlaybackMode::FixedFrame:
            return "고정 프레임";
        case SubUVFramePlaybackMode::LifeProgress:
            return "수명 맞춤 재생";
        case SubUVFramePlaybackMode::FramesPerSecond:
            return "초당 프레임 재생";
        case SubUVFramePlaybackMode::RandomFrame:
            return "랜덤 프레임";
        default:
            return "고정 프레임";
        }
    }

    static const char* Get_DistributionRandomSeedModeLabel(DistributionRandomSeedMode mode)
    {
        switch (mode)
        {
        case DistributionRandomSeedMode::Default:
            return "기본값";
        case DistributionRandomSeedMode::Manual:
            return "수동";
        default:
            return "기본값";
        }
    }

    static uint32 Generate_RandomSeedValue()
    {
        static uint32 state{ 0xA341316Cu };
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state != 0u ? state : 0x6D2B79F5u;
    }

    static bool Draw_RandomSeedProperty(
        DetailPropertyContext& detailContext,
        DistributionRandomSeedData& data)
    {
        bool changed = false;

        if (!detailContext.Begin_PropertyTable("DistributionRandomSeed"))
            return false;

        ImGui::PushID("RandomSeedMode");
        detailContext.Draw_PropertyLabel("시드 모드", "같은 시드는 같은 랜덤 분포를 재현합니다.");
        DistributionRandomSeedMode nextMode = data.mode;
        if (ImGui::BeginCombo("##Value", Get_DistributionRandomSeedModeLabel(nextMode)))
        {
            const DistributionRandomSeedMode candidates[] = {
                DistributionRandomSeedMode::Default,
                DistributionRandomSeedMode::Manual,
            };

            for (const DistributionRandomSeedMode candidate : candidates)
            {
                const bool selected = nextMode == candidate;
                if (ImGui::Selectable(Get_DistributionRandomSeedModeLabel(candidate), selected))
                    nextMode = candidate;
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (data.mode != nextMode)
        {
            data.mode = nextMode;
            changed = true;
        }
        ImGui::PopID();

        ImGui::PushID("ManualSeed");
        detailContext.Draw_PropertyLabel("시드");
        const float randomizeButtonWidth = ImGui::CalcTextSize("무작위").x + ImGui::GetStyle().FramePadding.x * 2.f;
        const float seedInputWidth = max(1.f, ImGui::GetContentRegionAvail().x - randomizeButtonWidth - ImGui::GetStyle().ItemSpacing.x);
        ImGui::SetNextItemWidth(seedInputWidth);
        const bool manualMode = data.mode == DistributionRandomSeedMode::Manual;
        if (!manualMode)
            ImGui::BeginDisabled();
        changed |= ImGui::InputScalar("##Value", ImGuiDataType_U32, &data.manualSeed);
        if (!manualMode)
            ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("무작위"))
        {
            data.manualSeed = Generate_RandomSeedValue();
            data.mode = DistributionRandomSeedMode::Manual;
            changed = true;
        }
        ImGui::PopID();

        ImGui::PushID("UseInstanceSeed");
        detailContext.Draw_PropertyLabel(
            "재생마다 변주",
            "켜면 이 이펙트를 재생할 때마다 생성되는 seed를 이 랜덤값 샘플에 섞습니다."
        );
        changed |= ImGui::Checkbox("##Value", &data.useInstanceSeed);
        ImGui::PopID();

        detailContext.End_PropertyTable();
        return changed;
    }

    static const char* Get_SphereLocationSpawnModeLabel(SphereLocationSpawnMode mode)
    {
        switch (mode)
        {
        case SphereLocationSpawnMode::Volume:
            return "부피";
        case SphereLocationSpawnMode::Surface:
            return "표면";
        default:
            return "부피";
        }
    }

    static const char* Get_SphereLocationPlacementModeLabel(SphereLocationPlacementMode mode)
    {
        switch (mode)
        {
        case SphereLocationPlacementMode::Random:
            return "랜덤 샘플";
        case SphereLocationPlacementMode::EvenByParticleIndex:
            return "인덱스 균등";
        default:
            return "랜덤 샘플";
        }
    }

    static const char* Get_PlaneRadialLocationPlaneLabel(PlaneRadialLocationPlane plane)
    {
        switch (plane)
        {
        case PlaneRadialLocationPlane::XY:
            return "XY";
        case PlaneRadialLocationPlane::XZ:
            return "XZ";
        case PlaneRadialLocationPlane::YZ:
            return "YZ";
        case PlaneRadialLocationPlane::CameraFacing:
            return "카메라 정면";
        default:
            return "XY";
        }
    }

    static const char* Get_PlaneRadialLocationShapeLabel(PlaneRadialLocationShape shape)
    {
        switch (shape)
        {
        case PlaneRadialLocationShape::Rectangle:
            return "사각";
        case PlaneRadialLocationShape::Disc:
            return "원반";
        case PlaneRadialLocationShape::Ring:
            return "링";
        case PlaneRadialLocationShape::Arc:
            return "호";
        default:
            return "원반";
        }
    }

    static const char* Get_PlaneRadialLocationPlacementModeLabel(PlaneRadialLocationPlacementMode mode)
    {
        switch (mode)
        {
        case PlaneRadialLocationPlacementMode::Random:
            return "랜덤 샘플";
        case PlaneRadialLocationPlacementMode::EvenByParticleIndex:
            return "인덱스 균등";
        default:
            return "랜덤 샘플";
        }
    }

    static const char* Get_InitialRadialVelocityCenterDirectionModeLabel(InitialRadialVelocityCenterDirectionMode mode)
    {
        switch (mode)
        {
        case InitialRadialVelocityCenterDirectionMode::PlaneRadial:
            return "평면 방사";
        case InitialRadialVelocityCenterDirectionMode::RandomUpward:
        default:
            return "랜덤 상승";
        }
    }

    static const char* Get_PlaneRadialOrientationTargetKindLabel(PlaneRadialOrientationTargetKind targetKind)
    {
        switch (targetKind)
        {
        case PlaneRadialOrientationTargetKind::Auto:
            return "자동";
        case PlaneRadialOrientationTargetKind::Sprite2D:
            return "스프라이트";
        case PlaneRadialOrientationTargetKind::Mesh3D:
            return "메시";
        default:
            return "자동";
        }
    }

    static PlaneRadialOrientationTargetKind Resolve_PlaneRadialOrientationTargetKind(const AuthoringEmitter& emitter)
    {
        return emitter.typeData.kind == AuthoringTypeDataKind::Mesh
               ? PlaneRadialOrientationTargetKind::Mesh3D
               : PlaneRadialOrientationTargetKind::Sprite2D;
    }

    static bool Is_MeshEffectAuthoring(const AuthoringEmitter& emitter)
    {
        return emitter.typeData.kind == AuthoringTypeDataKind::Mesh;
    }

    static bool Is_PlaneRadialLocationAdvancedPlacementShape(PlaneRadialLocationShape shape)
    {
        return shape != PlaneRadialLocationShape::Rectangle;
    }

    static const char* Get_PlaneRadialOrientationModeLabel(PlaneRadialOrientationMode mode)
    {
        switch (mode)
        {
        case PlaneRadialOrientationMode::None:
            return "없음";
        case PlaneRadialOrientationMode::FaceRadialOut:
            return "바깥쪽";
        case PlaneRadialOrientationMode::FaceRadialIn:
            return "안쪽";
        case PlaneRadialOrientationMode::FaceTangentCW:
            return "접선 CW";
        case PlaneRadialOrientationMode::FaceTangentCCW:
            return "접선 CCW";
        case PlaneRadialOrientationMode::FacePlaneNormal:
            return "평면 normal";
        default:
            return "없음";
        }
    }

    static const char* Get_SphereRadialOrientationModeLabel(SphereRadialOrientationMode mode)
    {
        switch (mode)
        {
        case SphereRadialOrientationMode::None:
            return "없음";
        case SphereRadialOrientationMode::FaceRadialOut:
            return "바깥쪽";
        case SphereRadialOrientationMode::FaceRadialIn:
            return "안쪽";
        default:
            return "없음";
        }
    }

    static const char* Get_EffectOrbitPlaneLabel(EffectOrbitPlane plane)
    {
        switch (plane)
        {
        case EffectOrbitPlane::XY:
            return "XY";
        case EffectOrbitPlane::XZ:
            return "XZ";
        case EffectOrbitPlane::YZ:
            return "YZ";
        default:
            return "XY";
        }
    }

    static const char* Get_PlaneRadialOrientationAxisLabel(PlaneRadialOrientationAxis axis)
    {
        switch (axis)
        {
        case PlaneRadialOrientationAxis::PositiveX:
            return "+X";
        case PlaneRadialOrientationAxis::NegativeX:
            return "-X";
        case PlaneRadialOrientationAxis::PositiveY:
            return "+Y";
        case PlaneRadialOrientationAxis::NegativeY:
            return "-Y";
        case PlaneRadialOrientationAxis::PositiveZ:
            return "+Z";
        case PlaneRadialOrientationAxis::NegativeZ:
            return "-Z";
        default:
            return "+Z";
        }
    }

    static const char* Get_MaterialScalarTargetLabel(MaterialScalarModulationTargetField target)
    {
        switch (target)
        {
        case MaterialScalarModulationTargetField::Intensity:
            return "Intensity";
        case MaterialScalarModulationTargetField::OpacityPower:
            return "OpacityPower";
        case MaterialScalarModulationTargetField::NoiseStrength:
            return "NoiseStrength";
        case MaterialScalarModulationTargetField::AlphaErosion:
            return "AlphaErosion";
        case MaterialScalarModulationTargetField::AlphaCutoff:
            return "AlphaCutoff";
        case MaterialScalarModulationTargetField::CoreIntensity:
            return "CoreIntensity";
        case MaterialScalarModulationTargetField::OuterIntensity:
            return "OuterIntensity";
        case MaterialScalarModulationTargetField::CoreColor:
            return "CoreColor Multiplier";
        default:
            return "Intensity";
        }
    }

    static const char* Get_MaterialModulationTargetLabel(bool isCoreColorRgb, MaterialScalarModulationTargetField target)
    {
        return isCoreColorRgb ? "Core Color RGB" : Get_MaterialScalarTargetLabel(target);
    }

    static const char* Get_MaterialScalarOperationLabel(MaterialScalarModulationOperation operation)
    {
        switch (operation)
        {
        case MaterialScalarModulationOperation::Multiply:
            return "Multiply";
        default:
            return "Multiply";
        }
    }

    static const char* Get_MaterialScalarTimeSourceLabel(MaterialScalarModulationTimeSource timeSource)
    {
        switch (timeSource)
        {
        case MaterialScalarModulationTimeSource::ParticleLife:
            return "ParticleLife";
        case MaterialScalarModulationTimeSource::EmitterTime:
            return "EmitterTime";
        default:
            return "EmitterTime";
        }
    }

    static bool Is_SpriteMaterialScalarParticleLifeSupported(const AuthoringEmitter& emitter)
    {
        return emitter.typeData.kind == AuthoringTypeDataKind::None && emitter.rendererType == "Sprite";
    }

    static bool Is_MaterialScalarTimeSourceSelectable(
        MaterialScalarModulationTimeSource timeSource,
        bool allowParticleLife)
    {
        return timeSource != MaterialScalarModulationTimeSource::ParticleLife || allowParticleLife;
    }

    static bool Draw_MaterialModulationTargetProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        bool isCoreColorRgb,
        MaterialScalarModulationTargetField scalarTarget,
        bool& outNextCoreColorRgb,
        MaterialScalarModulationTargetField& outNextScalarTarget)
    {
        bool changed = false;
        outNextCoreColorRgb = isCoreColorRgb;
        outNextScalarTarget = scalarTarget;

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_MaterialModulationTargetLabel(isCoreColorRgb, scalarTarget)))
        {
            const MaterialScalarModulationTargetField candidates[] = {
                MaterialScalarModulationTargetField::Intensity,
                MaterialScalarModulationTargetField::OpacityPower,
                MaterialScalarModulationTargetField::NoiseStrength,
                MaterialScalarModulationTargetField::AlphaErosion,
                MaterialScalarModulationTargetField::AlphaCutoff,
                MaterialScalarModulationTargetField::CoreIntensity,
                MaterialScalarModulationTargetField::OuterIntensity,
                MaterialScalarModulationTargetField::CoreColor,
            };

            for (const MaterialScalarModulationTargetField candidate : candidates)
            {
                const bool isSelected = !isCoreColorRgb && scalarTarget == candidate;
                if (ImGui::Selectable(Get_MaterialScalarTargetLabel(candidate), isSelected))
                {
                    outNextCoreColorRgb = false;
                    outNextScalarTarget = candidate;
                    changed = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            const bool isRgbSelected = isCoreColorRgb;
            if (ImGui::Selectable("Core Color RGB", isRgbSelected))
            {
                outNextCoreColorRgb = true;
                outNextScalarTarget = MaterialScalarModulationTargetField::CoreColor;
                changed = true;
            }

            if (isRgbSelected)
                ImGui::SetItemDefaultFocus();

            ImGui::EndCombo();
        }

        const bool canReset = isCoreColorRgb || scalarTarget != MaterialScalarModulationTargetField::Intensity;
        if (detailContext.Draw_ResetButton(canReset))
        {
            outNextCoreColorRgb = false;
            outNextScalarTarget = MaterialScalarModulationTargetField::Intensity;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    static bool Draw_MaterialScalarTimeSourceProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        MaterialScalarModulationTimeSource& value,
        MaterialScalarModulationTimeSource defaultValue,
        bool allowParticleLife)
    {
        bool changed = false;

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_MaterialScalarTimeSourceLabel(value)))
        {
            const MaterialScalarModulationTimeSource candidates[] = {
                MaterialScalarModulationTimeSource::ParticleLife,
                MaterialScalarModulationTimeSource::EmitterTime,
            };

            for (const MaterialScalarModulationTimeSource candidate : candidates)
            {
                const bool isSelected = value == candidate;
                const bool selectable = Is_MaterialScalarTimeSourceSelectable(candidate, allowParticleLife);
                if (!selectable)
                    ImGui::BeginDisabled();

                if (ImGui::Selectable(Get_MaterialScalarTimeSourceLabel(candidate), isSelected) && selectable)
                {
                    value = candidate;
                    changed = true;
                }

                if (!selectable)
                    ImGui::EndDisabled();

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }
        if (!allowParticleLife && value == MaterialScalarModulationTimeSource::ParticleLife)
            ImGui::TextDisabled("ParticleLife is not supported for this emitter type or row.");

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    static bool Draw_SphereLocationSpawnModeProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        SphereLocationSpawnMode& value,
        SphereLocationSpawnMode defaultValue)
    {
        bool changed = false;

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_SphereLocationSpawnModeLabel(value)))
        {
            const SphereLocationSpawnMode candidates[] = {
                SphereLocationSpawnMode::Volume,
                SphereLocationSpawnMode::Surface,
            };

            for (const SphereLocationSpawnMode candidate : candidates)
            {
                const bool isSelected = value == candidate;
                if (ImGui::Selectable(Get_SphereLocationSpawnModeLabel(candidate), isSelected))
                {
                    value = candidate;
                    changed = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    static bool Draw_SphereLocationPlacementModeProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        SphereLocationPlacementMode& value,
        SphereLocationPlacementMode defaultValue,
        bool enabled)
    {
        bool changed = false;

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (!enabled)
            ImGui::BeginDisabled();

        if (ImGui::BeginCombo("##Value", Get_SphereLocationPlacementModeLabel(value)))
        {
            const SphereLocationPlacementMode candidates[] = {
                SphereLocationPlacementMode::Random,
                SphereLocationPlacementMode::EvenByParticleIndex,
            };

            for (const SphereLocationPlacementMode candidate : candidates)
            {
                const bool isSelected = value == candidate;
                if (ImGui::Selectable(Get_SphereLocationPlacementModeLabel(candidate), isSelected))
                {
                    value = candidate;
                    changed = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (!enabled)
            ImGui::EndDisabled();

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    static bool Draw_PlaneRadialLocationPlaneProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        PlaneRadialLocationPlane& value,
        PlaneRadialLocationPlane defaultValue)
    {
        bool changed = false;

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_PlaneRadialLocationPlaneLabel(value)))
        {
            const PlaneRadialLocationPlane candidates[] = {
                PlaneRadialLocationPlane::XY,
                PlaneRadialLocationPlane::XZ,
                PlaneRadialLocationPlane::YZ,
                PlaneRadialLocationPlane::CameraFacing,
            };

            for (const PlaneRadialLocationPlane candidate : candidates)
            {
                const bool isSelected = value == candidate;
                if (ImGui::Selectable(Get_PlaneRadialLocationPlaneLabel(candidate), isSelected))
                {
                    value = candidate;
                    changed = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    static bool Draw_PlaneRadialLocationShapeProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        PlaneRadialLocationShape& value,
        PlaneRadialLocationShape defaultValue)
    {
        bool changed = false;

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_PlaneRadialLocationShapeLabel(value)))
        {
            const PlaneRadialLocationShape candidates[] = {
                PlaneRadialLocationShape::Rectangle,
                PlaneRadialLocationShape::Disc,
                PlaneRadialLocationShape::Ring,
                PlaneRadialLocationShape::Arc,
            };

            for (const PlaneRadialLocationShape candidate : candidates)
            {
                const bool isSelected = value == candidate;
                if (ImGui::Selectable(Get_PlaneRadialLocationShapeLabel(candidate), isSelected))
                {
                    value = candidate;
                    changed = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    static bool Draw_PlaneRadialLocationPlacementModeProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        PlaneRadialLocationPlacementMode& value,
        PlaneRadialLocationPlacementMode defaultValue,
        bool enabled)
    {
        bool changed = false;

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (!enabled)
            ImGui::BeginDisabled();

        if (ImGui::BeginCombo("##Value", Get_PlaneRadialLocationPlacementModeLabel(value)))
        {
            const PlaneRadialLocationPlacementMode candidates[] = {
                PlaneRadialLocationPlacementMode::Random,
                PlaneRadialLocationPlacementMode::EvenByParticleIndex,
            };

            for (const PlaneRadialLocationPlacementMode candidate : candidates)
            {
                const bool isSelected = value == candidate;
                if (ImGui::Selectable(Get_PlaneRadialLocationPlacementModeLabel(candidate), isSelected))
                {
                    value = candidate;
                    changed = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (!enabled)
            ImGui::EndDisabled();

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    static bool Draw_InitialRadialVelocityCenterDirectionModeProperty(
        DetailPropertyContext& detailContext,
        InitialRadialVelocityCenterDirectionMode& value,
        InitialRadialVelocityCenterDirectionMode defaultValue)
    {
        bool changed = false;
        ImGui::PushID("InitialRadialVelocityCenterDirectionMode");
        detailContext.Draw_PropertyLabel(
            "방향 없음 처리",
            "생성 위치와 방사 기준점이 같아 방사 방향을 계산할 수 없을 때 사용할 방향입니다."
        );

        if (ImGui::BeginCombo("##Value", Get_InitialRadialVelocityCenterDirectionModeLabel(value)))
        {
            const InitialRadialVelocityCenterDirectionMode candidates[] = {
                InitialRadialVelocityCenterDirectionMode::RandomUpward,
                InitialRadialVelocityCenterDirectionMode::PlaneRadial,
            };

            for (const InitialRadialVelocityCenterDirectionMode candidate : candidates)
            {
                const bool selected = value == candidate;
                if (ImGui::Selectable(Get_InitialRadialVelocityCenterDirectionModeLabel(candidate), selected))
                {
                    value = candidate;
                    changed = true;
                }

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    static bool Draw_PlaneRadialOrientationModeProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        PlaneRadialOrientationMode& value,
        PlaneRadialOrientationMode defaultValue)
    {
        bool changed = false;

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_PlaneRadialOrientationModeLabel(value)))
        {
            const PlaneRadialOrientationMode candidates[] = {
                PlaneRadialOrientationMode::None,
                PlaneRadialOrientationMode::FaceRadialOut,
                PlaneRadialOrientationMode::FaceRadialIn,
                PlaneRadialOrientationMode::FaceTangentCW,
                PlaneRadialOrientationMode::FaceTangentCCW,
                PlaneRadialOrientationMode::FacePlaneNormal,
            };

            for (const PlaneRadialOrientationMode candidate : candidates)
            {
                const bool isSelected = value == candidate;
                if (ImGui::Selectable(Get_PlaneRadialOrientationModeLabel(candidate), isSelected))
                {
                    value = candidate;
                    changed = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    static bool Draw_SphereRadialOrientationModeProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        SphereRadialOrientationMode& value,
        SphereRadialOrientationMode defaultValue)
    {
        bool changed = false;

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_SphereRadialOrientationModeLabel(value)))
        {
            const SphereRadialOrientationMode candidates[] = {
                SphereRadialOrientationMode::None,
                SphereRadialOrientationMode::FaceRadialOut,
                SphereRadialOrientationMode::FaceRadialIn,
            };

            for (const SphereRadialOrientationMode candidate : candidates)
            {
                const bool isSelected = value == candidate;
                if (ImGui::Selectable(Get_SphereRadialOrientationModeLabel(candidate), isSelected))
                {
                    value = candidate;
                    changed = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    static bool Draw_EffectOrbitPlaneProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        EffectOrbitPlane& value,
        EffectOrbitPlane defaultValue)
    {
        bool changed = false;

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_EffectOrbitPlaneLabel(value)))
        {
            const EffectOrbitPlane candidates[] = {
                EffectOrbitPlane::XY,
                EffectOrbitPlane::XZ,
                EffectOrbitPlane::YZ,
            };

            for (const EffectOrbitPlane candidate : candidates)
            {
                const bool isSelected = value == candidate;
                if (ImGui::Selectable(Get_EffectOrbitPlaneLabel(candidate), isSelected))
                {
                    value = candidate;
                    changed = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    static void Draw_ReadOnlyTextProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        const char* value,
        const char* tooltip = nullptr)
    {
        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label, tooltip);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(value);
        detailContext.Draw_ResetButton(false);
        ImGui::PopID();
    }

    static void Draw_PlaneRadialOrientationResolvedTargetProperty(
        DetailPropertyContext& detailContext,
        const AuthoringEmitter& emitter)
    {
        Draw_ReadOnlyTextProperty(
            detailContext,
            "대상",
            Get_PlaneRadialOrientationTargetKindLabel(Resolve_PlaneRadialOrientationTargetKind(emitter)),
            "현재 emitter 타입 기준으로 자동 결정됩니다. targetKind 저장값은 유지하지만 이번 단계에서는 편집 UI를 숨깁니다."
        );
    }

    static bool Draw_PlaneRadialOrientationAxisProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        PlaneRadialOrientationAxis& value,
        PlaneRadialOrientationAxis defaultValue)
    {
        bool changed = false;

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_PlaneRadialOrientationAxisLabel(value)))
        {
            const PlaneRadialOrientationAxis candidates[] = {
                PlaneRadialOrientationAxis::PositiveX,
                PlaneRadialOrientationAxis::NegativeX,
                PlaneRadialOrientationAxis::PositiveY,
                PlaneRadialOrientationAxis::NegativeY,
                PlaneRadialOrientationAxis::PositiveZ,
                PlaneRadialOrientationAxis::NegativeZ,
            };

            for (const PlaneRadialOrientationAxis candidate : candidates)
            {
                const bool isSelected = value == candidate;
                if (ImGui::Selectable(Get_PlaneRadialOrientationAxisLabel(candidate), isSelected))
                {
                    value = candidate;
                    changed = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    static bool Has_PlaneRadialLocationModule(const AuthoringEmitter& emitter)
    {
        for (const AuthoringModule& module : emitter.modules)
        {
            if (module.type == AuthoringModuleType::PlaneRadialLocation)
                return true;
        }

        return false;
    }

    static bool Has_SphereLocationModule(const AuthoringEmitter& emitter)
    {
        for (const AuthoringModule& module : emitter.modules)
        {
            if (module.type == AuthoringModuleType::SphereLocation)
                return true;
        }

        return false;
    }

    static bool Has_EnabledPlaneRadialOrientationModule(const AuthoringEmitter& emitter)
    {
        for (const AuthoringModule& module : emitter.modules)
        {
            if (module.type != AuthoringModuleType::PlaneRadialOrientation || !module.enabled)
                continue;

            const auto* data = get_if<PlaneRadialOrientationModuleData>(&module.data);
            if (data != nullptr && data->orientationMode != PlaneRadialOrientationMode::None)
                return true;
        }

        return false;
    }

    static bool Should_ShowPlaneRadialOrientationMeshFields(const AuthoringEmitter& emitter)
    {
        return Resolve_PlaneRadialOrientationTargetKind(emitter) == PlaneRadialOrientationTargetKind::Mesh3D;
    }

    static bool Draw_SubUVFramePlaybackModeProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        SubUVFramePlaybackMode& value,
        SubUVFramePlaybackMode defaultValue)
    {
        bool changed = false;

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_SubUVFramePlaybackModeLabel(value)))
        {
            const SubUVFramePlaybackMode candidates[] = {
                SubUVFramePlaybackMode::FixedFrame,
                SubUVFramePlaybackMode::LifeProgress,
                SubUVFramePlaybackMode::FramesPerSecond,
                SubUVFramePlaybackMode::RandomFrame,
            };

            for (const SubUVFramePlaybackMode candidate : candidates)
            {
                const bool isSelected = value == candidate;
                if (ImGui::Selectable(Get_SubUVFramePlaybackModeLabel(candidate), isSelected))
                {
                    value = candidate;
                    changed = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    static bool Normalize_SubUVFrameIndexDistribution(FloatDistributionData& data, uint32 lastFrame)
    {
        bool changed = false;
        const float minFrame = 0.f;
        const float maxFrame = static_cast<float>(lastFrame);

        if (data.mode == DistributionMode::Uniform)
        {
            data = FloatDistributionData::Make_ConstantCurve(0.f);
            return true;
        }

        if (auto* constant = get_if<ConstantFloatDistributionData>(&data.payload))
        {
            const float clampedValue = clamp(constant->value, minFrame, maxFrame);
            if (!NearlyEqual(constant->value, clampedValue))
            {
                constant->value = clampedValue;
                changed = true;
            }
        }
        else if (auto* curve = get_if<ConstantCurveFloatDistributionData>(&data.payload))
        {
            for (FloatCurveKeyData& key : curve->keys)
            {
                const float clampedValue = clamp(key.value, minFrame, maxFrame);
                if (!NearlyEqual(key.value, clampedValue))
                {
                    key.value = clampedValue;
                    changed = true;
                }
            }
        }

        return changed;
    }

    static bool Is_SameMaterial(const EffectMaterialInstanceData& lhs, const EffectMaterialInstanceData& rhs)
    {
        return
            lhs.mainTextureGuid == rhs.mainTextureGuid &&
            lhs.mainTexturePath == rhs.mainTexturePath &&
            lhs.noiseTextureGuid == rhs.noiseTextureGuid &&
            lhs.noiseTexturePath == rhs.noiseTexturePath &&
            lhs.maskTextureGuid == rhs.maskTextureGuid &&
            lhs.maskTexturePath == rhs.maskTexturePath &&
            Is_SameColor(lhs.tint, rhs.tint) &&
            NearlyEqual(lhs.intensity, rhs.intensity) &&
            NearlyEqual(lhs.opacityPower, rhs.opacityPower) &&
            NearlyEqual(lhs.noiseStrength, rhs.noiseStrength) &&
            NearlyEqual(lhs.alphaCutoff, rhs.alphaCutoff) &&
            NearlyEqual(lhs.alphaErosion, rhs.alphaErosion) &&
            lhs.noiseSource == rhs.noiseSource &&
            lhs.maskSource == rhs.maskSource &&
            lhs.noiseInvert == rhs.noiseInvert &&
            lhs.maskInvert == rhs.maskInvert &&
            Is_SameVec2(lhs.mainUVScale, rhs.mainUVScale) &&
            Is_SameVec2(lhs.mainUVOffset, rhs.mainUVOffset) &&
            Is_SameVec2(lhs.mainUVScrollSpeed, rhs.mainUVScrollSpeed) &&
            lhs.mainUVTilingMode == rhs.mainUVTilingMode &&
            lhs.mainUVPolicy.uPolicy == rhs.mainUVPolicy.uPolicy &&
            lhs.mainUVPolicy.vPolicy == rhs.mainUVPolicy.vPolicy &&
            Is_SameVec2(lhs.noiseUVScale, rhs.noiseUVScale) &&
            Is_SameVec2(lhs.noiseUVOffset, rhs.noiseUVOffset) &&
            Is_SameVec2(lhs.noiseUVScrollSpeed, rhs.noiseUVScrollSpeed) &&
            lhs.noiseUVTilingMode == rhs.noiseUVTilingMode &&
            lhs.noiseUVPolicy.uPolicy == rhs.noiseUVPolicy.uPolicy &&
            lhs.noiseUVPolicy.vPolicy == rhs.noiseUVPolicy.vPolicy &&
            Is_SameVec2(lhs.maskUVScale, rhs.maskUVScale) &&
            Is_SameVec2(lhs.maskUVOffset, rhs.maskUVOffset) &&
            Is_SameVec2(lhs.maskUVScrollSpeed, rhs.maskUVScrollSpeed) &&
            lhs.maskUVTilingMode == rhs.maskUVTilingMode &&
            lhs.maskUVPolicy.uPolicy == rhs.maskUVPolicy.uPolicy &&
            lhs.maskUVPolicy.vPolicy == rhs.maskUVPolicy.vPolicy &&
            Is_SameVec2(lhs.flowUVScale, rhs.flowUVScale) &&
            Is_SameVec2(lhs.flowUVOffset, rhs.flowUVOffset) &&
            Is_SameVec2(lhs.flowUVScrollSpeed, rhs.flowUVScrollSpeed) &&
            lhs.flowUVTilingMode == rhs.flowUVTilingMode &&
            lhs.flowUVPolicy.uPolicy == rhs.flowUVPolicy.uPolicy &&
            lhs.flowUVPolicy.vPolicy == rhs.flowUVPolicy.vPolicy &&
            lhs.blendMode == rhs.blendMode &&
            lhs.opacitySource == rhs.opacitySource &&
            Is_SameAdditiveContribution(lhs.additive, rhs.additive) &&
            lhs.coreEmissive.enabled == rhs.coreEmissive.enabled &&
            Is_SameColor(lhs.coreEmissive.coreColor, rhs.coreEmissive.coreColor) &&
            NearlyEqual(lhs.coreEmissive.corePower, rhs.coreEmissive.corePower) &&
            NearlyEqual(lhs.coreEmissive.coreIntensity, rhs.coreEmissive.coreIntensity) &&
            NearlyEqual(lhs.coreEmissive.outerPower, rhs.coreEmissive.outerPower) &&
            NearlyEqual(lhs.coreEmissive.outerIntensity, rhs.coreEmissive.outerIntensity) &&
            lhs.twoSided == rhs.twoSided &&
            lhs.subUVRows == rhs.subUVRows &&
            lhs.subUVCols == rhs.subUVCols;
    }

    static string Try_ResolvePresetNameFromMaterial(const EffectMaterialInstanceData& material)
    {
        const fs::path presetFolder = Normalize_MaterialDialogPath(kMaterialDialogDefaultFolder);
        if (!fs::exists(presetFolder) || !fs::is_directory(presetFolder))
            return {};

        for (const auto& entry : fs::directory_iterator(presetFolder))
        {
            if (!entry.is_regular_file())
                continue;

            const fs::path presetPath = entry.path();
            if (!String::ToLowerCopy(presetPath.generic_string()).ends_with(".effectmaterial.json"))
                continue;

            EffectMaterialInstanceData presetMaterial{};
            if (!EffectMaterialPresetReader::Read(presetPath, presetMaterial))
                continue;

            if (Is_SameMaterial(material, presetMaterial))
                return presetMaterial.sourcePresetName;
        }

        return {};
    }

    static wstring Try_ResolvePresetPathFromMaterial(const EffectMaterialInstanceData& material)
    {
        const fs::path presetFolder = Normalize_MaterialDialogPath(kMaterialDialogDefaultFolder);
        if (!fs::exists(presetFolder) || !fs::is_directory(presetFolder))
            return {};

        if (!material.sourcePresetName.empty())
        {
            const fs::path candidatePath =
                presetFolder / (String::ToWString(material.sourcePresetName) + L".effectmaterial.json");
            if (fs::exists(candidatePath) && fs::is_regular_file(candidatePath))
                return candidatePath.wstring();
        }

        for (const auto& entry : fs::directory_iterator(presetFolder))
        {
            if (!entry.is_regular_file())
                continue;

            const fs::path presetPath = entry.path();
            if (!String::ToLowerCopy(presetPath.generic_string()).ends_with(".effectmaterial.json"))
                continue;

            EffectMaterialInstanceData presetMaterial{};
            if (!EffectMaterialPresetReader::Read(presetPath, presetMaterial))
                continue;

            if (Is_SameMaterial(material, presetMaterial))
                return presetPath.wstring();
        }

        return {};
    }

    static const string& Resolve_MaterialDisplayName(RequiredModuleData& data)
    {
        if (data.material.sourcePresetName.empty())
            data.material.sourcePresetName = Try_ResolvePresetNameFromMaterial(data.material);

        return data.material.sourcePresetName;
    }

    bool Try_PickMaterialPresetDialog(wstring& outFilePath)
    {
        outFilePath.clear();

        const HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        const bool shouldUnInit = SUCCEEDED(initHr);

        IFileOpenDialog* dialog = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
        if (FAILED(hr) || dialog == nullptr)
        {
            if (shouldUnInit)
                CoUninitialize();
            return false;
        }

        dialog->SetTitle(L"Select Effect Material");

        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST);

        const fs::path defaultFolder = Normalize_MaterialDialogPath(kMaterialDialogDefaultFolder);
        if (!defaultFolder.empty())
        {
            IShellItem* folderItem = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(defaultFolder.wstring().c_str(), nullptr, IID_PPV_ARGS(&folderItem))) && folderItem != nullptr)
            {
                dialog->SetFolder(folderItem);
                folderItem->Release();
            }
        }

        const COMDLG_FILTERSPEC filters[] =
        {
            { L"Effect Materials", L"*.effectmaterial.json" },
            { L"All", L"*.*" },
        };

        dialog->SetFileTypes(std::size(filters), filters);

        hr = dialog->Show(nullptr);
        if (SUCCEEDED(hr))
        {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)) && item != nullptr)
            {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path != nullptr)
                {
                    outFilePath = path;
                    CoTaskMemFree(path);
                }
                item->Release();
            }
        }

        dialog->Release();

        if (shouldUnInit)
            CoUninitialize();

        return !outFilePath.empty();
    }

    Shared<EffectMaterial_View> Get_MaterialView()
    {
        if (nullptr == EDITOR)
            return nullptr;

        const Shared<Editor_Window> materialWindow = EDITOR->Get_Window(L"Effect Material");
        return dynamic_pointer_cast<EffectMaterial_View>(materialWindow);
    }

    void Open_MaterialInstanceWindow(uint32 emitterId, uint32 moduleId)
    {
        if (nullptr == EDITOR)
            return;

        const Shared<Editor_Window> materialInstanceWindow = EDITOR->Get_Window(L"Effect Material Instance");
        const Shared<EffectMaterialInstance_View> materialInstanceView =
            dynamic_pointer_cast<EffectMaterialInstance_View>(materialInstanceWindow);
        if (nullptr == materialInstanceView)
            return;

        materialInstanceView->Open_Instance(emitterId, moduleId);
    }

    void Open_PresetBrowserWindow(const RequiredModuleData& data)
    {
        const Shared<EffectMaterial_View> materialView = Get_MaterialView();
        if (nullptr == materialView)
            return;

        const wstring presetPath = Try_ResolvePresetPathFromMaterial(data.material);
        if (!presetPath.empty())
        {
            materialView->Open_Preset(presetPath);
            return;
        }

        materialView->Open_PresetBrowser();
    }

    bool Apply_PresetFileToRequired(const wchar_t* filePath, RequiredModuleData& data)
    {
        if (filePath == nullptr || filePath[0] == L'\0')
            return false;

        const Shared<EffectMaterial_View> materialView = Get_MaterialView();
        if (materialView == nullptr)
            return false;

        materialView->Open_Preset(filePath);
        if (!materialView->Has_SelectedPreset())
            return false;

        data.material = materialView->Get_SelectedPresetMaterial();
        return true;
    }

    bool Draw_MaterialDropSlot(
        const AuthoringEmitter& emitter,
        RequiredModuleData& data,
        EffectMaterialPreviewRenderer* materialSlotPreview,
        float timeDelta,
        bool allowAuthoring)
    {
        constexpr float browseButtonWidth{ 28.f };
        constexpr float slotHeight = 54.f;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const ImGuiPayload* activePayload = ImGui::GetDragDropPayload();
        const bool isMaterialDragging =
            allowAuthoring &&
            activePayload != nullptr &&
            activePayload->IsDataType(kEffectMaterialPayloadType);

        const ImVec2 slotMin = ImGui::GetCursorScreenPos();
        const float slotWidth = max(180.f, ImGui::GetContentRegionAvail().x - browseButtonWidth - spacing);
        if (!allowAuthoring)
            ImGui::BeginDisabled();
        ImGui::InvisibleButton("##MaterialDropSlot", ImVec2(slotWidth, slotHeight));
        if (!allowAuthoring)
            ImGui::EndDisabled();

        bool changed = false;
        if (allowAuthoring && ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
                kEffectMaterialPayloadType,
                ImGuiDragDropFlags_AcceptBeforeDelivery
            ))
            {
                if (payload->IsDelivery())
                    changed = Apply_PresetFileToRequired(static_cast<const wchar_t*>(payload->Data), data);
            }

            ImGui::EndDragDropTarget();
        }

        const bool isHovered = ImGui::IsItemHovered(
            allowAuthoring ? ImGuiHoveredFlags_None : ImGuiHoveredFlags_AllowWhenDisabled
        );
        const ImVec2 slotMax{ slotMin.x + slotWidth, slotMin.y + slotHeight };
        const ImU32 fillColor = isMaterialDragging
                                ? IM_COL32(42, 56, 78, 255)
                                : !allowAuthoring ? IM_COL32(26, 28, 34, 255)
                                  : IM_COL32(30, 34, 42, 255);
        const ImU32 borderColor = isMaterialDragging
                                  ? IM_COL32(96, 165, 250, 255)
                                  : !allowAuthoring ? IM_COL32(58, 64, 76, 255)
                                    : isHovered ? IM_COL32(120, 130, 150, 255) : IM_COL32(70, 78, 92, 255);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(slotMin, slotMax, fillColor, 6.f);
        drawList->AddRect(slotMin, slotMax, borderColor, 6.f, 0, isMaterialDragging ? 2.5f : 1.5f);

        ShaderResourceView* previewSrv = nullptr;
        if (materialSlotPreview != nullptr)
        {
            const bool allowScalarParticleLife = Is_SpriteMaterialScalarParticleLifeSupported(emitter);
            materialSlotPreview->Update_MaterialTime(timeDelta);
            if (const MaterialScalarModulationModuleData* modulation = Find_MaterialScalarModulationModuleData(emitter))
            {
                materialSlotPreview->Render_Material(
                    data.material,
                    Build_CoreColorRgbModulationRuntimeDesc(*modulation),
                    Build_MaterialScalarModulationRuntimeDesc(*modulation),
                    data.duration,
                    allowScalarParticleLife
                );
            }
            else
                materialSlotPreview->Render_Material(data.material);
            previewSrv = materialSlotPreview->Get_SRV();
        }
        const ImVec2 thumbnailMin{ slotMin.x + 7.f, slotMin.y + 7.f };
        const ImVec2 thumbnailMax{ thumbnailMin.x + 40.f, thumbnailMin.y + 40.f };
        if (previewSrv != nullptr)
        {
            const ImTextureID textureId = static_cast<ImTextureID>(reinterpret_cast<intptr_t>(previewSrv));
            drawList->AddImage(textureId, thumbnailMin, thumbnailMax);
        }
        else
        {
            drawList->AddRectFilled(thumbnailMin, thumbnailMax, IM_COL32(18, 20, 24, 255), 4.f);
            drawList->AddText(ImVec2(thumbnailMin.x + 8.f, thumbnailMin.y + 12.f), IM_COL32(150, 160, 176, 255), "MAT");
        }
        drawList->AddRect(thumbnailMin, thumbnailMax, IM_COL32(85, 96, 116, 255), 4.f);

        const string& resolvedName = Resolve_MaterialDisplayName(data);
        const string displayName = resolvedName.empty() ? "(drop material here)" : resolvedName;
        const string blendModeName = string(magic_enum::enum_name(data.material.blendMode));
        const string sourceSummary =
            data.material.blendMode == EffectMaterialBlendMode::Additive
            ? string(magic_enum::enum_name(data.material.additive.colorSource)) + " / " +
              string(magic_enum::enum_name(data.material.additive.amountSource))
            : data.material.opacitySource;
        const string materialSummary = blendModeName + " / " + sourceSummary +
                                       " / SubUV " + to_string(data.material.subUVCols) + " cols x " + to_string(data.material.subUVRows) + " rows";
        drawList->PushClipRect(
            ImVec2(slotMin.x + 57.f, slotMin.y),
            ImVec2(slotMax.x - 8.f, slotMax.y),
            true
        );
        drawList->AddText(ImVec2(slotMin.x + 57.f, slotMin.y + 9.f), IM_COL32(235, 238, 245, 255), displayName.c_str());
        drawList->AddText(
            ImGui::GetFont(),
            ImGui::GetFontSize() * 0.9f,
            ImVec2(slotMin.x + 57.f, slotMin.y + 29.f),
            IM_COL32(145, 154, 172, 255),
            materialSummary.c_str()
        );
        drawList->PopClipRect();

        if (isHovered)
        {
            if (!allowAuthoring)
                ImGui::SetTooltip("Mesh 머티리얼은 Mesh TypeData의 Assigned Material에서 편집합니다.");
            else
            {
                const string mainTooltip =
                    "GUID: " + (data.material.mainTextureGuid.empty() ? string{ "(none)" } : data.material.mainTextureGuid) +
                    "\nPath: " + (data.material.mainTexturePath.empty() ? string{ "(none)" } : data.material.mainTexturePath);
                const string noiseTooltip =
                    data.material.noiseTextureGuid.empty() && data.material.noiseTexturePath.empty()
                    ? string{ "GUID: (none)\nPath: (none)" }
                    : "GUID: " + (data.material.noiseTextureGuid.empty() ? string{ "(none)" } : data.material.noiseTextureGuid) +
                      "\nPath: " + (data.material.noiseTexturePath.empty() ? string{ "(none)" } : data.material.noiseTexturePath);
                ImGui::SetTooltip(
                    "Content Browser의 .effectmaterial.json 머티리얼 파일을 여기에 드롭하면 현재 embedded material instance로 복사합니다.\n"
                    "현재 sprite effect 경로에서는 Main Texture를 emissive color source로, Opacity Source를 별도 opacity 해석 기준으로 사용합니다.\n\n"
                    "Main Texture (Emissive)\n%s\n\nNoise Texture\n%s\n\n%s / %s / SubUV %u cols x %u rows",
                    mainTooltip.c_str(),
                    noiseTooltip.c_str(),
                    blendModeName.c_str(),
                    sourceSummary.c_str(),
                    data.material.subUVCols,
                    data.material.subUVRows
                );
            }
        }

        ImGui::SameLine();
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2{ 0.5f, 0.5f });
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 0.f, 0.f });
        if (!allowAuthoring)
            ImGui::BeginDisabled();
        if (ImGui::Button(ICON_FA_FOLDER_OPEN "##PickMaterial", ImVec2{ browseButtonWidth, slotHeight }))
        {
            wstring pickedFilePath{};
            if (Try_PickMaterialPresetDialog(pickedFilePath))
                changed = Apply_PresetFileToRequired(pickedFilePath.c_str(), data);
        }
        if (!allowAuthoring)
            ImGui::EndDisabled();
        ImGui::PopStyleVar(2);
        if (ImGui::IsItemHovered(allowAuthoring ? ImGuiHoveredFlags_DelayShort : ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::SetTooltip(
                allowAuthoring
                ? "머티리얼 원본 파일 선택"
                : "Mesh 머티리얼은 Mesh TypeData의 Assigned Material에서 편집합니다."
            );
        }

        return changed;
    }

    static uint32 Resolve_SubUvFrameCount(const AuthoringEmitter& emitter)
    {
        for (const AuthoringModule& module : emitter.modules)
        {
            if (module.type != AuthoringModuleType::Required)
                continue;

            const RequiredModuleData* requiredData = get_if<RequiredModuleData>(&module.data);
            if (requiredData == nullptr)
                return 1;

            const uint32 rows = max(1u, requiredData->material.subUVRows);
            const uint32 cols = max(1u, requiredData->material.subUVCols);
            return max(1u, rows * cols);
        }

        return 1;
    }

    static bool Is_SameVec3(const Vec3& lhs, const Vec3& rhs)
    {
        return
            NearlyEqual(lhs.x, rhs.x) &&
            NearlyEqual(lhs.y, rhs.y) &&
            NearlyEqual(lhs.z, rhs.z);
    }

    static Vec3 Resolve_CoreColorRgbDefault(const AuthoringEmitter& emitter)
    {
        for (const AuthoringModule& module : emitter.modules)
        {
            if (module.type != AuthoringModuleType::Required)
                continue;

            const RequiredModuleData* requiredData = get_if<RequiredModuleData>(&module.data);
            if (requiredData == nullptr)
                break;

            return Vec3{
                requiredData->material.coreEmissive.coreColor.x,
                requiredData->material.coreEmissive.coreColor.y,
                requiredData->material.coreEmissive.coreColor.z
            };
        }

        return Vec3{ 1.f, 0.85f, 0.45f };
    }

    static MaterialCoreColorRgbModulatorData Make_CoreColorRgbModulator(const AuthoringEmitter& emitter)
    {
        MaterialCoreColorRgbModulatorData data{};
        data.distribution = Vector3DistributionData::Make_Constant(Resolve_CoreColorRgbDefault(emitter));
        return data;
    }

    static bool Draw_RadialPivotProperty(
        DetailPropertyContext& detailContext,
        Vec3& value,
        const Vec3& defaultValue)
    {
        bool changed = false;

        ImGui::PushID("RadialPivot");
        detailContext.Draw_PropertyLabel("방사 기준점");
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        {
            ImGui::SetTooltip(
                "입자 생성 위치에서 이 기준점을 뺀 방향으로 초기 속도를 더합니다.\n"
                "기준점이 생성 위치보다 위에 있으면 +속도에서도 아래 방향으로 퍼질 수 있습니다."
            );
        }

        ImGui::SetNextItemWidth(-FLT_MIN);
        changed |= ImGui::DragFloat3("##Value", &value.x, 0.01f);
        if (detailContext.Draw_ResetButton(!Is_SameVec3(value, defaultValue)))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    static bool Draw_RadialWorldSpaceProperty(
        DetailPropertyContext& detailContext,
        bool& value,
        bool defaultValue)
    {
        bool changed = false;

        ImGui::PushID("RadialWorldSpace");
        detailContext.Draw_PropertyLabel("월드 스페이스");
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        {
            ImGui::SetTooltip(
                "켜면 방사 기준점을 world offset으로 사용합니다.\n"
                "끄면 emitter 회전 기준 local offset으로 변환합니다."
            );
        }

        changed |= ImGui::Checkbox("##Value", &value);
        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    static const char* Get_BlendModeLabel(EffectMaterialBlendMode value)
    {
        switch (value)
        {
        case EffectMaterialBlendMode::AlphaBlend:
            return "AlphaBlend";
        case EffectMaterialBlendMode::Additive:
            return "Additive";
        case EffectMaterialBlendMode::Masked:
            return "Masked";
        case EffectMaterialBlendMode::Modulate:
            return "Modulate";
        default:
            return "AlphaBlend";
        }
    }

    static const char* Get_SortPolicyLabel(EmitterSortPolicy value)
    {
        switch (value)
        {
        case EmitterSortPolicy::EmitterDepth:
            return "Emitter Depth";
        case EmitterSortPolicy::None:
        default:
            return "None";
        }
    }

    static bool Draw_BlendModeProperty(
        DetailPropertyContext& detailContext,
        const AuthoringEmitter& emitter,
        EffectMaterialBlendMode& value,
        EffectMaterialBlendMode defaultValue)
    {
        static constexpr EffectMaterialBlendMode kModes[]{
            EffectMaterialBlendMode::AlphaBlend,
            EffectMaterialBlendMode::Additive,
            EffectMaterialBlendMode::Masked,
        };

        const bool trailEmitter = emitter.typeData.kind == AuthoringTypeDataKind::Trail;
        bool changed = false;

        ImGui::PushID("RequiredBlendMode");
        detailContext.Draw_PropertyLabel("Blend Mode");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_BlendModeLabel(value)))
        {
            for (const EffectMaterialBlendMode mode : kModes)
            {
                const bool selectable = !(trailEmitter && mode == EffectMaterialBlendMode::Masked);
                const bool selected = value == mode;

                if (!selectable)
                    ImGui::BeginDisabled();

                if (ImGui::Selectable(Get_BlendModeLabel(mode), selected) && selectable)
                {
                    value = mode;
                    changed = true;
                }

                if (!selectable)
                    ImGui::EndDisabled();

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    static bool Draw_SortPolicyProperty(
        DetailPropertyContext& detailContext,
        EmitterSortPolicy& value,
        EmitterSortPolicy defaultValue)
    {
        static constexpr EmitterSortPolicy kPolicies[]{
            EmitterSortPolicy::None,
            EmitterSortPolicy::EmitterDepth,
        };

        bool changed = false;

        ImGui::PushID("RequiredSortPolicy");
        detailContext.Draw_PropertyLabel("Sort Policy");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_SortPolicyLabel(value)))
        {
            for (const EmitterSortPolicy policy : kPolicies)
            {
                const bool selected = value == policy;
                if (ImGui::Selectable(Get_SortPolicyLabel(policy), selected))
                {
                    value = policy;
                    changed = true;
                }

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    static bool Draw_IntProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        int32& value,
        int32 defaultValue)
    {
        bool changed = false;

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        int inputValue = value;
        if (ImGui::InputInt("##Value", &inputValue, 1, 10))
        {
            value = static_cast<int32>(inputValue);
            changed = true;
        }

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }
}

bool RequiredModuleDetail::Draw(
    AuthoringEmitter& emitter,
    const AuthoringModule& module,
    RequiredModuleData& data,
    DetailPropertyContext& detailContext,
    float timeDelta)
{
    const RequiredModuleData defaultData{};
    bool changed = false;
    const AuthoringModuleType moduleType = module.type;

    if (_materialSlotPreview == nullptr)
        _materialSlotPreview = EffectMaterialPreviewRenderer::Create(64);

    const bool allowRequiredMaterialAuthoring = !Internal::Is_MeshEffectAuthoring(emitter);

    if (ImGui::CollapsingHeader("머티리얼", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("RequiredMaterial"))
        {
            ImGui::PushID("MaterialDropSlot");
            detailContext.Draw_PropertyLabel("머티리얼");
            changed |= Internal::Draw_MaterialDropSlot(
                emitter,
                data,
                _materialSlotPreview.get(),
                timeDelta,
                allowRequiredMaterialAuthoring
            );
            detailContext.Draw_ResetButton(false);
            ImGui::PopID();

            ImGui::PushID("MaterialEdit");
            detailContext.Draw_PropertyLabel("작업");

            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float availableWidth = ImGui::GetContentRegionAvail().x;
            const float editButtonWidth = (availableWidth - spacing) * 0.5f;
            const float sourceButtonWidth = (availableWidth - spacing) * 0.5f;

            if (!allowRequiredMaterialAuthoring)
                ImGui::BeginDisabled();
            if (ImGui::Button("인스턴스 편집", ImVec2{ editButtonWidth, 24.f }))
                Internal::Open_MaterialInstanceWindow(emitter.id, module.id);
            if (!allowRequiredMaterialAuthoring)
                ImGui::EndDisabled();
            if (!allowRequiredMaterialAuthoring && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Mesh 머티리얼은 Mesh TypeData의 Assigned Material에서 편집합니다.");

            ImGui::SameLine();

            if (!allowRequiredMaterialAuthoring)
                ImGui::BeginDisabled();
            if (ImGui::Button("원본 보기", ImVec2{ sourceButtonWidth, 24.f }))
                Internal::Open_PresetBrowserWindow(data);
            if (!allowRequiredMaterialAuthoring)
                ImGui::EndDisabled();

            if (ImGui::IsItemHovered(allowRequiredMaterialAuthoring ? ImGuiHoveredFlags_DelayShort : ImGuiHoveredFlags_AllowWhenDisabled))
            {
                ImGui::SetTooltip(
                    allowRequiredMaterialAuthoring
                    ? "원본 머티리얼을 확인"
                    : "Mesh 머티리얼은 Mesh TypeData의 Assigned Material에서 편집합니다."
                );
            }
            ImGui::PopID();

            detailContext.End_PropertyTable();
        }
    }

    if (ImGui::CollapsingHeader("이미터 좌표", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("RequiredTransform"))
        {
            changed |= detailContext.Draw_Vec3Property("원점", data.emitterOrigin, defaultData.emitterOrigin);
            changed |= detailContext.Draw_Vec3Property("회전 (도)", data.emitterRotationDegrees, defaultData.emitterRotationDegrees);
            detailContext.End_PropertyTable();
        }
    }

    if (ImGui::CollapsingHeader("공간 / 정렬", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("RequiredSpaceSort"))
        {
            const bool isSourceHistorySpriteTrail = emitter.typeData.kind == AuthoringTypeDataKind::SourceHistorySpriteTrail;
            changed |= isSourceHistorySpriteTrail
                       ? detailContext.Draw_SourceHistorySpriteTrailScreenAlignmentProperty("화면 정렬", data.screenAlignment, defaultData.screenAlignment)
                       : detailContext.Draw_ScreenAlignmentProperty("화면 정렬", data.screenAlignment, defaultData.screenAlignment);
            if (Internal::Uses_DirectionalAlignmentOptions(data.screenAlignment))
            {
                const EmitterDirectionalAlignmentMode oldMode = data.directionalAlignmentMode;

                if (detailContext.Draw_DirectionalAlignmentModeProperty("방향 정렬 방식", data.directionalAlignmentMode, defaultData.directionalAlignmentMode))
                {
                    changed = true;
                    if (oldMode != data.directionalAlignmentMode)
                        Internal::Notify_DirectionalAlignmentModeChanged(data.screenAlignment, data.directionalAlignmentMode);
                }
            }

            const bool showSpriteTextureAxis =
                isSourceHistorySpriteTrail
                ? Internal::Uses_SourceHistorySpriteTrailCardAxis(data.screenAlignment, data.directionalAlignmentMode)
                : Internal::Uses_DirectionalAlignmentOptions(data.screenAlignment) &&
                  data.directionalAlignmentMode == EmitterDirectionalAlignmentMode::TextureAxis;
            if (showSpriteTextureAxis)
            {
                const EmitterSpriteTextureAxis oldAxis = data.spriteTextureAxis;
                if (isSourceHistorySpriteTrail)
                {
                    if (detailContext.Draw_SpriteTextureAxisProperty(
                        "카드 길이 축",
                        data.spriteTextureAxis,
                        defaultData.spriteTextureAxis,
                        "Texture X를 SourceHistorySpriteTrail 카드 길이 방향으로 사용합니다.",
                        "Texture Y를 SourceHistorySpriteTrail 카드 길이 방향으로 사용합니다."
                    ))
                    {
                        changed = true;
                        if (oldAxis != data.spriteTextureAxis)
                            Internal::Notify_TextureAxisChanged(data.spriteTextureAxis);
                    }
                }
                else if (detailContext.Draw_SpriteTextureAxisProperty("정렬할 텍스처 축", data.spriteTextureAxis, defaultData.spriteTextureAxis))
                {
                    changed = true;
                    if (oldAxis != data.spriteTextureAxis)
                        Internal::Notify_TextureAxisChanged(data.spriteTextureAxis);
                }

                changed |= detailContext.Draw_FloatProperty("롤 보정 (도)", data.spriteRollOffsetDegrees, defaultData.spriteRollOffsetDegrees, 0.1f);
            }

            changed |= detailContext.Draw_BoolProperty("로컬 스페이스 사용", data.useLocalSpace, defaultData.useLocalSpace);
            detailContext.End_PropertyTable();
        }

        if (Internal::Uses_DirectionalAlignmentOptions(data.screenAlignment))
        {
            if (data.directionalAlignmentMode == EmitterDirectionalAlignmentMode::TextureAxis)
                ImGui::TextDisabled("Texture Axis는 world X/Y/Z가 아니라 texture 가로/세로 축입니다. 나머지 축은 카메라 가시성을 유지합니다.");
            else
                ImGui::TextDisabled("Look Direction은 기준 방향을 sprite 카드의 바라보는 방향으로 사용합니다.");
        }

        detailContext.Draw_ScreenAlignmentImplementationNote(data.screenAlignment);
    }

    if (ImGui::CollapsingHeader("렌더링 / 정렬", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("RequiredRenderingSort"))
        {
            changed |= Internal::Draw_BlendModeProperty(detailContext, emitter, data.material.blendMode, defaultData.material.blendMode);
            changed |= Internal::Draw_SortPolicyProperty(detailContext, data.sortPolicy, defaultData.sortPolicy);
            changed |= Internal::Draw_IntProperty(detailContext, "Sort Layer", data.sortLayer, defaultData.sortLayer);
            detailContext.End_PropertyTable();
        }

        if (emitter.typeData.kind == AuthoringTypeDataKind::Trail)
            ImGui::TextDisabled("Trail은 v1에서 Masked 선택을 막고 AlphaBlend/Additive 경로를 유지합니다.");

        if (ImGui::TreeNodeEx("Advanced", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (detailContext.Begin_PropertyTable("RequiredRenderingSortAdvanced"))
            {
                changed |= detailContext.Draw_FloatProperty(
                    "Sort Bias",
                    data.sortBias,
                    defaultData.sortBias,
                    0.01f,
                    "같은 Sort Layer와 같은 Sort Policy 안에서만 마지막 보정값으로 사용합니다."
                );
                detailContext.End_PropertyTable();
            }
            ImGui::TreePop();
        }
    }

    if (ImGui::CollapsingHeader("이미터 재생 / 스폰 시간", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("RequiredTime"))
        {
            changed |= detailContext.Draw_UintProperty("루프 횟수", data.loopCount, defaultData.loopCount);
            changed |= detailContext.Draw_FloatProperty(
                "스폰 활성 시간",
                data.duration,
                defaultData.duration,
                0.01f,
                nullptr,
                Find_AuthoringValueRange(moduleType, "duration")
            );
            changed |= detailContext.Draw_FloatProperty(
                "스폰 시작 딜레이",
                data.delay,
                defaultData.delay,
                0.01f,
                nullptr,
                Find_AuthoringValueRange(moduleType, "delay")
            );
            changed |= detailContext.Draw_BoolProperty("첫 루프만 스폰 딜레이", data.delayFirstLoopOnly, defaultData.delayFirstLoopOnly);
            detailContext.End_PropertyTable();
        }

        ImGui::TextDisabled("스폰 활성 시간은 새 파티클을 만들 수 있는 emitter loop 구간입니다. 이미 태어난 파티클은 Particle Lifetime이 끝날 때까지 남을 수 있습니다.");
    }

    if (ImGui::CollapsingHeader("종료 / 킬", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("RequiredKill"))
        {
            changed |= detailContext.Draw_BoolProperty("비활성화 시 킬 (외부 비활성화 계약 보류)", data.killOnDeactivate, defaultData.killOnDeactivate);
            changed |= detailContext.Draw_BoolProperty("완료 시 킬", data.killOnCompleted, defaultData.killOnCompleted);
            detailContext.End_PropertyTable();
        }

        if (data.killOnCompleted)
            ImGui::TextDisabled("완료 시 active particle도 즉시 종료합니다.");
    }

    if (ImGui::CollapsingHeader("렌더링 제한", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("RequiredRenderLimit"))
        {
            changed |= detailContext.Draw_BoolProperty("최대 드로우 수 사용", data.useMaxDrawCount, defaultData.useMaxDrawCount);
            changed |= detailContext.Draw_UintProperty("최대 드로우 수", data.maxDrawCount, defaultData.maxDrawCount);
            detailContext.End_PropertyTable();
        }
    }

    if (changed)
    {
        emitter.rendererType = emitter.typeData.kind == AuthoringTypeDataKind::None
                               ? data.rendererType
                               : Authoring::Resolve_RendererType(emitter.typeData);
        emitter.textureId = data.material.mainTexturePath;
        emitter.resourceSummary = data.material.mainTexturePath;
    }

    return changed;
}

bool SpawnModuleDetail::Draw(AuthoringEmitter&, const AuthoringModule& module, SpawnModuleData& data, DetailPropertyContext& detailContext)
{
    const SpawnModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("연속 스폰", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("SpawnRate"))
        {
            changed |= detailContext.Draw_BoolProperty("연속 스폰 사용", data.processSpawnRate, defaultData.processSpawnRate);
            detailContext.End_PropertyTable();
        }

        changed |= detailContext.Draw_FloatDistributionGroup(
            "초당 생성 수",
            data.spawnRate,
            defaultData.spawnRate,
            0.01f,
            true,
            true,
            Find_AuthoringValueRange(module.type, "spawnRate")
        );
        changed |= detailContext.Draw_FloatDistributionGroup(
            "스폰 속도 스케일",
            data.spawnRateScale,
            defaultData.spawnRateScale,
            0.01f,
            true,
            true,
            Find_AuthoringValueRange(module.type, "spawnRateScale")
        );
    }

    if (ImGui::CollapsingHeader("버스트", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("SpawnBurst"))
        {
            changed |= detailContext.Draw_BoolProperty("버스트 사용", data.processBurstList, defaultData.processBurstList);
            detailContext.End_PropertyTable();
        }

        changed |= detailContext.Draw_FloatDistributionGroup(
            "버스트 스케일",
            data.burstScale,
            defaultData.burstScale,
            0.01f,
            true,
            true,
            Find_AuthoringValueRange(module.type, "burstScale")
        );
        changed |= detailContext.Draw_BurstListProperty(data.burstList, defaultData.burstList);
    }

    if (ImGui::CollapsingHeader("제한", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("SpawnLimit"))
        {
            changed |= detailContext.Draw_UintProperty("최대 활성 파티클 수", data.maxParticleCount, defaultData.maxParticleCount);
            detailContext.End_PropertyTable();
        }
    }

    return changed;
}

bool LifetimeModuleDetail::Draw(const AuthoringEmitter& emitter, const AuthoringModule& module, LifetimeModuleData& data, DetailPropertyContext& detailContext)
{
    const LifetimeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("파티클 생존 시간", ImGuiTreeNodeFlags_DefaultOpen))
    {
        changed |= detailContext.Draw_FloatDistributionGroup(
            "파티클 생존 시간",
            data.lifeTime,
            defaultData.lifeTime,
            0.01f,
            true,
            true,
            Find_AuthoringValueRange(module.type, "lifeTime")
        );
        ImGui::TextDisabled("이미 생성된 particle 또는 visual life를 생애 중간에 다시 바꾸는 Over Life curve가 아닙니다.");
        if (emitter.typeData.kind == AuthoringTypeDataKind::Beam)
        {
            ImGui::TextDisabled("Beam ConstantCurve: 전체 반복 순서 진행률로 각 loop의 visual life를 한 번 샘플합니다.");
            ImGui::TextDisabled("Beam 무한 반복: 반복 번호를 curve 구간에 순환 적용합니다.");
        }
        else if (emitter.typeData.kind == AuthoringTypeDataKind::Mesh)
            ImGui::TextDisabled("Mesh ConstantCurve: particle spawn 시점의 현재 loop 진행률로 최대 수명을 한 번 샘플합니다.");
        else if (emitter.typeData.kind == AuthoringTypeDataKind::Trail ||
                 emitter.typeData.kind == AuthoringTypeDataKind::Ribbon ||
                 emitter.typeData.kind == AuthoringTypeDataKind::SourceHistorySpriteTrail)
            ImGui::TextDisabled("Trail/Ribbon/SourceHistorySpriteTrail ConstantCurve sampling axis는 이번 Sprite/Mesh/Beam 구현 범위 밖입니다.");
        else
            ImGui::TextDisabled("Sprite ConstantCurve: particle spawn 시점의 현재 loop 진행률로 최대 수명을 한 번 샘플합니다.");
    }

    return changed;
}

bool InitialSizeModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, InitialSizeModuleData& data, DetailPropertyContext& detailContext)
{
    const InitialSizeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("크기", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= detailContext.Draw_Vector2DistributionGroup("크기", data.size, defaultData.size);

    return changed;
}

bool InitialMeshSizeModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, InitialMeshSizeModuleData& data, DetailPropertyContext& detailContext)
{
    const InitialMeshSizeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("메시 크기", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= detailContext.Draw_Vector3DistributionGroup("크기 XYZ", data.size, defaultData.size);

    return changed;
}

bool InitialLocationModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, InitialLocationModuleData& data, DetailPropertyContext& detailContext)
{
    const InitialLocationModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("위치", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= detailContext.Draw_Vector3DistributionGroup("위치", data.location, defaultData.location, 0.01f, false, true);

    return changed;
}

bool SphereLocationModuleDetail::Draw(AuthoringEmitter&, const AuthoringModule& module, SphereLocationModuleData& data, DetailPropertyContext& detailContext)
{
    const SphereLocationModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("구 위치", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("SphereLocation"))
        {
            changed |= detailContext.Draw_Vec3Property("오프셋", data.offset, defaultData.offset);
            changed |= detailContext.Draw_FloatProperty(
                "반지름",
                data.radius,
                defaultData.radius,
                0.01f,
                nullptr,
                Find_AuthoringValueRange(module.type, "radius")
            );
            changed |= Internal::Draw_SphereLocationSpawnModeProperty(
                detailContext,
                "생성 방식",
                data.spawnMode,
                defaultData.spawnMode
            );
            const bool placementModeEnabled = data.spawnMode == SphereLocationSpawnMode::Surface;
            changed |= Internal::Draw_SphereLocationPlacementModeProperty(
                detailContext,
                "표면 배치",
                data.placementMode,
                defaultData.placementMode,
                placementModeEnabled
            );
            detailContext.End_PropertyTable();
        }

        changed |= Internal::Draw_RandomSeedProperty(detailContext, data.randomSeed);
    }

    data.radius = max(0.f, data.radius);
    return changed;
}

bool PlaneRadialLocationModuleDetail::Draw(
    AuthoringEmitter&,
    const AuthoringModule& module,
    PlaneRadialLocationModuleData& data,
    DetailPropertyContext& detailContext)
{
    const PlaneRadialLocationModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("평면/방사 위치", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("PlaneRadialLocation"))
        {
            changed |= Internal::Draw_PlaneRadialLocationPlaneProperty(
                detailContext,
                "기준 평면",
                data.plane,
                defaultData.plane
            );
            changed |= Internal::Draw_PlaneRadialLocationShapeProperty(
                detailContext,
                "형태",
                data.shape,
                defaultData.shape
            );
            changed |= detailContext.Draw_Vec3Property("중심 오프셋", data.offset, defaultData.offset);
            changed |= detailContext.Draw_FloatProperty(
                "두께",
                data.thickness,
                defaultData.thickness,
                0.01f,
                "기준 평면의 normal 방향으로 더할 위치 범위입니다. runtime sampling에서 음수는 절댓값 기준으로 정규화할 예정입니다."
            );
            detailContext.End_PropertyTable();
        }

        if (data.shape == PlaneRadialLocationShape::Rectangle)
        {
            changed |= detailContext.Draw_FloatDistributionGroup("U 분포", data.uDistribution, defaultData.uDistribution, 0.01f, true, false);
            changed |= detailContext.Draw_FloatDistributionGroup("V 분포", data.vDistribution, defaultData.vDistribution, 0.01f, true, false);
        }
        else
        {
            changed |= detailContext.Draw_FloatDistributionGroup(
                "반지름 분포",
                data.radiusDistribution,
                defaultData.radiusDistribution,
                0.01f,
                true,
                false,
                Find_AuthoringValueRange(module.type, "radiusDistribution")
            );
            changed |= detailContext.Draw_FloatDistributionGroup(
                "각도 분포",
                data.angleDegreesDistribution,
                defaultData.angleDegreesDistribution,
                0.01f,
                true,
                false
            );
        }

        ImGui::TextDisabled("위치 분포는 spawn 시점 1회 샘플이라 현재 상수/균등만 지원합니다.");
        if (data.shape == PlaneRadialLocationShape::Disc)
            ImGui::TextDisabled("원반 채움은 보통 반지름 최소값 0을 사용합니다.");

        if (ImGui::TreeNodeEx("고급", ImGuiTreeNodeFlags_SpanAvailWidth))
        {
            const bool placementModeEnabled = Internal::Is_PlaneRadialLocationAdvancedPlacementShape(data.shape);
            if (detailContext.Begin_PropertyTable("PlaneRadialLocationAdvanced"))
            {
                changed |= Internal::Draw_PlaneRadialLocationPlacementModeProperty(
                    detailContext,
                    "배치 방식",
                    data.placementMode,
                    defaultData.placementMode,
                    placementModeEnabled
                );
                detailContext.End_PropertyTable();
            }

            if (!placementModeEnabled)
                ImGui::TextDisabled("배치 방식은 Rectangle에서는 사용되지 않습니다.");

            ImGui::TreePop();
        }
    }

    return changed;
}

bool InitialVelocityModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, InitialVelocityModuleData& data, DetailPropertyContext& detailContext)
{
    const InitialVelocityModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("속도", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("InitialVelocity"))
        {
            changed |= detailContext.Draw_BoolProperty("월드 스페이스", data.inWorldSpace, defaultData.inWorldSpace);
            detailContext.End_PropertyTable();
        }

        changed |= detailContext.Draw_Vector3DistributionGroup("속도", data.velocity, defaultData.velocity);
    }

    return changed;
}

bool InitialRadialVelocityModuleDetail::Draw(
    const AuthoringEmitter& emitter,
    AuthoringModule&,
    InitialRadialVelocityModuleData& data,
    DetailPropertyContext& detailContext)
{
    const InitialRadialVelocityModuleData defaultData{};
    bool changed = false;

    const bool radialVelocityOpen = ImGui::CollapsingHeader("방사성 속도", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
    {
        ImGui::SetTooltip(
            "생성 위치와 방사 기준점의 방향으로 더하는 초기 속도입니다.\n"
            "중앙 발산/흡입형 sprite streak의 1차 후보입니다.\n"
            "양수는 기준점에서 멀어지고, 음수는 기준점 쪽으로 이동합니다.\n"
            "지속 attractor가 아니라 생성 시점의 속도 항입니다."
        );
    }

    if (radialVelocityOpen)
    {
        if (detailContext.Begin_PropertyTable("InitialRadialVelocity"))
        {
            changed |= Internal::Draw_RadialPivotProperty(detailContext, data.radialPivot, defaultData.radialPivot);
            changed |= Internal::Draw_RadialWorldSpaceProperty(detailContext, data.inWorldSpace, defaultData.inWorldSpace);
            changed |= Internal::Draw_InitialRadialVelocityCenterDirectionModeProperty(
                detailContext,
                data.centerDirectionMode,
                defaultData.centerDirectionMode
            );
            detailContext.End_PropertyTable();
        }

        if (data.centerDirectionMode == InitialRadialVelocityCenterDirectionMode::PlaneRadial)
        {
            ImGui::TextDisabled("평면 방사는 같은 emitter의 평면/방사 위치 방향을 사용합니다.");
            if (!Internal::Has_PlaneRadialLocationModule(emitter))
                ImGui::TextDisabled("평면/방사 위치가 없으면 랜덤 상승 방향으로 처리됩니다.");
        }

        changed |= detailContext.Draw_FloatDistributionGroup("속도", data.speed, defaultData.speed);
    }

    return changed;
}

bool VelocityConeModuleDetail::Draw(
    AuthoringEmitter&,
    AuthoringModule&,
    VelocityConeModuleData& data,
    DetailPropertyContext& detailContext)
{
    const VelocityConeModuleData defaultData{};
    bool changed = false;

    const bool coneVelocityOpen = ImGui::CollapsingHeader("속도 원뿔", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
    {
        ImGui::SetTooltip(
            "생성 위치와 무관하게 축 기준 원뿔 안에서 뽑은 방향으로 더하는 초기 속도입니다.\n"
            "InitialVelocity, InitialRadialVelocity와 누적됩니다.\n"
            "각도는 중심축 기준 half-angle입니다."
        );
    }

    if (coneVelocityOpen)
    {
        if (detailContext.Begin_PropertyTable("VelocityCone"))
        {
            changed |= detailContext.Draw_Vec3Property("축", data.axis, defaultData.axis);
            changed |= detailContext.Draw_FloatProperty("각도", data.angleDegrees, defaultData.angleDegrees, 0.1f);
            changed |= detailContext.Draw_BoolProperty("월드 스페이스", data.inWorldSpace, defaultData.inWorldSpace);
            detailContext.End_PropertyTable();
        }

        const float clampedAngle = clamp(data.angleDegrees, 0.f, 180.f);
        if (data.angleDegrees != clampedAngle)
        {
            data.angleDegrees = clampedAngle;
            changed = true;
        }

        changed |= detailContext.Draw_FloatDistributionGroup("속도", data.speed, defaultData.speed);
    }

    return changed;
}

bool AccelerationModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, AccelerationModuleData& data, DetailPropertyContext& detailContext)
{
    const AccelerationModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("가속", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= detailContext.Draw_Vector3DistributionGroup("가속", data.acceleration, defaultData.acceleration);

    return changed;
}

bool DragModuleDetail::Draw(AuthoringEmitter&, const AuthoringModule& module, DragModuleData& data, DetailPropertyContext& detailContext)
{
    const DragModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("드래그", ImGuiTreeNodeFlags_DefaultOpen))
    {
        changed |= detailContext.Draw_FloatDistributionGroup(
            "드래그",
            data.drag,
            defaultData.drag,
            0.01f,
            true,
            true,
            Find_AuthoringValueRange(module.type, "drag")
        );
    }

    return changed;
}

bool VelocityOverLifeModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, VelocityOverLifeModuleData& data, DetailPropertyContext& detailContext)
{
    const VelocityOverLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명에 따른 속도", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= detailContext.Draw_FloatDistributionGroup("속도 배율", data.scaleOverLife, defaultData.scaleOverLife);

    return changed;
}

bool InitialRotationModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, InitialRotationModuleData& data, DetailPropertyContext& detailContext)
{
    const InitialRotationModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("초기 회전", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= detailContext.Draw_FloatDistributionGroup("회전각", data.rotationDegrees, defaultData.rotationDegrees);

    return changed;
}

bool OrbitOverLifeModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, OrbitOverLifeModuleData& data, DetailPropertyContext& detailContext)
{
    const OrbitOverLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명에 따른 공전", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("OrbitOverLife"))
        {
            Internal::Draw_ReadOnlyTextProperty(detailContext, "Pivot", "Emitter Origin", "v1 runtime은 EmitterOrigin만 소비합니다.");
            changed |= Internal::Draw_EffectOrbitPlaneProperty(detailContext, "평면", data.plane, defaultData.plane);
            Internal::Draw_ReadOnlyTextProperty(detailContext, "방향 추적", "Preserve", "v1 runtime은 orientation follow를 적용하지 않습니다.");
            detailContext.End_PropertyTable();
        }

        changed |= detailContext.Draw_FloatDistributionGroup("공전 각도", data.angleDegreesOverLife, defaultData.angleDegreesOverLife);
        changed |= detailContext.Draw_FloatDistributionGroup("반지름 배율", data.radiusScaleOverLife, defaultData.radiusScaleOverLife);
    }

    return changed;
}

bool PlaneRadialOrientationModuleDetail::Draw(
    const AuthoringEmitter& emitter,
    AuthoringModule&,
    PlaneRadialOrientationModuleData& data,
    DetailPropertyContext& detailContext)
{
    const PlaneRadialOrientationModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("평면/방사 회전", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("PlaneRadialOrientation"))
        {
            Internal::Draw_PlaneRadialOrientationResolvedTargetProperty(detailContext, emitter);
            changed |= Internal::Draw_PlaneRadialOrientationModeProperty(
                detailContext,
                "방향",
                data.orientationMode,
                defaultData.orientationMode
            );

            if (Internal::Should_ShowPlaneRadialOrientationMeshFields(emitter))
            {
                changed |= Internal::Draw_PlaneRadialOrientationAxisProperty(
                    detailContext,
                    "메시 앞축",
                    data.meshForwardAxis,
                    defaultData.meshForwardAxis
                );
                changed |= Internal::Draw_PlaneRadialOrientationAxisProperty(
                    detailContext,
                    "메시 위축",
                    data.meshUpAxis,
                    defaultData.meshUpAxis
                );
                changed |= detailContext.Draw_FloatProperty("기울기", data.tiltDegrees, defaultData.tiltDegrees, 0.1f);
                changed |= detailContext.Draw_FloatProperty("Roll 오프셋", data.rollOffsetDegrees, defaultData.rollOffsetDegrees, 0.1f);
            }

            detailContext.End_PropertyTable();
        }

        if (!Internal::Has_PlaneRadialLocationModule(emitter))
            ImGui::TextDisabled("같은 emitter에 평면/방사 위치가 없으면 runtime radial frame을 만들 수 없습니다.");
    }

    return changed;
}

bool SphereRadialOrientationModuleDetail::Draw(
    const AuthoringEmitter& emitter,
    AuthoringModule&,
    SphereRadialOrientationModuleData& data,
    DetailPropertyContext& detailContext)
{
    const SphereRadialOrientationModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("구/방사 회전", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("SphereRadialOrientation"))
        {
            changed |= Internal::Draw_SphereRadialOrientationModeProperty(
                detailContext,
                "방향",
                data.orientationMode,
                defaultData.orientationMode
            );
            changed |= Internal::Draw_PlaneRadialOrientationAxisProperty(
                detailContext,
                "메시 앞축",
                data.meshForwardAxis,
                defaultData.meshForwardAxis
            );
            changed |= Internal::Draw_PlaneRadialOrientationAxisProperty(
                detailContext,
                "메시 위축",
                data.meshUpAxis,
                defaultData.meshUpAxis
            );
            changed |= detailContext.Draw_FloatProperty("기울기", data.tiltDegrees, defaultData.tiltDegrees, 0.1f);
            changed |= detailContext.Draw_FloatProperty("Roll 오프셋", data.rollOffsetDegrees, defaultData.rollOffsetDegrees, 0.1f);

            detailContext.End_PropertyTable();
        }

        if (!Internal::Has_SphereLocationModule(emitter))
            ImGui::TextDisabled("같은 emitter에 구 위치가 없으면 runtime sphere radial frame을 만들 수 없습니다.");
        if (data.orientationMode != SphereRadialOrientationMode::None &&
            Internal::Has_EnabledPlaneRadialOrientationModule(emitter))
            ImGui::TextDisabled("평면/방사 회전도 켜져 있으면 runtime은 구/방사 회전을 먼저 적용합니다.");
    }

    return changed;
}

bool RotationOverLifeModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, RotationOverLifeModuleData& data, DetailPropertyContext& detailContext)
{
    const RotationOverLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명에 따른 회전", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= detailContext.Draw_FloatDistributionGroup("추가 회전각", data.rotationOverLife, defaultData.rotationOverLife);

    return changed;
}

bool InitialRotationRateModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, InitialRotationRateModuleData& data, DetailPropertyContext& detailContext)
{
    const InitialRotationRateModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("초기 회전 속도", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= detailContext.Draw_FloatDistributionGroup("회전 속도", data.rotationRateDegrees, defaultData.rotationRateDegrees);

    return changed;
}

bool RotationRateScaleByLifeModuleDetail::Draw(
    AuthoringEmitter&,
    AuthoringModule&,
    RotationRateScaleByLifeModuleData& data,
    DetailPropertyContext& detailContext)
{
    const RotationRateScaleByLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명에 따른 회전 속도", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= detailContext.Draw_FloatDistributionGroup("회전 속도 배율", data.scaleOverLife, defaultData.scaleOverLife);

    return changed;
}

bool InitialMeshRotationModuleDetail::Draw(
    AuthoringEmitter&,
    AuthoringModule&,
    InitialMeshRotationModuleData& data,
    DetailPropertyContext& detailContext)
{
    const InitialMeshRotationModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("초기 메시 회전", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= detailContext.Draw_Vector3DistributionGroup("회전 XYZ", data.rotationDegrees, defaultData.rotationDegrees);

    return changed;
}

bool MeshRotationOverLifeModuleDetail::Draw(
    AuthoringEmitter&,
    AuthoringModule&,
    MeshRotationOverLifeModuleData& data,
    DetailPropertyContext& detailContext)
{
    const MeshRotationOverLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명에 따른 메시 회전", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= detailContext.Draw_Vector3DistributionGroup("추가 회전 XYZ", data.rotationOverLife, defaultData.rotationOverLife);

    return changed;
}

bool InitialMeshRotationRateModuleDetail::Draw(
    AuthoringEmitter&,
    AuthoringModule&,
    InitialMeshRotationRateModuleData& data,
    DetailPropertyContext& detailContext)
{
    const InitialMeshRotationRateModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("초기 메시 회전 속도", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= detailContext.Draw_Vector3DistributionGroup("회전 속도 XYZ", data.rotationRateDegrees, defaultData.rotationRateDegrees);

    return changed;
}

bool MeshRotationRateScaleByLifeModuleDetail::Draw(
    AuthoringEmitter&,
    AuthoringModule&,
    MeshRotationRateScaleByLifeModuleData& data,
    DetailPropertyContext& detailContext)
{
    const MeshRotationRateScaleByLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명에 따른 메시 회전 속도", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= detailContext.Draw_Vector3DistributionGroup("회전 속도 배율 XYZ", data.scaleOverLife, defaultData.scaleOverLife);

    return changed;
}

bool InitialColorModuleDetail::Draw(AuthoringEmitter&, const AuthoringModule& module, InitialColorModuleData& data, DetailPropertyContext& detailContext)
{
    const InitialColorModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("초기 컬러", ImGuiTreeNodeFlags_DefaultOpen))
    {
        changed |= detailContext.Draw_ColorRgbDistributionGroup("색상(RGB)", data.color, defaultData.color);
        changed |= detailContext.Draw_FloatDistributionGroup(
            "알파",
            data.alpha,
            defaultData.alpha,
            0.01f,
            true,
            true,
            Find_AuthoringValueRange(module.type, "alpha")
        );
    }

    return changed;
}

bool ColorOverLifeModuleDetail::Draw(AuthoringEmitter&, const AuthoringModule& module, ColorOverLifeModuleData& data, DetailPropertyContext& detailContext)
{
    const ColorOverLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명에 따른 색상", ImGuiTreeNodeFlags_DefaultOpen))
    {
        changed |= detailContext.Draw_ColorRgbDistributionGroup("색상(RGB)", data.colorOverLife, defaultData.colorOverLife);
        changed |= detailContext.Draw_FloatDistributionGroup(
            "알파",
            data.alphaOverLife,
            defaultData.alphaOverLife,
            0.01f,
            true,
            true,
            Find_AuthoringValueRange(module.type, "alphaOverLife")
        );
    }

    return changed;
}

bool SubUVFrameOverLifeModuleDetail::Draw(
    const AuthoringEmitter& emitter,
    AuthoringModule&,
    SubUVFrameOverLifeModuleData& data,
    DetailPropertyContext& detailContext)
{
    const SubUVFrameOverLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("서브UV 프레임 선택/재생", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const bool isBeamEmitter = emitter.typeData.kind == AuthoringTypeDataKind::Beam;

        if (detailContext.Begin_PropertyTable("SubUVFrameOverLife"))
        {
            changed |= Internal::Draw_SubUVFramePlaybackModeProperty(
                detailContext,
                "재생 방식",
                data.playbackMode,
                defaultData.playbackMode
            );

            if (data.playbackMode == SubUVFramePlaybackMode::FixedFrame)
                changed |= detailContext.Draw_UintProperty("프레임", data.startFrame, defaultData.startFrame);
            else
            {
                changed |= detailContext.Draw_UintProperty("시작 프레임", data.startFrame, defaultData.startFrame);
                changed |= detailContext.Draw_UintProperty("종료 프레임", data.endFrame, defaultData.endFrame);

                if (data.playbackMode == SubUVFramePlaybackMode::FramesPerSecond)
                {
                    changed |= detailContext.Draw_BoolProperty("루프", data.loop, defaultData.loop);
                    changed |= detailContext.Draw_FloatProperty("FPS", data.framesPerSecond, defaultData.framesPerSecond, 0.1f);
                }
            }

            detailContext.End_PropertyTable();
        }

        const bool usesRandomSeed =
            data.playbackMode == SubUVFramePlaybackMode::RandomFrame ||
            (isBeamEmitter &&
             data.playbackMode == SubUVFramePlaybackMode::FramesPerSecond &&
             data.perStripSubUVVariation);
        if (isBeamEmitter)
        {
            if (ImGui::CollapsingHeader("Beam Multi-Strip Variation", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (detailContext.Begin_PropertyTable("BeamSubUVVariation"))
                {
                    changed |= detailContext.Draw_BoolProperty(
                        "Per-Strip SubUV Variation",
                        data.perStripSubUVVariation,
                        defaultData.perStripSubUVVariation,
                        "Beam multi-strip에서 RandomFrame은 strip별로 샘플하고, loop FPS는 strip별 phase offset을 적용합니다."
                    );
                    detailContext.End_PropertyTable();
                }

                if (!usesRandomSeed)
                    ImGui::TextDisabled("RandomFrame 또는 looped FramesPerSecond에서 seed 기반 strip variation을 사용합니다.");
            }
        }

        if (usesRandomSeed)
            changed |= Internal::Draw_RandomSeedProperty(detailContext, data.randomSeed);

        const uint32 frameCount = Internal::Resolve_SubUvFrameCount(emitter);
        const uint32 lastFrame = frameCount > 0 ? frameCount - 1 : 0;
        data.startFrame = min(data.startFrame, lastFrame);
        data.endFrame = min(data.endFrame, lastFrame);
        data.framesPerSecond = max(0.f, data.framesPerSecond);
        changed |= Internal::Normalize_SubUVFrameIndexDistribution(data.frameIndex, lastFrame);

        ImGui::TextDisabled("현재 atlas frame 수: %u", frameCount);
    }

    return changed;
}

bool SizeByLifeModuleDetail::Draw(
    AuthoringEmitter& emitter,
    AuthoringModule&,
    SizeByLifeModuleData& data,
    DetailPropertyContext& detailContext)
{
    const SizeByLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명에 따른 크기", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("SizeByLife"))
        {
            changed |= detailContext.Draw_BoolProperty("X축 적용", data.multiplyX, defaultData.multiplyX);
            changed |= detailContext.Draw_BoolProperty("Y축 적용", data.multiplyY, defaultData.multiplyY);
            detailContext.End_PropertyTable();
        }

        const auto axis_lock_label = [](SizeByLifeModuleData::AxisLock axisLock)
        {
            switch (axisLock)
            {
            case SizeByLifeModuleData::AxisLock::X:
                return "X";
            case SizeByLifeModuleData::AxisLock::Y:
                return "Y";
            case SizeByLifeModuleData::AxisLock::None:
            default:
                return "None";
            }
        };

        ImGui::PushID("SizeByLifeAxisLock");
        ImGui::TextUnformatted("축 잠금");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160.f);
        if (ImGui::BeginCombo("##AxisLock", axis_lock_label(data.axisLock)))
        {
            const auto draw_axis_lock = [&](SizeByLifeModuleData::AxisLock value)
            {
                const bool selected = data.axisLock == value;
                if (ImGui::Selectable(axis_lock_label(value), selected))
                {
                    data.axisLock = value;
                    changed = true;
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            };

            draw_axis_lock(SizeByLifeModuleData::AxisLock::None);
            draw_axis_lock(SizeByLifeModuleData::AxisLock::X);
            draw_axis_lock(SizeByLifeModuleData::AxisLock::Y);
            ImGui::EndCombo();
        }
        ImGui::PopID();

        changed |= detailContext.Draw_Vector2DistributionGroup("수명 배율", data.scaleOverLife, defaultData.scaleOverLife, 0.01f);
    }

    return changed;
}

bool MeshSizeByLifeModuleDetail::Draw(
    AuthoringEmitter&,
    AuthoringModule&,
    MeshSizeByLifeModuleData& data,
    DetailPropertyContext& detailContext)
{
    const MeshSizeByLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명에 따른 메시 크기", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("MeshSizeByLife"))
        {
            changed |= detailContext.Draw_BoolProperty("X축 적용", data.multiplyX, defaultData.multiplyX);
            changed |= detailContext.Draw_BoolProperty("Y축 적용", data.multiplyY, defaultData.multiplyY);
            changed |= detailContext.Draw_BoolProperty("Z축 적용", data.multiplyZ, defaultData.multiplyZ);
            detailContext.End_PropertyTable();
        }

        changed |= detailContext.Draw_Vector3DistributionGroup("수명 배율 XYZ", data.scaleOverLife, defaultData.scaleOverLife, 0.01f);
    }

    return changed;
}

bool SpawnPerUnitModuleDetail::Draw(
    AuthoringEmitter&,
    AuthoringModule&,
    SpawnPerUnitModuleData& data,
    DetailPropertyContext& detailContext)
{
    const SpawnPerUnitModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("거리당 스폰", ImGuiTreeNodeFlags_DefaultOpen))
    {
        changed |= detailContext.Draw_FloatDistributionGroup("분포", data.spawnPerUnit, defaultData.spawnPerUnit);

        if (detailContext.Begin_PropertyTable("SpawnPerUnitPolicy"))
        {
            changed |= detailContext.Draw_FloatProperty(
                "단위 스칼라",
                data.unitScalar,
                defaultData.unitScalar,
                0.01f,
                "분포 값을 실제 world distance로 환산하는 기준 스칼라입니다."
            );
            changed |= detailContext.Draw_FloatProperty(
                "무브먼트 허용치",
                data.movementTolerance,
                defaultData.movementTolerance,
                0.001f,
                "이 거리 미만의 provider sample 이동은 새 history sample로 받지 않습니다."
            );
            changed |= detailContext.Draw_FloatProperty(
                "최대 프레임 거리",
                data.maxFrameDistance,
                defaultData.maxFrameDistance,
                0.01f,
                "한 frame 이동이 이 거리보다 크면 teleport/skip으로 보고 history를 재시드합니다. 0 이하면 제한이 없습니다."
            );
            detailContext.End_PropertyTable();
        }
    }

    data.unitScalar = max(0.0001f, data.unitScalar);
    data.movementTolerance = max(0.f, data.movementTolerance);
    data.maxFrameDistance = max(0.f, data.maxFrameDistance);
    return changed;
}

bool SourceHistorySpriteTrailPathFollowModuleDetail::Draw(
    AuthoringEmitter&,
    AuthoringModule&,
    SourceHistorySpriteTrailPathFollowModuleData& data,
    DetailPropertyContext& detailContext)
{
    const SourceHistorySpriteTrailPathFollowModuleData defaultData{};
    bool changed = false;

    const auto direction_label = [](SourceHistorySpriteTrailPathFollowDirection direction)
    {
        switch (direction)
        {
        case SourceHistorySpriteTrailPathFollowDirection::TowardHead:
            return "Head 방향";
        case SourceHistorySpriteTrailPathFollowDirection::TowardTail:
            return "Tail 방향";
        default:
            return "Head 방향";
        }
    };

    const auto arrival_mode_label = [](SourceHistorySpriteTrailArrivalMode mode)
    {
        switch (mode)
        {
        case SourceHistorySpriteTrailArrivalMode::ClampAtEnd:
            return "끝점 고정";
        case SourceHistorySpriteTrailArrivalMode::KillOnArrive:
        default:
            return "도착 시 제거";
        }
    };

    if (ImGui::CollapsingHeader("경로 따라가기", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("SourceHistorySpriteTrailPathFollowPolicy"))
        {
            ImGui::PushID("Direction");
            detailContext.Draw_PropertyLabel("방향");
            SourceHistorySpriteTrailPathFollowDirection nextDirection = data.direction;
            if (ImGui::BeginCombo("##Value", direction_label(nextDirection)))
            {
                const SourceHistorySpriteTrailPathFollowDirection candidates[] = {
                    SourceHistorySpriteTrailPathFollowDirection::TowardHead,
                    SourceHistorySpriteTrailPathFollowDirection::TowardTail,
                };
                for (const SourceHistorySpriteTrailPathFollowDirection candidate : candidates)
                {
                    const bool selected = nextDirection == candidate;
                    if (ImGui::Selectable(direction_label(candidate), selected))
                        nextDirection = candidate;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (data.direction != nextDirection)
            {
                data.direction = nextDirection;
                changed = true;
            }
            ImGui::PopID();

            ImGui::PushID("ArrivalMode");
            detailContext.Draw_PropertyLabel("도착 후 처리");
            SourceHistorySpriteTrailArrivalMode nextArrivalMode = data.arrivalMode;
            if (ImGui::BeginCombo("##Value", arrival_mode_label(nextArrivalMode)))
            {
                const SourceHistorySpriteTrailArrivalMode candidates[] = {
                    SourceHistorySpriteTrailArrivalMode::KillOnArrive,
                    SourceHistorySpriteTrailArrivalMode::ClampAtEnd,
                };
                for (const SourceHistorySpriteTrailArrivalMode candidate : candidates)
                {
                    const bool selected = nextArrivalMode == candidate;
                    if (ImGui::Selectable(arrival_mode_label(candidate), selected))
                        nextArrivalMode = candidate;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (data.arrivalMode != nextArrivalMode)
            {
                data.arrivalMode = nextArrivalMode;
                changed = true;
            }
            ImGui::PopID();
            detailContext.End_PropertyTable();
        }

        changed |= detailContext.Draw_FloatDistributionGroup("속도", data.speed, defaultData.speed);
        changed |= detailContext.Draw_FloatDistributionGroup("시작 딜레이", data.startDelay, defaultData.startDelay);
    }

    return changed;
}

bool SourceHistorySpriteTrailPathReplayModuleDetail::Draw(
    AuthoringEmitter&,
    AuthoringModule&,
    SourceHistorySpriteTrailPathReplayModuleData& data,
    DetailPropertyContext& detailContext)
{
    const SourceHistorySpriteTrailPathReplayModuleData defaultData{};
    bool changed = false;

    const auto replay_mode_label = [](SourceHistorySpriteTrailPathReplayMode mode)
    {
        switch (mode)
        {
        case SourceHistorySpriteTrailPathReplayMode::RecordedSpeed:
            return "기록 속도";
        case SourceHistorySpriteTrailPathReplayMode::FitDuration:
            return "지정 시간";
        default:
            return "기록 속도";
        }
    };

    const auto start_mode_label = [](SourceHistorySpriteTrailPathReplayStartMode mode)
    {
        switch (mode)
        {
        case SourceHistorySpriteTrailPathReplayStartMode::TailFirst:
            return "Tail 먼저";
        case SourceHistorySpriteTrailPathReplayStartMode::AllAtOnce:
            return "동시 시작";
        default:
            return "Tail 먼저";
        }
    };

    const auto arrival_mode_label = [](SourceHistorySpriteTrailArrivalMode mode)
    {
        switch (mode)
        {
        case SourceHistorySpriteTrailArrivalMode::ClampAtEnd:
            return "끝점 고정";
        case SourceHistorySpriteTrailArrivalMode::KillOnArrive:
        default:
            return "도착 시 제거";
        }
    };

    if (ImGui::CollapsingHeader("경로 리플레이", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("SourceHistorySpriteTrailPathReplayPolicy"))
        {
            changed |= detailContext.Draw_FloatProperty(
                "딜레이",
                data.delayTime,
                defaultData.delayTime,
                0.01f,
                "stamp 생성 후 replay 시작 전 유지 시간입니다."
            );

            ImGui::PushID("ReplayMode");
            detailContext.Draw_PropertyLabel("재생 방식");
            SourceHistorySpriteTrailPathReplayMode nextReplayMode = data.replayMode;
            if (ImGui::BeginCombo("##Value", replay_mode_label(nextReplayMode)))
            {
                const SourceHistorySpriteTrailPathReplayMode candidates[] = {
                    SourceHistorySpriteTrailPathReplayMode::RecordedSpeed,
                    SourceHistorySpriteTrailPathReplayMode::FitDuration,
                };
                for (const SourceHistorySpriteTrailPathReplayMode candidate : candidates)
                {
                    const bool selected = nextReplayMode == candidate;
                    if (ImGui::Selectable(replay_mode_label(candidate), selected))
                        nextReplayMode = candidate;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (data.replayMode != nextReplayMode)
            {
                data.replayMode = nextReplayMode;
                changed = true;
            }
            ImGui::PopID();

            changed |= detailContext.Draw_FloatProperty(
                "속도 배율",
                data.speedScale,
                defaultData.speedScale,
                0.01f,
                "기록 속도 모드에서 source 기록 속도에 곱하는 배율입니다."
            );
            changed |= detailContext.Draw_FloatProperty(
                "흡수 시간",
                data.drainDuration,
                defaultData.drainDuration,
                0.01f,
                "지정 시간 모드에서 head까지 따라붙는 총 시간입니다."
            );

            ImGui::PushID("StartMode");
            detailContext.Draw_PropertyLabel("시작 방식");
            SourceHistorySpriteTrailPathReplayStartMode nextStartMode = data.startMode;
            if (ImGui::BeginCombo("##Value", start_mode_label(nextStartMode)))
            {
                const SourceHistorySpriteTrailPathReplayStartMode candidates[] = {
                    SourceHistorySpriteTrailPathReplayStartMode::TailFirst,
                    SourceHistorySpriteTrailPathReplayStartMode::AllAtOnce,
                };
                for (const SourceHistorySpriteTrailPathReplayStartMode candidate : candidates)
                {
                    const bool selected = nextStartMode == candidate;
                    if (ImGui::Selectable(start_mode_label(candidate), selected))
                        nextStartMode = candidate;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (data.startMode != nextStartMode)
            {
                data.startMode = nextStartMode;
                changed = true;
            }
            ImGui::PopID();

            ImGui::PushID("ArrivalMode");
            detailContext.Draw_PropertyLabel("도착 후 처리");
            SourceHistorySpriteTrailArrivalMode nextArrivalMode = data.arrivalMode;
            if (ImGui::BeginCombo("##Value", arrival_mode_label(nextArrivalMode)))
            {
                const SourceHistorySpriteTrailArrivalMode candidates[] = {
                    SourceHistorySpriteTrailArrivalMode::KillOnArrive,
                    SourceHistorySpriteTrailArrivalMode::ClampAtEnd,
                };
                for (const SourceHistorySpriteTrailArrivalMode candidate : candidates)
                {
                    const bool selected = nextArrivalMode == candidate;
                    if (ImGui::Selectable(arrival_mode_label(candidate), selected))
                        nextArrivalMode = candidate;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (data.arrivalMode != nextArrivalMode)
            {
                data.arrivalMode = nextArrivalMode;
                changed = true;
            }
            ImGui::PopID();
            detailContext.End_PropertyTable();
        }

        changed |= detailContext.Draw_FloatDistributionGroup("흡수 곡선", data.drainCurve, defaultData.drainCurve, 0.01f, false, true);
    }

    data.delayTime = max(0.f, data.delayTime);
    data.speedScale = max(0.f, data.speedScale);
    data.drainDuration = max(0.0001f, data.drainDuration);
    return changed;
}

bool MaterialScalarModulationModuleDetail::Draw(
    const AuthoringEmitter& emitter,
    AuthoringModule&,
    MaterialScalarModulationModuleData& data,
    DetailPropertyContext& detailContext)
{
    bool changed = false;
    const bool allowScalarParticleLife = Internal::Is_SpriteMaterialScalarParticleLifeSupported(emitter);

    if (ImGui::CollapsingHeader("머티리얼 변조", ImGuiTreeNodeFlags_DefaultOpen))
    {
        constexpr uint32 kMaxMaterialScalarModulators{ 8 };
        constexpr uint32 kMaxCoreColorRgbModulators{ 8 };
        if (ImGui::Button("+ 변조 추가"))
        {
            data.modulators.push_back(MaterialScalarModulatorData{});
            changed = true;
        }

        ImGui::SameLine();
        ImGui::TextDisabled(
            "%zu / %u",
            data.modulators.size() + data.coreColorRgbModulators.size(),
            kMaxMaterialScalarModulators + kMaxCoreColorRgbModulators
        );

        struct MaterialModulationUiRow
        {
            bool isCoreColorRgb{};
            size_t index{};
        };

        vector<MaterialModulationUiRow> rows{};
        rows.reserve(data.modulators.size() + data.coreColorRgbModulators.size());
        for (size_t index = 0; index < data.modulators.size(); ++index)
            rows.push_back(MaterialModulationUiRow{ false, index });
        for (size_t index = 0; index < data.coreColorRgbModulators.size(); ++index)
            rows.push_back(MaterialModulationUiRow{ true, index });

        for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
        {
            const MaterialModulationUiRow row = rows[rowIndex];
            ImGui::PushID(row.isCoreColorRgb ? "CoreColorRgb" : "Scalar");
            ImGui::PushID(static_cast<int>(row.index));

            const MaterialScalarModulationTimeSource timeSource =
                row.isCoreColorRgb
                ? data.coreColorRgbModulators[row.index].timeSource
                : data.modulators[row.index].timeSource;
            const string header =
                string("#") + to_string(rowIndex + 1) + " " +
                Internal::Get_MaterialModulationTargetLabel(
                    row.isCoreColorRgb,
                    row.isCoreColorRgb
                    ? MaterialScalarModulationTargetField::CoreColor
                    : data.modulators[row.index].targetField
                ) + " / " +
                Internal::Get_MaterialScalarTimeSourceLabel(timeSource);
            bool structureChanged = false;

            if (ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (row.isCoreColorRgb)
                {
                    MaterialCoreColorRgbModulatorData& modulator = data.coreColorRgbModulators[row.index];
                    MaterialCoreColorRgbModulatorData defaultData = Internal::Make_CoreColorRgbModulator(emitter);

                    if (detailContext.Begin_PropertyTable("MaterialCoreColorRgbModulator"))
                    {
                        changed |= detailContext.Draw_BoolProperty("변조 사용", modulator.enabled, defaultData.enabled);

                        bool nextCoreColorRgb = true;
                        MaterialScalarModulationTargetField nextTarget = MaterialScalarModulationTargetField::CoreColor;
                        if (Internal::Draw_MaterialModulationTargetProperty(
                            detailContext,
                            "대상",
                            true,
                            MaterialScalarModulationTargetField::CoreColor,
                            nextCoreColorRgb,
                            nextTarget
                        ))
                        {
                            changed = true;
                            if (!nextCoreColorRgb)
                            {
                                MaterialScalarModulatorData scalarModulator{};
                                scalarModulator.enabled = modulator.enabled;
                                scalarModulator.timeSource = modulator.timeSource;
                                scalarModulator.targetField = nextTarget;
                                scalarModulator.distribution = FloatDistributionData::Make_Constant(1.f);
                                data.modulators.push_back(scalarModulator);
                                data.coreColorRgbModulators.erase(data.coreColorRgbModulators.begin() + static_cast<ptrdiff_t>(row.index));
                                structureChanged = true;
                            }
                        }

                        if (!structureChanged)
                        {
                            changed |= Internal::Draw_MaterialScalarTimeSourceProperty(
                                detailContext,
                                "시간 축",
                                modulator.timeSource,
                                defaultData.timeSource,
                                false
                            );
                        }
                        detailContext.End_PropertyTable();
                    }

                    if (!structureChanged)
                    {
                        changed |= detailContext.Draw_Vector3DistributionGroup(
                            "분포",
                            modulator.distribution,
                            defaultData.distribution
                        );

                        if (ImGui::Button("삭제"))
                        {
                            data.coreColorRgbModulators.erase(data.coreColorRgbModulators.begin() + static_cast<ptrdiff_t>(row.index));
                            changed = true;
                            structureChanged = true;
                        }
                    }
                }
                else
                {
                    MaterialScalarModulatorData& modulator = data.modulators[row.index];
                    const MaterialScalarModulatorData defaultData{};

                    if (detailContext.Begin_PropertyTable("MaterialScalarModulator"))
                    {
                        changed |= detailContext.Draw_BoolProperty("변조 사용", modulator.enabled, defaultData.enabled);

                        bool nextCoreColorRgb = false;
                        MaterialScalarModulationTargetField nextTarget = modulator.targetField;
                        if (Internal::Draw_MaterialModulationTargetProperty(
                            detailContext,
                            "대상",
                            false,
                            modulator.targetField,
                            nextCoreColorRgb,
                            nextTarget
                        ))
                        {
                            changed = true;
                            if (nextCoreColorRgb)
                            {
                                MaterialCoreColorRgbModulatorData coreColorRgbModulator = Internal::Make_CoreColorRgbModulator(emitter);
                                coreColorRgbModulator.enabled = modulator.enabled;
                                coreColorRgbModulator.timeSource = modulator.timeSource;
                                data.coreColorRgbModulators.push_back(coreColorRgbModulator);
                                data.modulators.erase(data.modulators.begin() + static_cast<ptrdiff_t>(row.index));
                                structureChanged = true;
                            }
                            else
                                modulator.targetField = nextTarget;
                        }

                        if (!structureChanged)
                        {
                            detailContext.Draw_PropertyLabel("연산");
                            ImGui::TextUnformatted(Internal::Get_MaterialScalarOperationLabel(modulator.operation));
                            if (detailContext.Draw_ResetButton(modulator.operation != defaultData.operation))
                            {
                                modulator.operation = defaultData.operation;
                                changed = true;
                            }
                            changed |= Internal::Draw_MaterialScalarTimeSourceProperty(
                                detailContext,
                                "시간 축",
                                modulator.timeSource,
                                defaultData.timeSource,
                                allowScalarParticleLife
                            );
                        }
                        detailContext.End_PropertyTable();
                    }

                    if (!structureChanged)
                    {
                        changed |= detailContext.Draw_FloatDistributionGroup(
                            "분포",
                            modulator.distribution,
                            defaultData.distribution
                        );

                        if (ImGui::Button("삭제"))
                        {
                            data.modulators.erase(data.modulators.begin() + static_cast<ptrdiff_t>(row.index));
                            changed = true;
                            structureChanged = true;
                        }
                    }
                }
            }

            ImGui::PopID();
            ImGui::PopID();

            if (structureChanged)
                break;
        }
    }

    return changed;
}

NS_END

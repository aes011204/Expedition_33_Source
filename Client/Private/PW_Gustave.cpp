#include "PW_Gustave.h"

#include <algorithm>
#include <cmath>

#include <OffsetAnimationModifier.h>

#include "Body_Player.h"
#include "CurveAnimationModifier.h"
#include "CurveClip.h"
#include "GameInstance.h"
#include "Bounding_OBB.h"
#include "ColliderCom.h"
#include "Head_Player.h"
#include "ModelCom.h"
#include "PlayableParts.h"
#include "PlayerStatCom.h"
#include "SkillTreeCom.h"

namespace
{
    constexpr wchar_t kDebugFacialCurveClipRelPath[] =
        L"Animations/Facial/Metahuman_Common/MicroMovements/A_MH_RandomMovement_GameMenu.curveclip";
    constexpr float kDebugFacialMorphStrength = 2.f;
    constexpr float kDebugFacialBlinkStrength = 5.5f;

    constexpr float kDebugFacialEyeYawDegrees = 20.f;
    constexpr float kDebugFacialEyePitchDegrees = 10.f;

    bool Try_GetCurveValue(const CurveClip::CurveClip_Pose& pose, const string& name, float& outValue)
    {
        for (const CurveClip::Curve& curve : pose.curves)
        {
            if (curve.name == name)
            {
                outValue = curve.value;
                return true;
            }
        }

        return false;
    }

    float Average_CurvePair(const CurveClip::CurveClip_Pose& pose, const string& leftName, const string& rightName)
    {
        float sum = 0.f;
        float value = 0.f;
        uint32 count = 0;

        if (Try_GetCurveValue(pose, leftName, value))
        {
            sum += value;
            ++count;
        }

        if (Try_GetCurveValue(pose, rightName, value))
        {
            sum += value;
            ++count;
        }

        return count > 0 ? sum / static_cast<float>(count) : 0.f;
    }

    bool Is_BlinkMorphCurve(const string& curveName)
    {
        const bool isGeometryMorph =
            curveName.find("head_lod0_mesh__") == 0 ||
            curveName.find("cartilage_lod0_mesh__") == 0;
        if (!isGeometryMorph)
            return false;

        return curveName.find("eye_blink") != string::npos ||
            curveName.find("Eblink") != string::npos;
    }

    void Apply_BlinkMorphBoost(ModelCom* model, const CurveClip::CurveClip_Pose& pose)
    {
        if (model == nullptr)
            return;

        for (const CurveClip::Curve& curve : pose.curves)
        {
            if (!Is_BlinkMorphCurve(curve.name))
                continue;

            const int32 targetIndex = model->Find_MorphTargetIndex(curve.name);
            if (targetIndex < 0)
                continue;

            model->Set_MorphWeight_Unclamped(
                static_cast<uint32>(targetIndex),
                curve.value * kDebugFacialBlinkStrength);
        }
    }
}

IMPLEMENT_REFLECTION(PW_Gustave)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "PW_Gustave";
    info.category = "Character";
    

    return true;
}

PW_Gustave::PW_Gustave(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: Player_WorldBase { device, context }
{
}

PW_Gustave::PW_Gustave(const PW_Gustave& prototype)
	: Player_WorldBase{ prototype }
{

}

HRESULT PW_Gustave::Initialize(void* arg)
{
    CHECK_FAILED(__super::Initialize(arg), E_FAIL);

    auto skillTreeCom = Get_Component<SkillTreeCom>();

    skillTreeCom->TryUnlock("LumiereAssault");
    skillTreeCom->TryUnlock("UnleashedCharge");
    skillTreeCom->TryUnlock("MarkingShot");

    CHECK_FAILED(Ready_Modifier(), E_FAIL);

    return S_OK; 
}

void PW_Gustave::BeginPlay()
{
    Player_WorldBase::BeginPlay();

}

void PW_Gustave::Update_SkillPanelFacialMorphPreview(float timeDelta, bool enabled)
{
    auto* head = dynamic_cast<Head_Player*>(Find_PartObject(TEXT("Part_Head")));
    if (head == nullptr)
        return;

    if (!enabled)
    {
        if (_debugFacialMorphEnabled)
        {
            Stop_DebugFacialMorphTest(head);
            LOG_INFO("[GustaveFacialTest] Stop SkillPanel GameMenu morph curveclip");
        }
        return;
    }

    if (!_debugFacialMorphEnabled)
    {
        if (!Ensure_DebugFacialMorphTest(head))
            return;

        _debugFacialMorphEnabled = true;
        LOG_INFO("[GustaveFacialTest] Play SkillPanel GameMenu morph curveclip");
    }

    const Shared<ModelCom> headModel = head->Get_Model();
    if (headModel == nullptr || _debugFacialMorphModifier == nullptr)
        return;

    _debugFacialMorphModifier->Evaluate_Modifier(headModel.get(), timeDelta);
    Apply_DebugFacialEyeLook(head, timeDelta);
    CHECK_FAILED(headModel->Apply_MorphWeightsToCpuVertices(), );
}

bool PW_Gustave::Ensure_DebugFacialMorphTest(Head_Player* head)
{
    if (head == nullptr)
        return false;

    const Shared<ModelCom> headModel = head->Get_Model();
    if (headModel == nullptr)
        return false;

    if (!headModel->Has_MorphData())
    {
        LOG_WARN("[GustaveFacialTest] Head model has no morph data");
        return false;
    }

    if (_debugFacialMorphModifier != nullptr)
    {
        _debugFacialMorphModifier->Set_Active(true);
        _debugFacialMorphModifier->Set_PlayTimeline(true);
        _debugFacialMorphModifier->Set_LoopTimeline(true);
        _debugFacialMorphModifier->Set_PoseWeight(kDebugFacialMorphStrength);
        _debugFacialMorphModifier->Set_AllowWeightOvershoot(false);
        _debugFacialMorphModifier->Set_TimelineTimeSec(0.f);
        _debugFacialTimelineTimeSec = 0.f;
        return true;
    }

    const fs::path clipPath = fs::path(GAME->Get_AssetRoot()) / kDebugFacialCurveClipRelPath;
    auto clip = CurveClip::Create();
    if (clip == nullptr || FAILED(clip->Load_Binary(String::ToString(clipPath.wstring()))))
    {
        LOG_WARN("[GustaveFacialTest] Failed to load curveclip: {}", String::ToString(clipPath.wstring()));
        return false;
    }

    CurveAnimationModifier::CurveModifierDesc desc{};
    desc.clip = clip;
    desc.poseWeight = kDebugFacialMorphStrength;
    desc.allowWeightOvershoot = false;
    desc.playTimeline = true;
    desc.loopTimeline = true;

    _debugFacialMorphModifier = CurveAnimationModifier::Create(&desc);
    if (_debugFacialMorphModifier == nullptr)
        return false;

    _debugFacialCurveClip = clip;
    _debugFacialMorphModifier->Set_TimelineTimeSec(0.f);
    _debugFacialTimelineTimeSec = 0.f;
    LOG_INFO("[GustaveFacialTest] Loaded curveclip | duration={} | poses={} | morphTargets={} | strength={}",
        clip->Get_DurationSec(),
        clip->Get_Poses().size(),
        headModel->Get_MorphTargetCount(),
        kDebugFacialMorphStrength);
    return true;
}

void PW_Gustave::Apply_DebugFacialEyeLook(Head_Player* head, float timeDelta)
{
    if (head == nullptr || _debugFacialCurveClip == nullptr || !_debugFacialCurveClip->Has_Timeline())
        return;

    const Shared<ModelCom> headModel = head->Get_Model();
    if (headModel == nullptr)
        return;

    if (!_debugFacialEyeBindCaptured)
    {
        _debugFacialEyeBoneL = headModel->Get_BoneIndex("FACIAL_L_Eye");
        _debugFacialEyeBoneR = headModel->Get_BoneIndex("FACIAL_R_Eye");

        if (_debugFacialEyeBoneL < 0 || _debugFacialEyeBoneR < 0)
        {
            LOG_WARN("[GustaveFacialTest] Eye bones missing for curve eye look");
            return;
        }

        _debugFacialEyeBindLocalL = headModel->Get_BoneLocalMatrix(static_cast<uint32>(_debugFacialEyeBoneL));
        _debugFacialEyeBindLocalR = headModel->Get_BoneLocalMatrix(static_cast<uint32>(_debugFacialEyeBoneR));
        _debugFacialEyeBindCaptured = true;
    }

    _debugFacialTimelineTimeSec += std::max(0.f, timeDelta);

    const float durationSec = _debugFacialCurveClip->Get_DurationSec();
    if (durationSec > 0.f && _debugFacialTimelineTimeSec >= durationSec)
        _debugFacialTimelineTimeSec = std::fmod(_debugFacialTimelineTimeSec, durationSec);

    CurveClip::CurveClip_Pose pose{};
    if (!_debugFacialCurveClip->Evaluate_Timeline(_debugFacialTimelineTimeSec, true, pose))
        return;

    Apply_BlinkMorphBoost(headModel.get(), pose);

    const float lookDown = Average_CurvePair(
        pose,
        "CTRL_expressions_eyeLookDownL",
        "CTRL_expressions_eyeLookDownR");
    const float lookLeft = Average_CurvePair(
        pose,
        "head_lod0_mesh__ElookDown_ElookLeft_L",
        "head_lod0_mesh__ElookDown_ElookLeft_R");
    const float lookRight = Average_CurvePair(
        pose,
        "head_lod0_mesh__ElookDown_ElookRight_L",
        "head_lod0_mesh__ElookDown_ElookRight_R");

    const float yawRad = DirectX::XMConvertToRadians((lookRight - lookLeft) * kDebugFacialEyeYawDegrees);
    const float pitchRad = DirectX::XMConvertToRadians(-lookDown * kDebugFacialEyePitchDegrees);
    const Matrix eyeRotation = Matrix::CreateFromYawPitchRoll(yawRad, pitchRad, 0.f);

    headModel->Set_BoneLocalMatrix(static_cast<uint32>(_debugFacialEyeBoneL), eyeRotation * _debugFacialEyeBindLocalL);
    headModel->Set_BoneLocalMatrix(static_cast<uint32>(_debugFacialEyeBoneR), eyeRotation * _debugFacialEyeBindLocalR);
    headModel->Rebuild_BoneCombinedMatrices();
}

void PW_Gustave::Reset_DebugFacialEyeLook(Head_Player* head)
{
    if (head == nullptr || !_debugFacialEyeBindCaptured)
        return;

    const Shared<ModelCom> headModel = head->Get_Model();
    if (headModel == nullptr)
        return;

    if (_debugFacialEyeBoneL >= 0)
        headModel->Set_BoneLocalMatrix(static_cast<uint32>(_debugFacialEyeBoneL), _debugFacialEyeBindLocalL);

    if (_debugFacialEyeBoneR >= 0)
        headModel->Set_BoneLocalMatrix(static_cast<uint32>(_debugFacialEyeBoneR), _debugFacialEyeBindLocalR);

    headModel->Rebuild_BoneCombinedMatrices();
    _debugFacialTimelineTimeSec = 0.f;
    _debugFacialEyeBoneL = -1;
    _debugFacialEyeBoneR = -1;
    _debugFacialEyeBindCaptured = false;
}

void PW_Gustave::Stop_DebugFacialMorphTest(Head_Player* head)
{
    _debugFacialMorphEnabled = false;

    if (_debugFacialMorphModifier != nullptr)
    {
        _debugFacialMorphModifier->Set_Active(false);
        _debugFacialMorphModifier->Set_TimelineTimeSec(0.f);
    }

    Reset_DebugFacialEyeLook(head);

    if (head == nullptr)
        return;

    const Shared<ModelCom> headModel = head->Get_Model();
    if (headModel == nullptr)
        return;

    headModel->Reset_MorphWeights();
    CHECK_FAILED(headModel->Apply_MorphWeightsToCpuVertices(), );
}

HRESULT PW_Gustave::Ready_Modifier()
{
    if (auto body = dynamic_cast<Body_Player*>(Find_PartObject(TEXT("Part_Body"))))
    {
        OffsetAnimationModifier::OffsetModifierDesc aimOffsetDesc{};
        aimOffsetDesc.baseModelCom = body->Get_Model().get();
        aimOffsetDesc.animName = "Gustave_IdleFreeAim_LoopingWorld_Sweep";
        aimOffsetDesc.offsetBoneRanges = { { "spine_01", "hand_r" } };
        aimOffsetDesc.blendMode = AnimationModifier::ModifierBlendMode::ModifyAddictive;
        aimOffsetDesc.blendInTime = 0.75f;
        aimOffsetDesc.blendOutTime = 0.75f;
        aimOffsetDesc.responseCurve = 1.5f;
        aimOffsetDesc.modifierTag = L"AimOffset";
        if (auto modifier = OffsetAnimationModifier::Create(&aimOffsetDesc))
        {
            modifier->Set_Active(false);
            aimOffsetDesc.baseModelCom->Add_AnimModifier(modifier);
        }
        else return E_FAIL;

        OffsetAnimationModifier::OffsetModifierDesc wOffsetDesc{};
        wOffsetDesc.baseModelCom = body->Get_Model().get();
        wOffsetDesc.animName = "Gustave_Walk_F";
        wOffsetDesc.offsetBoneRanges = {
            {"pelvis", "ball_l"},  
            {"pelvis", "ball_r"},
        };
        wOffsetDesc.translationOnlyBones = { "pelvis" };
        wOffsetDesc.blendMode = AnimationModifier::ModifierBlendMode::ModifyReplace;
        wOffsetDesc.blendInTime = 0.25f;
        wOffsetDesc.blendOutTime = 0.25f;
        wOffsetDesc.responseCurve = 1.5f;
        wOffsetDesc.isDynamicPlay = true;
        wOffsetDesc.playRate = 1.f;
        wOffsetDesc.modifierTag = L"Walk_F";
        if (auto modifier = OffsetAnimationModifier::Create(&wOffsetDesc))
        {
            modifier->Set_Active(false);
            aimOffsetDesc.baseModelCom->Add_AnimModifier(modifier);
        }
        else return E_FAIL;

        wOffsetDesc.animName = "Gustave_Walk_B";
        wOffsetDesc.modifierTag = L"Walk_B";
        if (auto modifier = OffsetAnimationModifier::Create(&wOffsetDesc))
        {
            modifier->Set_Active(false);
            aimOffsetDesc.baseModelCom->Add_AnimModifier(modifier);
        }
        else return E_FAIL;

        wOffsetDesc.animName = "Gustave_Walk_LF";
        wOffsetDesc.modifierTag = L"Walk_L";
        if (auto modifier = OffsetAnimationModifier::Create(&wOffsetDesc))
        {
            modifier->Set_Active(false);
            wOffsetDesc.baseModelCom->Add_AnimModifier(modifier);
        }
        else return E_FAIL;

        wOffsetDesc.animName = "Gustave_Walk_RF";
        wOffsetDesc.modifierTag = L"Walk_R";
        if (auto modifier = OffsetAnimationModifier::Create(&wOffsetDesc))
        {
            modifier->Set_Active(false);
            wOffsetDesc.baseModelCom->Add_AnimModifier(modifier);
        }
        else return E_FAIL;
        
        wOffsetDesc.animName = "Gustave_Walk_LB";
        wOffsetDesc.modifierTag = L"Walk_LB";
        if (auto modifier = OffsetAnimationModifier::Create(&wOffsetDesc))
        {
            modifier->Set_Active(false);
            wOffsetDesc.baseModelCom->Add_AnimModifier(modifier);
        }
        else return E_FAIL;

        wOffsetDesc.animName = "Gustave_Walk_RB";
        wOffsetDesc.modifierTag = L"Walk_RB";
        if (auto modifier = OffsetAnimationModifier::Create(&wOffsetDesc))
        {
            modifier->Set_Active(false);
            wOffsetDesc.baseModelCom->Add_AnimModifier(modifier);
        }
        else return E_FAIL;
    }
    else
        return E_FAIL;

    return S_OK;
}

HairChainPhysicsDesc PW_Gustave::Get_HairChainPhysicsDesc() const
{
    HairChainPhysicsDesc desc{};

    desc.profile = HairPhysicsProfile::Gustave;
    desc.bonePrefix = "Strand_";

    // Gustave: 짧은 머리 체인이 많아서 과하게 찰랑이면 어색함.
    desc.stiffness = 0.065f;
    desc.damping = 0.55f;
    desc.windScale = 0.04f;
    desc.gravity = Vec3(0.f, -0.5f, 0.f);

    desc.movementInertiaScale = 0.065f;
    desc.maxMovementInertia = 0.9f;
    desc.maxMovementOffset = 0.02f;

    desc.poseRestoreStrength = 0.26f;
    desc.maxBendAngleDegree = 18.f;

    desc.teleportDistance = 300.f;
    desc.solverIterations = 2;

    return desc;
}

Shared<PW_Gustave> PW_Gustave::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
	auto instance = make_shared<PW_Gustave>(device, context);

	if (FAILED(instance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : PW_Gustave");
		return nullptr;
	}
	return instance;
}

Shared<GameObject> PW_Gustave::Clone(void* arg)
{
	auto instance = make_shared<PW_Gustave>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Cloned : PW_Gustave");
		return nullptr;
	}
	return instance;
}

void PW_Gustave::Free()
{
	__super::Free();
        
}




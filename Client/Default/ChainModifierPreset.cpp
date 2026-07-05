#include "pch.h"
#include "ChainModifierPreset.h"

NS_BEGIN(Client)

namespace
{
    const Vec3 kCommonGravity{ 0.f, -780.f, 0.f };
    const Vec3 kCommonWindDirection{ Vec3{1.f, 0.f, 1.f} };
    constexpr float kScarfWindIntensity{ 0.45f };
    constexpr float kArmbandWindIntensity{ 0.12f };
    constexpr float kMonsterClothWindIntensity{ 0.00075f };

    Vec3 ConvertUePointToEngine(const Vec3& uePoint)
    {
        return Vec3(uePoint.x, uePoint.z, -uePoint.y);
    }

    Quaternion ConvertUeRotationToEngine(const Vec3& ueRotationDegrees)
    {
        const Quaternion ueRotation = Quaternion::CreateFromYawPitchRoll(
            XMConvertToRadians(ueRotationDegrees.y),
            XMConvertToRadians(ueRotationDegrees.x),
            XMConvertToRadians(ueRotationDegrees.z));

        const Matrix basisToEngine = Matrix::CreateRotationX(-XM_PIDIV2);
        const Matrix basisToUe = Matrix::CreateRotationX(XM_PIDIV2);
        const Matrix ueRotationMatrix = Matrix::CreateFromQuaternion(ueRotation);
        const Matrix engineRotationMatrix = basisToEngine * ueRotationMatrix * basisToUe;

        Vec3 scale = Vec3::One;
        Quaternion rotation = Quaternion::Identity;
        Vec3 translation = Vec3::Zero;
        Matrix temp = engineRotationMatrix;
        temp.Decompose(scale, rotation, translation);
        return rotation;
    }

    ChainModifier::CapsuleColliderDesc MakeSourceCapsule(
        const char* boneName,
        const Vec3& ueOffsetLocation,
        const Vec3& ueOffsetRotationDegrees,
        float radius,
        float length)
    {
        ChainModifier::CapsuleColliderDesc desc{};
        desc.boneName = boneName;
        desc.center = ConvertUePointToEngine(ueOffsetLocation);
        desc.rotationQuat = ConvertUeRotationToEngine(ueOffsetRotationDegrees);
        desc.radius = radius;
        desc.halfLength = std::max(0.f, length * 0.5f - radius);
        return desc;
    }

    ChainModifier::CapsuleColliderDesc MakeSourceSphere(
        const char* boneName,
        const Vec3& ueOffsetLocation,
        float radius)
    {
        ChainModifier::CapsuleColliderDesc desc{};
        desc.boneName = boneName;
        desc.center = ConvertUePointToEngine(ueOffsetLocation);
        desc.rotationQuat = Quaternion::Identity;
        desc.radius = radius;
        desc.halfLength = 0.f;
        return desc;
    }

    wstring GetPhysicsAssetPath(PlayerCharacterType characterType)
    {
        switch (characterType)
        {
        case PlayerCharacterType::Maelle:
            return L"../Bin/Resources/ModelPhysics/SK_Maelle_Placeholder_Physics.json";
        case PlayerCharacterType::Lune:
            return L"../Bin/Resources/ModelPhysics/SK_Lune_Placeholder_Physics.json";
        case PlayerCharacterType::Gustave:
            return L"../Bin/Resources/ModelPhysics/SK_Gustave_Placeholder_Physics.json";
        default:
            return L"";
        }
    }

    ChainModifier::BoneChainGroupDesc MakeBindPoseHairGroup(
        const char* rootBoneName,
        float stiffnessMul,
        float dampingMul,
        float movementInertiaMul,
        float poseRestoreMul,
        float maxBendAngleMul)
    {
        ChainModifier::BoneChainGroupDesc desc{};
        desc.rootBoneName = rootBoneName;
        desc.stiffnessMul = stiffnessMul;
        desc.dampingMul = dampingMul;
        desc.movementInertiaMul = movementInertiaMul;
        desc.poseRestoreMul = poseRestoreMul;
        desc.maxBendAngleMul = maxBendAngleMul;
        desc.gravityMul = 1.f;
        desc.windMul = 0.f;
        return desc;
    }

    vector<string> GetScarfCollisionBones(PlayerCharacterType characterType)
    {
        if (characterType == PlayerCharacterType::Gustave)
        {
            return { "spine_05", "upperarm_r" };
        }

        if (characterType == PlayerCharacterType::Lune)
        {
            return { "spine_05", "clavicle_r", "upperarm_r" };
        }

        if (characterType == PlayerCharacterType::Maelle)
        {
            return {
                "spine_04", "spine_05",
                "upperarm_r", "lowerarm_r", "hand_r",
                "upperarm_l", "lowerarm_l", "hand_l",
                "pelvis"
            };
        }

        return {};
    }

    vector<string> GetCoatCollisionBones(PlayerCharacterType characterType)
    {
        if (characterType == PlayerCharacterType::Gustave)
        {
            return { "thigh_l", "thigh_r", "calf_r", "calf_l" };
        }

        if (characterType == PlayerCharacterType::Lune)
        {
            return {
                "thigh_l", "thigh_r", "calf_r", "calf_l",
                "hand_r", "hand_l",
                "lowerarm_r", "lowerarm_l",
                "upperarm_r", "upperarm_l"
            };
        }

        if (characterType == PlayerCharacterType::Maelle)
        {
            return { "spine_05", "upperarm_r", "pelvis", "thigh_r", "thigh_l", "calf_l", "hand_l" };
        }

        return {};
    }

    vector<ChainModifier::CapsuleColliderDesc> GetSourceCoatCapsules(PlayerCharacterType characterType)
    {
        switch (characterType)
        {
        case PlayerCharacterType::Gustave:
            return {
                MakeSourceSphere("thigh_r", Vec3(0.f, 0.f, 0.f), 5.f),
                MakeSourceCapsule("thigh_r", Vec3(0.f, 0.f, 0.f), Vec3(0.f, 0.f, 0.f), 5.f, 11.25f),
                MakeSourceCapsule("thigh_r", Vec3(0.f, 0.f, 0.f), Vec3(0.f, 0.f, 0.f), 5.f, 10.f),
                MakeSourceCapsule("thigh_l", Vec3(0.f, 0.f, 0.f), Vec3(0.f, 0.f, 0.f), 5.f, 10.f),
                MakeSourceCapsule("thigh_r", Vec3(0.f, 0.f, 0.f), Vec3(0.f, 0.f, 0.f), 6.3217564f, 6.376228f),
                MakeSourceCapsule("thigh_l", Vec3(-28.69498f, 0.f, -0.4f), Vec3(-271.f, 0.f, 0.f), 10.5f, 40.150112f),
                MakeSourceCapsule("thigh_r", Vec3(28.105162f, 0.f, 0.4f), Vec3(-271.f, 0.f, 0.f), 10.5f, 40.150112f),
                MakeSourceCapsule("calf_r", Vec3(24.592226f, 0.f, 0.f), Vec3(-266.34155f, 0.f, 0.f), 9.091599f, 31.18919f),
                MakeSourceCapsule("calf_l", Vec3(-24.664698f, 0.f, 0.f), Vec3(-266.34155f, 0.f, 0.f), 9.091599f, 31.18919f)
            };

        case PlayerCharacterType::Lune:
            return {
                MakeSourceCapsule("thigh_l", Vec3(-15.542742f, -3.f, 0.63393074f), Vec3(79.84995f, -180.f, -180.f), 9.5f, 40.150112f),
                MakeSourceCapsule("thigh_r", Vec3(15.892315f, 3.f, -0.6635702f), Vec3(80.155045f, 179.99998f, 180.f), 9.5f, 40.150112f),
                MakeSourceCapsule("calf_r", Vec3(24.592226f, 0.f, 0.f), Vec3(-266.34155f, 0.f, 0.f), 9.091599f, 31.18919f),
                MakeSourceCapsule("calf_l", Vec3(-24.664698f, 0.f, 0.f), Vec3(-266.34155f, 0.f, 0.f), 9.091599f, 31.18919f),
                MakeSourceCapsule("hand_r", Vec3(-5.f, 0.f, 0.f), Vec3(90.f, 0.f, 0.f), 3.5f, 10.f),
                MakeSourceCapsule("hand_l", Vec3(5.f, 0.f, 0.f), Vec3(90.f, 0.f, 0.f), 3.5f, 10.f),
                MakeSourceCapsule("lowerarm_r", Vec3(-8.f, 0.f, 0.f), Vec3(90.f, 0.f, 0.f), 3.f, 20.f),
                MakeSourceCapsule("lowerarm_l", Vec3(8.f, 0.f, 0.f), Vec3(90.f, 0.f, 0.f), 3.f, 20.f),
                MakeSourceCapsule("upperarm_r", Vec3(-10.f, 0.f, 0.f), Vec3(90.f, 0.f, 0.f), 3.f, 20.f),
                MakeSourceCapsule("upperarm_l", Vec3(10.f, 0.f, 0.f), Vec3(90.f, 0.f, 0.f), 3.f, 20.f)
            };

        case PlayerCharacterType::Maelle:
            return {
                MakeSourceCapsule("spine_05", Vec3(-2.f, 0.f, 0.f), Vec3(0.f, 0.f, 0.f), 10.f, 12.f),
                MakeSourceCapsule("upperarm_r", Vec3(-10.f, 0.f, 0.f), Vec3(90.f, 0.f, 0.f), 4.3f, 20.f),
                MakeSourceCapsule("pelvis", Vec3(-4.f, 0.f, 0.f), Vec3(0.f, 0.f, 0.f), 11.f, 10.f),
                MakeSourceCapsule("thigh_r", Vec3(10.f, 0.f, 0.f), Vec3(90.f, 0.f, 0.f), 7.f, 20.f),
                MakeSourceCapsule("thigh_l", Vec3(-20.f, 0.f, 0.f), Vec3(90.f, 0.f, 0.f), 7.f, 25.f),
                MakeSourceCapsule("calf_l", Vec3(-20.f, 0.f, 0.f), Vec3(90.f, 0.f, 0.f), 7.f, 40.f),
                MakeSourceCapsule("hand_l", Vec3(5.f, 0.f, 0.f), Vec3(90.f, 0.f, 0.f), 1.f, 10.f)
            };

        default:
            return {};
        }
    }
}

void ChainModifierPreset::BuildScarfDesc(
    ChainModifier::ChainModifierDesc& outDesc,
    TransformCom* ownerTransform,
    PlayerCharacterType characterType,
    const wstring& modifierTag)
{
    auto& desc = outDesc;
    desc = {};
    desc.modifierTag = modifierTag;

    const auto MakeHairGroup = [](const char* rootBoneName)
        {
            ChainModifier::BoneChainGroupDesc group{};
            group.rootBoneName = rootBoneName;
            group.maxDepth = 0;
            return group;
        };

    desc.shape.stiffness = 0.9f;
    desc.shape.poseRestoreStrength = 0.88f;
    desc.shape.maxBendAngleDegree = 18.f;
    desc.shape.pinRoot = true;

    desc.motion.damping = 0.35f;
    desc.runtime.gravity = Vec3::Zero;
    desc.motion.movementInertiaScale = 0.f;
    desc.motion.maxMovementOffset = 0.f;
    desc.motion.solverIterations = 4;

    desc.collision.collisionRadiusPadding = 0.f;
    desc.collision.enableCollision = true;
    desc.collision.physicsAssetPath = GetPhysicsAssetPath(characterType);
    desc.collision.includedBones = GetScarfCollisionBones(characterType);
    desc.collision.debugDrawCapsules = false;

    desc.runtime.ownerTransform = ownerTransform;
    desc.runtime.windDirection = kCommonWindDirection;
    desc.runtime.windScale = kScarfWindIntensity;
    desc.blendMode = AnimationModifier::ModifierBlendMode::ModifyAddictive;

    if (characterType == PlayerCharacterType::Maelle)
    {
        desc.collision.collisionRadiusPadding = 0.f;
        desc.groups = {
            { "Maelle_V4_Scarf_01_01" },
            { "Maelle_V4_Scarf_02_01" },
            { "Maelle_V4_Scarf_03_01" },
            { "Maelle_V4_Scarf_04_01" },
            { "Maelle_V4_Scarf_05_01" },
            { "Maelle_V4_Scarf_06_01" }
        };
    }
    else if (characterType == PlayerCharacterType::Lune)
    {
        desc.collision.collisionRadiusPadding = 0.f;
        desc.groups = {
            { "Lune_V2_Scarf_01_01" },
            { "Lune_V2_Scarf_02_01" },
            { "Lune_V2_Scarf_03_01" },
            { "Lune_V2_Scarf_04_01" },
            { "Lune_V2_Scarf_05_01" },
            { "Lune_V2_Scarf_06_01" },
        };
    }
    else if (characterType == PlayerCharacterType::Gustave)
    {
        desc.collision.collisionRadiusPadding = 0.f;
        desc.groups = {
            { "Noah_V2_cloth2" },
            { "Noah_V2_cloth1" },
            { "Noah_V2_cloth3" },
            { "Noah_V2_cloth4" }
        };
        return;
    }
}

void ChainModifierPreset::BuildPoseFollowScarfDesc(
    PoseFollowChainModifier::PoseFollowChainModifierDesc& outDesc,
    TransformCom* ownerTransform,
    PlayerCharacterType characterType,
    const wstring& modifierTag)
{
    auto& desc = outDesc;
    desc = {};
    desc.modifierTag = modifierTag;
    desc.ownerTransform = ownerTransform;

    desc.physicsAssetPath = GetPhysicsAssetPath(characterType);
    desc.includedCollisionBones = GetScarfCollisionBones(characterType);
    desc.particleCollisionRadius = 1.4f;
    desc.collisionRadiusPadding = 0.04f;

    desc.windDirection = kCommonWindDirection;
    desc.windScale = 0.f;
    desc.ownerMotionScale = 0.009f;
    desc.animatedMotionScale = 0.22f;
    desc.animatedMotionDeadzone = 0.09f;
    desc.maxExternalOffset = 1.45f;

    desc.damping = 0.72f;
    desc.tipDampingScale = 1.10f;
    desc.poseFollowStrength = 0.60f;
    desc.tipFollowScale = 0.26f;
    desc.maxBendAngleDegree = 20.f;
    desc.tipBendAngleScale = 2.25f;
    desc.bendLimitStrength = 0.88f;
    desc.rootOutputAlpha = 0.1f;
    desc.tipOutputAlpha = 0.82f;
    desc.idleOutputAlphaScale = 0.22f;
    desc.outputSmoothAlpha = 0.42f;
    desc.idleOutputSmoothAlpha = 0.78f;
    desc.outputDeadzoneDegree = 1.3f;
    desc.idleSettleStrength = 0.90f;
    desc.sleepVelocityThreshold = 0.03f;
    desc.sleepPoseDistance = 0.18f;
    desc.solverIterations = 5;
    desc.targetFrameRate = 45.f;

    if (characterType == PlayerCharacterType::Maelle)
    {
        desc.particleCollisionRadius = 2.2f;
        desc.collisionRadiusPadding = 0.22f;
        desc.windScale = 0.05f;
        desc.animatedMotionScale = 0.26f;
        desc.damping = 0.68f;
        desc.tipDampingScale = 1.06f;
        desc.poseFollowStrength = 0.50f;
        desc.tipFollowScale = 0.18f;
        desc.tipOutputAlpha = 0.90f;
        desc.outputSmoothAlpha = 0.36f;
        desc.targetFrameRate = 60.f;
        desc.groups = {
            { "Maelle_V4_Scarf_01_01", 0.26f, 0.92f, 0.88f },
            { "Maelle_V4_Scarf_02_01", 0.26f, 0.92f, 0.88f },
            { "Maelle_V4_Scarf_03_01", 0.26f, 0.92f, 0.88f },
            { "Maelle_V4_Scarf_04_01", 0.26f, 0.92f, 0.88f },
            { "Maelle_V4_Scarf_05_01", 0.26f, 0.92f, 0.88f },
            { "Maelle_V4_Scarf_06_01", 0.26f, 0.92f, 0.88f }
        };
        return;
    }

    if (characterType == PlayerCharacterType::Lune)
    {
        desc.groups = {
            { "Lune_V2_Scarf_01_01", 0.20f, 0.82f, 0.96f },
            { "Lune_V2_Scarf_02_01", 0.20f, 0.82f, 0.96f },
            { "Lune_V2_Scarf_03_01", 0.20f, 0.82f, 0.96f },
            { "Lune_V2_Scarf_04_01", 0.20f, 0.82f, 0.96f },
            { "Lune_V2_Scarf_05_01", 0.20f, 0.82f, 0.96f },
            { "Lune_V2_Scarf_06_01", 0.20f, 0.82f, 0.96f }
        };
        return;
    }
     
    if (characterType == PlayerCharacterType::Gustave)
    {
        desc.groups = {
            { "Noah_V2_cloth2", 0.20f, 0.82f, 0.96f },
            { "Noah_V2_cloth1", 0.20f, 0.82f, 0.96f },
            { "Noah_V2_cloth3", 0.20f, 0.82f, 0.96f },
            { "Noah_V2_cloth4", 0.20f, 0.82f, 0.96f }
        };
    }
}

void ChainModifierPreset::BuildArmbandDesc(
    ChainModifier::ChainModifierDesc& outDesc,
    TransformCom* ownerTransform,
    PlayerCharacterType characterType,
    const wstring& modifierTag)
{
    auto& desc = outDesc;
    desc = {};
    desc.modifierTag = modifierTag;
   

    if (characterType == PlayerCharacterType::Lune)
    {
        desc.groups = {
            { "Armband_Root" },
            { "Lune_FrontBands_Root"}
        };
    }
    else
    {
        desc.groups = {
            { "Armband_Root" }
        };
    }

    desc.shape.stiffness = 0.f;
    desc.shape.tipStiffnessScale = 0.f;
    desc.shape.poseRestoreStrength = 0.f;
    desc.shape.tipPoseRestoreScale = 0.f;
    desc.shape.maxBendAngleDegree = 0.f;
    desc.shape.tipMaxBendAngleScale = 0.f;
    desc.shape.surfaceConstraintStrength = 0.f;
    desc.shape.rootOutputAlpha = 0.f;
    desc.shape.tipOutputAlpha = 0.825f;
    desc.shape.pinRoot = true;
    for (auto& group : desc.groups)
    {
        group.rootRotationOffsetDegrees = Vec3(9.f, -9.f, 30.f);
    }

    desc.motion.damping = 0.985f;
    desc.motion.tipDampingScale = 2.050f;
    desc.motion.movementInertiaScale = 0.230f;
    desc.motion.ownerMovementInertiaScale = 0.f;
    desc.motion.tipMovementInertiaScale = 1.520f;
    desc.motion.maxMovementOffset = 0.025f;
    desc.motion.solverIterations = 3;

    desc.collision.enableCollision = false;
    desc.collision.particleCollisionRadius = 0.f;
    desc.collision.collisionRadiusPadding = 0.15f;
    desc.collision.physicsAssetPath = GetPhysicsAssetPath(characterType);
    desc.collision.debugDrawCapsules = false;

    desc.runtime.gravity = kCommonGravity;
    desc.runtime.ownerTransform = ownerTransform;
    desc.runtime.resetDistance = 5.f;
    desc.runtime.targetFrameRate = 60.f;
    desc.runtime.windDirection = kCommonWindDirection;
    desc.runtime.windScale = 0.f;
    desc.runtime.windStartRatio = -1.f;
    desc.runtime.windWeightScale = -1.f;
    desc.runtime.constraintDeadzone = 0.f;

    desc.blendMode = AnimationModifier::ModifierBlendMode::ModifyReplace;
    return;
}

void ChainModifierPreset::BuildPoseFollowHairDesc(
    PoseFollowChainModifier::PoseFollowChainModifierDesc& outDesc,
    TransformCom* ownerTransform,
    PlayerCharacterType characterType,
    const wstring& modifierTag)
{
    auto& desc = outDesc;
    desc = {};
    desc.modifierTag = modifierTag;
    desc.ownerTransform = ownerTransform;

    desc.enableCollision = false;
    desc.windDirection = kCommonWindDirection;
    desc.windScale = 0.001f;
    desc.ownerMotionScale = 1000.f;
    desc.animatedMotionScale = 1000.f;
    desc.ownerMotionDeadzone = 0.02f;
    desc.ownerRotationDeadzoneDegree = 0.4f;
    desc.animatedMotionDeadzone = 0.05f;
    desc.maxExternalOffset = 0.42f;

    desc.damping = 0.905f;
    desc.tipDampingScale = 4.010f;
    desc.poseFollowStrength = 0.375f;
    desc.tipFollowScale = 0.430f;
    desc.maxBendAngleDegree = 12.f;
    desc.tipBendAngleScale = 1.45f;
    desc.bendLimitStrength = 1.0f;
    desc.surfaceConstraintStrength = 0.f;
    desc.rootOutputAlpha = 0.020f;
    desc.tipOutputAlpha = 0.580f;
    desc.idleOutputAlphaScale = 0.220f;
    desc.outputSmoothAlpha = 0.640f;
    desc.idleOutputSmoothAlpha = 0.840f;
    desc.outputDeadzoneDegree = 0.400f;
    desc.idleSettleStrength = 0.960f;
    desc.sleepVelocityThreshold = 0.045f;
    desc.sleepPoseDistance = 0.080f;
    desc.solverIterations = 5;
    desc.targetFrameRate = 60.f;

    if (characterType == PlayerCharacterType::Maelle)
    {
        desc.windScale = 0.0012f;
        desc.ownerMotionScale = 3.65f;
        desc.animatedMotionScale = 2.055f;
        desc.maxExternalOffset = 0.35f;

        desc.damping = 0.98f;
        desc.tipDampingScale = 1.f;
        desc.poseFollowStrength = 1.f;
        desc.tipFollowScale = 1.f;
        desc.maxBendAngleDegree = 120.f;
        desc.tipBendAngleScale = 1.f;
        desc.bendLimitStrength = 1.f;
        desc.surfaceConstraintStrength = 0.f;
        desc.rootOutputAlpha = 0.05f;
        desc.tipOutputAlpha = 0.97f;

        desc.groups = {
            { "Strand_0_1", 1.f, 1.f, 1.0f },
            { "Strand_5_1", 1.f, 1.f, 1.0f },
            { "Strand_6_1", 1.f, 1.f, 1.0f }
        };
        return;
    }

    if (characterType == PlayerCharacterType::Lune)
    {
        desc.groups = {
            { "Strand_1_1", 1.f, 1.25f, 1.08f },
            { "Strand_2_1", 1.f, 1.25f, 1.08f },
            { "Strand_3_1", 1.f, 1.25f, 1.08f },
            { "Strand_4_1", 1.f, 1.25f, 1.08f },
            { "Strand_5_1", 1.f, 1.20f, 1.08f },
            { "Strand_6_1", 1.f, 1.20f, 1.08f },
            { "Strand_7_1", 1.f, 1.05f, 1.05f },
            { "Strand_8_1", 1.f, 1.05f, 1.05f },
            { "Strand_9_1", 1.f, 1.05f, 1.05f },
            { "Strand_10_1", 1.f, 0.25f, 1.05f },
            { "Strand_11_1", 1.f, 0.25f, 1.05f },
            { "Strand_12_1", 1.f, 0.25f, 1.05f },
            { "Strand_13_1", 1.f, 0.25f, 1.05f },
            { "Strand_14_1", 1.f, 0.25f, 1.05f }
        };
        return;
    }

    if (characterType == PlayerCharacterType::Gustave)
    {
        desc.groups = {
            { "Strand_1_1", 1.f, 1.25f, 1.08f },
            { "Strand_2_1", 1.f, 1.25f, 1.08f },
            { "Strand_3_1", 1.f, 1.25f, 1.08f },
            { "Strand_4_1", 1.f, 1.25f, 1.08f },
            { "Strand_5_1", 1.f, 1.20f, 1.08f },
            { "Strand_6_1", 1.f, 1.20f, 1.08f },
            { "Strand_7_1", 1.f, 1.05f, 1.05f },
            { "Strand_8_1", 1.f, 1.05f, 1.05f },
            { "Strand_9_1", 1.f, 1.05f, 1.05f },
            { "Strand_10_1", 1.f, 0.25f, 1.05f },
            { "Strand_11_1", 1.f, 0.25f, 1.05f }
        };
    }
}

void ChainModifierPreset::BuildBindPoseHairDesc(
    ChainModifier::ChainModifierDesc& outDesc,
    TransformCom* ownerTransform,
    PlayerCharacterType characterType,
    const wstring& modifierTag)
{
    if (characterType == PlayerCharacterType::Gustave)
        return;

    auto& desc = outDesc;
    desc = {};
    desc.modifierTag = modifierTag;

    desc.shape.stiffness = 0.80f;
    desc.shape.tipStiffnessScale = 1.f;
    desc.shape.poseRestoreStrength = 0.18f;
    desc.shape.tipPoseRestoreScale = 1.f;
    desc.shape.maxBendAngleDegree = 28.f;
    desc.shape.tipMaxBendAngleScale = 1.f;
    desc.shape.surfaceConstraintStrength = 0.f;
    desc.shape.rootOutputAlpha = 0.f;
    desc.shape.tipOutputAlpha = 1.f;
    desc.shape.pinRoot = true;
    desc.shape.useBindPoseAsRest = true;

    desc.motion.damping = 0.58f;
    desc.motion.tipDampingScale = 1.f;
    desc.motion.movementInertiaScale = 0.120f;
    desc.motion.ownerMovementInertiaScale = 0.f;
    desc.motion.tipMovementInertiaScale = 1.f;
    desc.motion.maxMovementOffset = 0.035f;
    desc.motion.solverIterations = 3;

    desc.collision.enableCollision = false;
    desc.collision.particleCollisionRadius = 0.f;
    desc.collision.collisionRadiusPadding = 0.f;
    desc.collision.debugDrawCapsules = false;

    desc.runtime.ownerTransform = ownerTransform;
    desc.runtime.gravity = Vec3(0.f, -0.45f, 0.f);
    desc.runtime.windDirection = kCommonWindDirection;
    desc.runtime.windScale = 0.f;
    desc.runtime.windStartRatio = -1.f;
    desc.runtime.windWeightScale = -1.f;
    desc.runtime.constraintDeadzone = 0.010f;
    desc.runtime.resetDistance = 3.f;
    desc.runtime.targetFrameRate = 60.f;

    desc.blendMode = AnimationModifier::ModifierBlendMode::ModifyReplace;

    if (characterType == PlayerCharacterType::Maelle)
    {
        desc.groups = {
            MakeBindPoseHairGroup("Strand_0_1", 0.82f, 1.060f, 2.10f, 0.68f, 1.05f),
            MakeBindPoseHairGroup("Strand_5_1", 1.025f, 1.010f, 1.25f, 1.05f, 0.90f),
            MakeBindPoseHairGroup("Strand_6_1", 1.025f, 1.010f, 1.25f, 1.05f, 0.90f)
        };

        desc.shape.stiffness = 0.625f;
        desc.shape.tipStiffnessScale = 0.430f;
        desc.shape.poseRestoreStrength = 0.100f;
        desc.shape.tipPoseRestoreScale = 1.000f;
        desc.shape.maxBendAngleDegree = 39.750f;
        desc.shape.tipMaxBendAngleScale = 1.000f;
        desc.shape.rootOutputAlpha = 0.120f;
        desc.shape.tipOutputAlpha = 1.f;
        desc.shape.surfaceConstraintStrength = 0.f;
        desc.shape.pinRoot = true;

        desc.motion.damping = 0.815f;
        desc.motion.tipDampingScale = 1.220f;
        desc.motion.movementInertiaScale = 0.140f;
        desc.motion.ownerMovementInertiaScale = 0.690f;
        desc.motion.tipMovementInertiaScale = 1.000f;
        desc.motion.maxMovementOffset = 0.058f;
        desc.motion.solverIterations = 3;

        desc.runtime.gravity = Vec3(0.f, -3.100f, 0.f);
        desc.runtime.windDirection = kCommonWindDirection;
        desc.runtime.windScale = 1.260f;
        desc.runtime.windStartRatio = -1.f;
        desc.runtime.windWeightScale = 4.280f;
        desc.runtime.constraintDeadzone = 0.010f;
        desc.runtime.resetDistance = 3.f;
        desc.runtime.targetFrameRate = 60.f;
        return;
    }

    if (characterType == PlayerCharacterType::Lune)
    {
        desc.groups = {
            MakeBindPoseHairGroup("Strand_1_1", 1.15f, 0.942f, 0.65f, 1.35f, 0.75f),
            MakeBindPoseHairGroup("Strand_2_1", 1.15f, 0.942f, 0.65f, 1.35f, 0.75f),
            MakeBindPoseHairGroup("Strand_3_1", 1.00f, 1.000f, 0.90f, 1.05f, 0.95f),
            MakeBindPoseHairGroup("Strand_4_1", 1.00f, 1.000f, 0.90f, 1.05f, 0.95f),
            MakeBindPoseHairGroup("Strand_5_1", 1.20f, 0.870f, 0.42f, 1.35f, 0.55f),
            MakeBindPoseHairGroup("Strand_6_1", 1.20f, 0.870f, 0.42f, 1.35f, 0.55f),
            MakeBindPoseHairGroup("Strand_7_1", 1.20f, 0.870f, 0.42f, 1.35f, 0.55f),
            MakeBindPoseHairGroup("Strand_8_1", 1.20f, 0.870f, 0.42f, 1.35f, 0.55f),
            MakeBindPoseHairGroup("Strand_9_1", 1.20f, 0.870f, 0.42f, 1.35f, 0.55f),
            MakeBindPoseHairGroup("Strand_10_1", 1.20f, 0.870f, 0.42f, 1.35f, 0.55f),
            MakeBindPoseHairGroup("Strand_11_1", 1.20f, 0.870f, 0.42f, 1.35f, 0.55f),
            MakeBindPoseHairGroup("Strand_12_1", 1.15f, 0.942f, 0.65f, 1.35f, 0.75f),
            MakeBindPoseHairGroup("Strand_13_1", 1.15f, 0.942f, 0.65f, 1.35f, 0.75f),
            MakeBindPoseHairGroup("Strand_14_1", 1.15f, 0.942f, 0.65f, 1.35f, 0.75f)
        };

        desc.shape.stiffness = 0.805f;
        desc.shape.tipStiffnessScale = 1.000f;
        desc.shape.poseRestoreStrength = 0.020f;
        desc.shape.tipPoseRestoreScale = 0.540f;
        desc.shape.maxBendAngleDegree = 28.f;
        desc.shape.tipMaxBendAngleScale = 1.000f;
        desc.shape.surfaceConstraintStrength = 0.f;
        desc.shape.rootOutputAlpha = 0.005f;
        desc.shape.tipOutputAlpha = 1.f;
        desc.shape.pinRoot = false;

        desc.motion.damping = 0.830f;
        desc.motion.tipDampingScale = 1.420f;
        desc.motion.movementInertiaScale = 0.125f;
        desc.motion.ownerMovementInertiaScale = 0.875f;
        desc.motion.tipMovementInertiaScale = 1.220f;
        desc.motion.maxMovementOffset = 0.018f;
        desc.runtime.gravity = Vec3(0.f, -0.45f, 0.f);
        desc.runtime.windDirection = kCommonWindDirection;
        desc.runtime.windScale = 0.f;
        desc.runtime.windStartRatio = -1.f;
        desc.runtime.windWeightScale = -1.f;
        desc.runtime.constraintDeadzone = 0.010f;
        desc.runtime.resetDistance = 3.f;
        desc.runtime.targetFrameRate = 60.f;

        desc.collision.enableCollision = false;
        desc.collision.particleCollisionRadius = 0.f;
        return;
    }

    if (characterType == PlayerCharacterType::Gustave)
    {
        desc.groups = {
            MakeBindPoseHairGroup("Strand_1_1", 1.00f, 0.853f, 0.55f, 1.25f, 0.75f),
            MakeBindPoseHairGroup("Strand_2_1", 1.00f, 0.853f, 0.55f, 1.25f, 0.75f),
            MakeBindPoseHairGroup("Strand_3_1", 1.00f, 0.902f, 0.70f, 1.18f, 0.85f),
            MakeBindPoseHairGroup("Strand_4_1", 1.00f, 0.902f, 0.70f, 1.18f, 0.85f),
            MakeBindPoseHairGroup("Strand_5_1", 1.00f, 0.902f, 0.70f, 1.18f, 0.85f),
            MakeBindPoseHairGroup("Strand_6_1", 1.00f, 0.902f, 0.70f, 1.18f, 0.85f),
            MakeBindPoseHairGroup("Strand_7_1", 1.00f, 0.939f, 0.80f, 1.10f, 0.90f),
            MakeBindPoseHairGroup("Strand_8_1", 1.00f, 0.939f, 0.80f, 1.10f, 0.90f),
            MakeBindPoseHairGroup("Strand_9_1", 1.00f, 0.939f, 0.80f, 1.10f, 0.90f),
            MakeBindPoseHairGroup("Strand_10_1", 1.00f, 0.853f, 0.55f, 1.25f, 0.75f),
            MakeBindPoseHairGroup("Strand_11_1", 1.00f, 0.853f, 0.55f, 1.25f, 0.75f)
        };

        desc.shape.stiffness = 1.f;
        desc.shape.poseRestoreStrength = 0.26f;
        desc.shape.maxBendAngleDegree = 18.f;
        desc.motion.damping = 0.45f;
        desc.motion.movementInertiaScale = 0.065f;
        desc.motion.maxMovementOffset = 0.020f;
        desc.motion.solverIterations = 3;
        desc.runtime.gravity = Vec3(0.f, -0.5f, 0.f);
    }
}

void ChainModifierPreset::BuildMaelleHairChainDesc(
    ChainModifier::ChainModifierDesc& outDesc,
    TransformCom* ownerTransform,
    const wstring& modifierTag)
{
    auto& desc = outDesc;
    desc = {};
    desc.modifierTag = modifierTag;

    desc.groups = {
        { "Strand_0_1" },
        { "Strand_5_1" },
        { "Strand_6_1" }
    };

    desc.shape.stiffness = 1.0f;
    desc.shape.tipStiffnessScale = 1.0f;
    desc.shape.poseRestoreStrength = 0.f;
    desc.shape.tipPoseRestoreScale = 1.0f;
    desc.shape.maxBendAngleDegree = 120.f;
    desc.shape.tipMaxBendAngleScale = 1.0f;
    desc.shape.surfaceConstraintStrength = 0.f;
    desc.shape.rootOutputAlpha = 0.05f;
    desc.shape.tipOutputAlpha = 0.97f;
    desc.shape.pinRoot = true;

    desc.motion.damping = 0.98f;
    desc.motion.tipDampingScale = 1.f;
    desc.motion.movementInertiaScale = 2.055f;
    desc.motion.ownerMovementInertiaScale = 3.65f;
    desc.motion.tipMovementInertiaScale = 2.8f;
    desc.motion.maxMovementOffset = 0.35f;
    desc.motion.solverIterations = 2;

    desc.runtime.ownerTransform = ownerTransform;
    desc.runtime.gravity = kCommonGravity;
    desc.runtime.windDirection = kCommonWindDirection;
    desc.runtime.windScale = 0.0012f;
    desc.runtime.constraintDeadzone = 0.025f;

    desc.collision.enableCollision = false;
    desc.collision.particleCollisionRadius = 0.f;
    desc.collision.collisionRadiusPadding = 1.f;
    desc.collision.debugDrawCapsules = false;

    desc.blendMode = AnimationModifier::ModifierBlendMode::ModifyReplace;
}

void ChainModifierPreset::BuildAbbestClothDesc(
    ChainModifier::ChainModifierDesc& outDesc,
    TransformCom* ownerTransform,
    PlayerCharacterType characterType,
    const wstring& modifierTag)
{
    auto& desc = outDesc;
    desc = {};
    desc.modifierTag = modifierTag;

    desc.groups = {
        { "Abbest_Cloth_01_01" },
        { "Abbest_Cloth_02_01" },
        { "Abbest_Cloth_03_01" },
        { "Abbest_Cloth_04_01" }
    };

    desc.shape.stiffness = 1.0f;
    desc.shape.tipStiffnessScale = 0.62f;
    desc.shape.poseRestoreStrength = 0.006f;
    desc.shape.tipPoseRestoreScale = 0.28f;
    desc.shape.maxBendAngleDegree = 105.f;
    desc.shape.tipMaxBendAngleScale = 1.22f;
    desc.shape.rootOutputAlpha = 0.12f;
    desc.shape.tipOutputAlpha = 0.92f;
    desc.shape.pinRoot = true;
    desc.motion.damping = 0.965f;
    desc.motion.tipDampingScale = 1.08f;
    desc.motion.movementInertiaScale = 0.145f;
    desc.motion.ownerMovementInertiaScale = 0.10f;
    desc.motion.tipMovementInertiaScale = 2.8f;
    desc.motion.maxMovementOffset = 0.042f;
    desc.motion.solverIterations = 2;

    desc.runtime.ownerTransform = ownerTransform;
    desc.runtime.gravity = kCommonGravity;
    desc.runtime.windDirection = kCommonWindDirection;
    desc.runtime.windScale = kMonsterClothWindIntensity;
    desc.runtime.windStartRatio = 0.05f;
    desc.runtime.windWeightScale = 0.9f;

    desc.blendMode = AnimationModifier::ModifierBlendMode::ModifyReplace;
    return;
}

void ChainModifierPreset::BuildBishopClothDesc(
    ChainModifier::ChainModifierDesc& outDesc,
    TransformCom* ownerTransform,
    PlayerCharacterType characterType,
    const wstring& modifierTag)
{
    auto& desc = outDesc;
    desc = {};
    desc.modifierTag = modifierTag;

    desc.groups = {
        { "Eveque_Top_Cloth_BL_01_01" },
        { "Eveque_Top_Cloth_BL_02_01" },
        { "Eveque_Top_Cloth_FL_01_01" },
        { "Eveque_Top_Cloth_FL_02_01" },
        
        { "Eveque_Top_Cloth_BR_01_01" },
        { "Eveque_Top_Cloth_BR_02_01" },
        { "Eveque_Top_Cloth_FR_01_01" },
        { "Eveque_Top_Cloth_FR_02_01" }
    };

    desc.shape.stiffness = 1.0f;
    desc.shape.tipStiffnessScale = 1.0f;
    desc.shape.poseRestoreStrength = 0.f;
    desc.shape.tipPoseRestoreScale = 1.0f;
    desc.shape.maxBendAngleDegree = 120.f;
    desc.shape.tipMaxBendAngleScale = 1.0f;
    desc.shape.surfaceConstraintStrength = 0.f;
    desc.shape.rootOutputAlpha = 0.05f;
    desc.shape.tipOutputAlpha = 0.97f;
    desc.shape.pinRoot = true;

    desc.motion.damping = 0.98f;
    desc.motion.tipDampingScale = 1.f;
    desc.motion.movementInertiaScale = 2.055f;
    desc.motion.ownerMovementInertiaScale = 3.65f;
    desc.motion.tipMovementInertiaScale = 2.8f;
    desc.motion.maxMovementOffset = 0.35f;
    desc.motion.solverIterations = 2;

    desc.runtime.ownerTransform = ownerTransform;
    desc.runtime.gravity = kCommonGravity;
    desc.runtime.windDirection = kCommonWindDirection;
    desc.runtime.windScale = 0.0012f;
    desc.runtime.constraintDeadzone = 0.025f;
    desc.collision.enableCollision = true;
    desc.collision.particleCollisionRadius = 0.f;
    desc.collision.collisionRadiusPadding = 1.f;
    desc.collision.debugDrawCapsules = false;

    desc.blendMode = AnimationModifier::ModifierBlendMode::ModifyReplace;
    return;
}

void ChainModifierPreset::BuildBishopLegClothDesc(
    ChainModifier::ChainModifierDesc& outDesc,
    TransformCom* ownerTransform,
    PlayerCharacterType characterType,
    const wstring& modifierTag)
{
    auto& desc = outDesc;
    desc = {};
    desc.modifierTag = modifierTag;

    desc.groups = {
        { "Eveque_Leg_Cloth_F_01" },

        { "Eveque_Leg_Cloth_l_01_01" },
        { "Eveque_Leg_Cloth_l_02_01" },
        { "Eveque_Leg_Cloth_l_03_01" },

        { "Eveque_Leg_Cloth_r_01_01" },
        { "Eveque_Leg_Cloth_r_02_01" },
        { "Eveque_Leg_Cloth_r_03_01" }
    };

    desc.shape.stiffness = 1.0f;
    desc.shape.tipStiffnessScale = 1.0f;
    desc.shape.poseRestoreStrength = 0.f;
    desc.shape.tipPoseRestoreScale = 1.0f;
    desc.shape.maxBendAngleDegree = 120.f;
    desc.shape.tipMaxBendAngleScale = 1.0f;
    desc.shape.surfaceConstraintStrength = 0.f;
    desc.shape.rootOutputAlpha = 0.05f;
    desc.shape.tipOutputAlpha = 0.97f;
    desc.shape.pinRoot = true;

    desc.motion.damping = 0.98f;
    desc.motion.tipDampingScale = 1.f;
    desc.runtime.gravity = kCommonGravity;
    desc.motion.movementInertiaScale = 2.055f;
    desc.motion.ownerMovementInertiaScale = 3.65f;
    desc.motion.tipMovementInertiaScale = 2.8f;
    desc.motion.maxMovementOffset = 0.35f;
    desc.motion.solverIterations = 2;

    desc.runtime.ownerTransform = ownerTransform;
    desc.runtime.windDirection = kCommonWindDirection;
    desc.runtime.windScale = 0.0012f;
    desc.runtime.constraintDeadzone = 0.025f;
    desc.collision.enableCollision = true;
    desc.collision.particleCollisionRadius = 0.f;
    desc.collision.collisionRadiusPadding = 1.f;
    desc.collision.debugDrawCapsules = false;

    desc.blendMode = AnimationModifier::ModifierBlendMode::ModifyReplace;
    return;
}

void ChainModifierPreset::BuildPotatoBagHairDesc(
    ChainModifier::ChainModifierDesc& outDesc,
    TransformCom* ownerTransform,
    PlayerCharacterType characterType,
    const wstring& modifierTag)
{
    UNREFERENCED_PARAMETER(characterType);

    auto& desc = outDesc;
    desc = {};
    desc.modifierTag = modifierTag;

    desc.groups = {
        { "Head_Custom_Bones_00_00" },
        { "Head_Custom_Bones_01_00" },
        { "Head_Custom_Bones_02_00" },
        { "Head_Custom_Bones_03_00" }
    };

    desc.shape.stiffness = 0.935f;
    desc.shape.tipStiffnessScale = 1.150f;
    desc.shape.poseRestoreStrength = 0.070f;
    desc.shape.tipPoseRestoreScale = 1.110f;
    desc.shape.maxBendAngleDegree = 80.f;
    desc.shape.tipMaxBendAngleScale = 1.0f;
    desc.shape.surfaceConstraintStrength = 0.f;
    desc.shape.rootOutputAlpha = 0.f;
    desc.shape.tipOutputAlpha = 0.97f;
    desc.shape.pinRoot = true;

    desc.motion.damping = 0.985f;
    desc.motion.tipDampingScale = 0.965f;
    desc.motion.movementInertiaScale = 0.770f;
    desc.motion.ownerMovementInertiaScale = 0.850f;
    desc.motion.tipMovementInertiaScale = 1.740f;
    desc.motion.maxMovementOffset = 0.082f;
    desc.motion.solverIterations = 2;

    desc.runtime.ownerTransform = ownerTransform;
    desc.runtime.gravity = Vec3(0.f, -50.f, 0.f);
    desc.runtime.windDirection = kCommonWindDirection;
    desc.runtime.windScale = 0.001f;
    desc.runtime.windStartRatio = -1.f;
    desc.runtime.windWeightScale = -1.f;
    desc.runtime.constraintDeadzone = 0.010f;
    desc.runtime.resetDistance = 3.f;
    desc.runtime.targetFrameRate = 0.f;
    desc.collision.enableCollision = false;
    desc.collision.particleCollisionRadius = 0.f;
    desc.collision.collisionRadiusPadding = 1.f;
    desc.collision.debugDrawCapsules = false;

    desc.blendMode = AnimationModifier::ModifierBlendMode::ModifyReplace;
    return;
}

void ChainModifierPreset::BuildSimonHairPhysicsDesc(
    PhysicsModifier::PhysicsModifierDesc& outDesc,
    TransformCom* ownerTransform,
    const wstring& modifierTag)
{
    auto& desc = outDesc;
    desc = {};
    desc.modifierTag = modifierTag;
    desc.ownerTransform = ownerTransform;
    desc.blendMode = AnimationModifier::ModifierBlendMode::ModifyReplace;
    desc.physicsAssetPath = L"../Bin/Resources/ModelPhysics/SK_Simon_Physics.json";
    desc.includedCollisionBones = {
        "pelvis",
        "spine_02",
        "spine_04",
        "head",
        "upperarm_l",
        "upperarm_r",
        "lowerarm_r",
        "thigh_l",
        "thigh_r"
    };

    auto addChain = [&desc](
        const string& rootBoneName,
        float outputMultiplier = 1.f,
        float smoothMultiplier = 1.f,
        float maxAngleMul = 1.f,
        float radiusMul = 1.f,
        float dampingMul = 1.f,
        float restoreMul = 1.f)
    {
        PhysicsModifier::ChainDesc chain{};
        chain.rootBoneName = rootBoneName;
        chain.outputMultiplier = outputMultiplier;
        chain.smoothMultiplier = smoothMultiplier;
        chain.maxAngleMul = maxAngleMul;
        chain.radiusMul = radiusMul;
        chain.dampingMul = dampingMul;
        chain.restoreMul = restoreMul;
        desc.chains.push_back(std::move(chain));
    };
    auto makeRootName = [](int32 strandIndex, int32 boneIndex)
    {
        char buffer[64]{};
        sprintf_s(buffer, "Simon_Strand_Hair_%02d_%d", strandIndex, boneIndex);
        return string(buffer);
    };

    for (int32 i = 1; i <= 15; ++i)
    {
        if (i >= 5 && i <= 12)
        {
            addChain(makeRootName(i, 1), 1.00f, 0.18f, 1.18f, 1.10f, 0.78f, 0.58f);
        }
        else if (i == 13 || i == 14 || i == 15)
        {
            addChain(makeRootName(i, 1), 0.92f, 0.26f, 0.72f, 1.22f, 1.04f, 1.04f);
        }
        else
        {
            addChain(makeRootName(i, 1), 0.96f, 0.22f, 0.86f, 1.12f, 0.94f, 0.82f);
        }
    }

    desc.outputAlpha = 0.94f;
    desc.rotationOutputAlpha = 1.00f;
    desc.outputSmoothAlpha = 0.070f;
    desc.idleOutputSmoothAlpha = 0.62f;
    desc.idleMotionDeadzone = 0.060f;
    desc.maxOutputAngleDegree = 112.f;

    desc.contactRotationFollowAlpha = 0.16f;
    desc.freeRotationFollowAlpha = 0.24f;
    desc.rotationFollowDepthScale = 0.08f;
    desc.poseRestoreAlpha = 0.14f;
    desc.poseRestoreMaxSpeed = 8.0f;
    desc.maxLocalOffset = Vec3(4.80f, 3.20f, 5.60f);
    desc.movementInertiaScale = 1.85f;
    desc.tipMovementInertiaScale = 18.00f;
    desc.maxMovementInertiaSpeed = 64.0f;
    desc.lateralInertiaScale = 3.20f;
    desc.verticalInertiaScale = 0.40f;
    desc.depthInertiaScale = 3.20f;
    desc.includeDriverLateralInertia = true;
    desc.accelerationInertiaScale = 0.010f;
    desc.inertiaVelocitySmoothAlpha = 0.030f;
    desc.poseSpringStrength = 0.24f;
    desc.poseSpringDamping = 0.58f;
    desc.lengthConstraintAlpha = 0.34f;
    desc.bendConstraintAlpha = 0.035f;
    desc.projectionIterations = 5;

    desc.contactInertiaDamping = 0.30f;
    desc.releaseInertiaDamping = 0.50f;

    desc.enableCollision = true;
    desc.collisionPrimitiveScale = 0.42f;
    desc.collisionSphereRadius = 0.24f;
    desc.collisionRadiusPadding = 0.10f;
    desc.collisionContactSkinWidth = 0.055f;
    desc.collisionPushOutScale = 1.00f;
    desc.contactSnapAlpha = 0.52f;
    desc.collisionCorrectionSmoothAlpha = 0.48f;
    desc.collisionNormalSmoothAlpha = 0.36f;
    desc.collisionProjectionIterations = 3;
    desc.useHardCollisionProjection = true;
    desc.hardCollisionProjectionIterations = 8;
    desc.hardCollisionLengthAlpha = 1.00f;

    desc.windDirection = kCommonWindDirection;
    desc.windScale = 0.f;
    desc.windWaveScale = 0.f;
    desc.windWaveFrequency = 0.45f;
    desc.windWaveDepthPhase = 0.35f;
    desc.gravityScale = 0.46f;
    desc.teleportDistance = 300.f;
    desc.solverIterations = 8;
    desc.writeBackEnabled = true;
    desc.debugDrawCollisionCapsules = false;
}

void ChainModifierPreset::BuildDuallisteHairbandPhysicsDesc(
    PhysicsModifier::PhysicsModifierDesc& outDesc,
    TransformCom* ownerTransform,
    const wstring& modifierTag)
{
    auto& desc = outDesc;
    desc = {};
    desc.modifierTag = modifierTag;
    desc.ownerTransform = ownerTransform;
    desc.blendMode = AnimationModifier::ModifierBlendMode::ModifyReplace;

    desc.chains = {
        { "hairband_root", 1.00f, 0.30f, 4.20f, 0.30f, 0.18f, 0.62f }
    };

    desc.outputAlpha = 1.00f;
    desc.rotationOutputAlpha = 1.00f;
    desc.outputSmoothAlpha = 0.180f;
    desc.idleOutputSmoothAlpha = 0.10f;
    desc.idleMotionDeadzone = 0.002f;
    desc.maxOutputAngleDegree = 180.f;

    desc.contactRotationFollowAlpha = 0.26f;
    desc.freeRotationFollowAlpha = 0.36f;
    desc.rotationFollowDepthScale = 0.04f;
    desc.poseRestoreAlpha = 0.024f;
    desc.poseRestoreMaxSpeed = 18.0f;
    desc.maxLocalOffset = Vec3(48.00f, 88.00f, 56.00f);
    desc.movementInertiaScale = 9.60f;
    desc.tipMovementInertiaScale = 128.00f;
    desc.maxMovementInertiaSpeed = 260.f;
    desc.lateralInertiaScale = 18.50f;
    desc.verticalInertiaScale = 4.20f;
    desc.depthInertiaScale = 16.00f;
    desc.includeDriverLateralInertia = true;
    desc.accelerationInertiaScale = 0.065f;
    desc.inertiaVelocitySmoothAlpha = 0.120f;
    desc.poseSpringStrength = 0.14f;
    desc.poseSpringDamping = 0.18f;
    desc.lengthConstraintAlpha = 0.052f;
    desc.bendConstraintAlpha = 0.001f;
    desc.projectionIterations = 4;

    desc.contactInertiaDamping = 0.08f;
    desc.releaseInertiaDamping = 0.115f;

    desc.enableCollision = false;
    desc.collisionSphereRadius = 0.10f;
    desc.collisionRadiusPadding = 0.02f;
    desc.collisionContactSkinWidth = 0.02f;
    desc.collisionProjectionIterations = 1;

    desc.windDirection = kCommonWindDirection;
    desc.windScale = 0.18f;
    desc.windWaveScale = 0.f;
    desc.windWaveFrequency = 0.45f;
    desc.windWaveDepthPhase = 0.35f;
    desc.gravityScale = 13.50f;
    desc.useHierarchicalDepthRatio = true;
    desc.teleportDistance = 300.f;
    desc.solverIterations = 18;
    desc.writeBackEnabled = true;
    desc.debugDrawCollisionCapsules = false;
}

void ChainModifierPreset::BuildPotatoBagClothBackDesc(
    ChainModifier::ChainModifierDesc& outDesc,
    TransformCom* ownerTransform,
    PlayerCharacterType characterType,
    const wstring& modifierTag)
{
    BuildPotatoBagHairDesc(outDesc, ownerTransform, characterType, modifierTag);

    outDesc.groups = {
        { "Potatobag_Boss_Cloth_Back_L_01" },
        { "Potatobag_Boss_Cloth_Back_R_01" },
        { "Potatobag_Boss_Cloth_Pelvis_R_01" },
        { "Potatobag_Boss_Cloth_Pelvis_C_01" },
        { "Potatobag_Boss_Cloth_Pelvis_L_01" }
    };

    outDesc.shape.stiffness = 0.245f;
    outDesc.shape.tipStiffnessScale = 0.580f;
    outDesc.shape.poseRestoreStrength = 0.030f;
    outDesc.shape.tipPoseRestoreScale = 1.0f;
    outDesc.shape.maxBendAngleDegree = 90.f;
    outDesc.shape.tipMaxBendAngleScale = 1.0f;
    outDesc.shape.surfaceConstraintStrength = 0.155f;
    outDesc.shape.rootOutputAlpha = 0.5f;
    outDesc.shape.tipOutputAlpha = 0.840f;
    outDesc.shape.pinRoot = false;

    outDesc.motion.damping = 0.980f;
    outDesc.motion.tipDampingScale = 1.040f;
    outDesc.motion.movementInertiaScale = 0.185f;
    outDesc.motion.ownerMovementInertiaScale = 0.185f;
    outDesc.motion.tipMovementInertiaScale = 0.390f;
    outDesc.motion.maxMovementOffset = 0.051f;

    outDesc.runtime.gravity = Vec3(0.f, 0.f, 0.f);
    outDesc.runtime.windDirection = kCommonWindDirection;
    outDesc.runtime.windScale = 0.f;
    outDesc.runtime.windStartRatio = -1.f;
    outDesc.runtime.windWeightScale = -1.f;
    outDesc.runtime.constraintDeadzone = 0.059f;
    outDesc.runtime.resetDistance = 3.f;
    outDesc.runtime.targetFrameRate = 0.f;

    outDesc.collision.enableCollision = false;
    outDesc.collision.particleCollisionRadius = 0.f;
    outDesc.collision.collisionRadiusPadding = 1.f;
    outDesc.collision.debugDrawCapsules = false;
}

void ChainModifierPreset::BuildGustavePotatoCapePhysicsDesc(
    PhysicsModifier::PhysicsModifierDesc& outDesc,
    TransformCom* ownerTransform,
    const wstring& modifierTag)
{
    auto& desc = outDesc;
    desc = {};
    desc.modifierTag = modifierTag;
    desc.ownerTransform = ownerTransform;
    desc.blendMode = AnimationModifier::ModifierBlendMode::ModifyReplace;
    desc.physicsAssetPath = GetPhysicsAssetPath(PlayerCharacterType::Gustave);
    desc.includedCollisionBones = {
        "spine_03",
        "spine_05",
        "pelvis",
        "upperarm_l",
        "upperarm_r",
        "lowerarm_l",
        "lowerarm_r",
        "thigh_l",
        "thigh_r"
    };

    desc.chains = {
        { "Verso_Potato_Cape_01_01", 1.00f, 0.20f, 1.60f, 1.00f, 0.066f, 0.78f },
        { "Verso_Potato_Cape_02_01", 1.00f, 0.20f, 1.60f, 1.00f, 0.066f, 0.78f },
        { "Verso_Potato_Cape_03_01", 1.00f, 0.20f, 1.60f, 1.00f, 0.066f, 0.78f }
    };

    desc.outputAlpha = 1.00f;
    desc.rotationOutputAlpha = 1.00f;
    desc.outputSmoothAlpha = 0.065f;
    desc.idleOutputSmoothAlpha = 0.88f;
    desc.idleMotionDeadzone = 0.055f;
    desc.maxOutputAngleDegree = 105.f;

    desc.contactRotationFollowAlpha = 0.14f;
    desc.freeRotationFollowAlpha = 0.22f;
    desc.rotationFollowDepthScale = 0.10f;
    desc.poseRestoreAlpha = 0.18f;
    desc.poseRestoreMaxSpeed = 9.0f;
    desc.maxLocalOffset = Vec3(7.50f, 2.20f, 9.00f);
    desc.movementInertiaScale = 2.40f;
    desc.tipMovementInertiaScale = 24.00f;
    desc.maxMovementInertiaSpeed = 78.0f;
    desc.lateralInertiaScale = 5.00f;
    desc.verticalInertiaScale = 0.45f;
    desc.depthInertiaScale = 2.35f;
    desc.accelerationInertiaScale = 0.005f;
    desc.inertiaVelocitySmoothAlpha = 0.020f;
    desc.poseSpringStrength = 0.30f;
    desc.poseSpringDamping = 0.680f;
    desc.lengthConstraintAlpha = 0.070f;
    desc.bendConstraintAlpha = 0.010f;
    desc.projectionIterations = 3;

    desc.contactInertiaDamping = 0.120f;
    desc.releaseInertiaDamping = 0.550f;

    desc.enableCollision = false;
    desc.collisionSphereRadius = 0.18f;
    desc.collisionRadiusPadding = 0.08f;
    desc.collisionContactSkinWidth = 0.06f;
    desc.collisionProjectionIterations = 2;

    desc.windDirection = kCommonWindDirection;
    desc.windScale = 0.f;
    desc.windWaveScale = 0.f;
    desc.windWaveFrequency = 0.45f;
    desc.windWaveDepthPhase = 0.35f;
    desc.gravityScale = 0.42f;
    desc.teleportDistance = 300.f;
    desc.solverIterations = 8;
    desc.writeBackEnabled = true;
}

void ChainModifierPreset::BuildBackpackBandDesc(
    ChainModifier::ChainModifierDesc& outDesc,
    TransformCom* ownerTransform,
    PlayerCharacterType characterType,
    const wstring& modifierTag)
{
    auto& desc = outDesc;
    desc = {};
    desc.modifierTag = modifierTag;

    if (characterType != PlayerCharacterType::Gustave)
    {
        return;
    }

    desc.groups = {
        { "Noah_V2Luminas_holder_0"}
    };

    desc.shape.stiffness = 1.0f;
    desc.shape.tipStiffnessScale = 1.680f;
    desc.shape.poseRestoreStrength = 0.f;
    desc.shape.tipPoseRestoreScale = 0.f;
    desc.shape.maxBendAngleDegree = 18.f;
    desc.shape.tipMaxBendAngleScale = 0.f;
    desc.shape.surfaceConstraintStrength = 0.f;
    desc.shape.rootOutputAlpha = 0.f;
    desc.shape.tipOutputAlpha = 1.f;
    desc.shape.pinRoot = true;
    for (auto& group : desc.groups)
    {
        group.rootRotationOffsetDegrees = Vec3(0.f, 0.f, 0.f);
    }

    desc.motion.damping = 0.575f;
    desc.motion.tipDampingScale = 1.910f;
    desc.runtime.gravity = kCommonGravity;
    desc.motion.movementInertiaScale = 0.08f;
    desc.motion.ownerMovementInertiaScale = 0.f;
    desc.motion.tipMovementInertiaScale = 1.110f;
    desc.motion.maxMovementOffset = 0.052f;
    desc.motion.solverIterations = 5;

    desc.collision.enableCollision = false;
    desc.collision.collisionRadiusPadding = 0.35f;
    desc.collision.physicsAssetPath = GetPhysicsAssetPath(characterType);
    desc.collision.debugDrawCapsules = false;

    desc.runtime.ownerTransform = ownerTransform;
    desc.runtime.windDirection = kCommonWindDirection;
    desc.runtime.windScale = 0.120f;
    desc.runtime.windStartRatio = -1.f;
    desc.runtime.windWeightScale = -1.f;
    desc.runtime.constraintDeadzone = 0.048f;
    desc.runtime.resetDistance = 3.f;
    desc.runtime.targetFrameRate = 60.f;

    desc.blendMode = AnimationModifier::ModifierBlendMode::ModifyReplace;
}

void ChainModifierPreset::BuildCoatDesc(
    ChainModifier::ChainModifierDesc& outDesc,
    TransformCom* ownerTransform,
    PlayerCharacterType characterType,
    const wstring& modifierTag)
{
    auto& desc = outDesc;
    desc = {};
    desc.modifierTag = modifierTag;
    desc.strengthAlpha = 0.96f;

    desc.shape.stiffness = 0.70f;
    desc.shape.tipStiffnessScale = 0.68f;
    desc.shape.poseRestoreStrength = 0.12f;
    desc.shape.tipPoseRestoreScale = 1.18f;
    desc.shape.maxBendAngleDegree = 40.f;
    desc.shape.tipMaxBendAngleScale = 0.85f;
    desc.shape.surfaceConstraintStrength = 0.f;
    desc.shape.rootOutputAlpha = 0.05f;
    desc.shape.tipOutputAlpha = 1.f;
    desc.shape.pinRoot = true;
    desc.shape.useBindPoseAsRest = true;

    desc.motion.damping = 0.70f;
    desc.motion.tipDampingScale = 1.02f;
    desc.runtime.gravity = Vec3(0.f, -0.45f, 0.f);
    desc.motion.movementInertiaScale = 0.10f;
    desc.motion.ownerMovementInertiaScale = 0.f;
    desc.motion.tipMovementInertiaScale = 1.08f;
    desc.motion.maxMovementOffset = 0.024f;
    desc.motion.removeOwnerMotionFromPoseInertia = true;
    desc.motion.followOwnerTranslationInSimulation = true;
    desc.motion.solverIterations = 4;

    desc.collision.enableCollision = true;
    desc.collision.particleCollisionRadius = 3.0f;
    desc.collision.collisionRadiusPadding = 0.08f;
    desc.collision.physicsAssetPath = GetPhysicsAssetPath(characterType);
    desc.collision.includedBones = GetCoatCollisionBones(characterType);
    desc.collision.overrideCapsules = GetSourceCoatCapsules(characterType);
    desc.collision.debugDrawCapsules = false;

    if (!desc.collision.overrideCapsules.empty())
    {
        desc.collision.physicsAssetPath.clear();
        desc.collision.includedBones.clear();
    }

    desc.runtime.ownerTransform = ownerTransform;
    desc.runtime.targetFrameRate = 60.f;
    desc.runtime.windDirection = kCommonWindDirection;
    desc.runtime.windScale = 0.f;
    desc.runtime.windStartRatio = -1.f;
    desc.runtime.windWeightScale = -1.f;
    desc.runtime.constraintDeadzone = 0.010f;
    desc.blendMode = AnimationModifier::ModifierBlendMode::ModifyReplace;

    if (characterType == PlayerCharacterType::Maelle)
    {
        desc.groups = {
            { "Maelle_V2_Coat_01_01", 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 0xffffffffu, 0.5f, 5.f },
            { "Maelle_V2_Coat_02_01", 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 0xffffffffu, 0.5f, 10.f },
            { "Maelle_V2_Coat_03_01", 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 0xffffffffu, 0.5f, 10.f }
        };
        return;
    }

    if (characterType == PlayerCharacterType::Lune)
    {
        desc.collision.collisionRadiusPadding = 0.12f;
        desc.motion.solverIterations = 5;
        desc.groups = {
            { "Lune_V3_Coat_01_01", 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 0xffffffffu, 0.5f, 5.f },
            { "Lune_V3_Coat_02_01", 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 0xffffffffu, 0.5f, 10.f },
            { "Lune_V3_Coat_03_01", 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 0xffffffffu, 0.5f, 10.f },
            { "Lune_V3_Coat_04_01", 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 0xffffffffu, 0.5f, 10.f },
            { "Lune_V3_Coat_05_01", 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 0xffffffffu, 0.5f, 10.f },
            { "Lune_V3_Coat_06_01", 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 0xffffffffu, 0.5f, 10.f },
            { "Lune_V3_Coat_07_01", 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 0xffffffffu, 0.5f, 10.f }
        };
        return;
    }

    if (characterType == PlayerCharacterType::Gustave)
    {
        desc.collision.collisionRadiusPadding = 0.08f;
        desc.groups = {
            { "Noah_V2Coat_00_01", 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 0xffffffffu, 1.0f, 5.f },
            { "Noah_V2Coat_01_01", 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 0xffffffffu, 0.5f, 10.f },
            { "Noah_V2Coat_02_01", 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 0xffffffffu, 0.5f, 10.f },
            { "Noah_V2Coat_03_01", 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 0xffffffffu, 0.5f, 10.f },
            { "Noah_V2Coat_04_01", 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 0xffffffffu, 0.5f, 10.f },
            { "Noah_V2Coat_05_01", 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 0xffffffffu, 0.5f, 10.f },
            { "Noah_V2Coat_06_01", 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 0xffffffffu, 0.5f, 10.f },
            { "Noah_V2Coat_07_01", 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 0xffffffffu, 0.5f, 10.f },
            { "Noah_V2Coat_08_01", 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 0xffffffffu, 0.5f, 10.f }
        };
    }

    return;
}

void ChainModifierPreset::BuildPoseFollowCoatDesc(
    PoseFollowChainModifier::PoseFollowChainModifierDesc& outDesc,
    TransformCom* ownerTransform,
    PlayerCharacterType characterType,
    const wstring& modifierTag)
{
    auto& desc = outDesc;
    desc = {};
    desc.modifierTag = modifierTag;
    desc.ownerTransform = ownerTransform;

    desc.physicsAssetPath = GetPhysicsAssetPath(characterType);
    desc.includedCollisionBones = GetCoatCollisionBones(characterType);
    desc.overrideCapsules = GetSourceCoatCapsules(characterType);
    if (!desc.overrideCapsules.empty())
    {
        desc.physicsAssetPath.clear();
        desc.includedCollisionBones.clear();
    }

    desc.particleCollisionRadius = 2.6f;
    desc.windDirection = kCommonWindDirection;
    desc.windScale = 0.f;
    desc.ownerMotionScale = 0.006f;
    desc.animatedMotionScale = 0.17f;
    desc.animatedMotionDeadzone = 0.11f;
    desc.maxExternalOffset = 1.28f;

    desc.damping = 0.68f;
    desc.tipDampingScale = 1.08f;
    desc.poseFollowStrength = 0.70f;
    desc.tipFollowScale = 0.32f;
    desc.maxBendAngleDegree = 16.f;
    desc.tipBendAngleScale = 2.15f;
    desc.bendLimitStrength = 0.9f;
    desc.surfaceConstraintStrength = 0.24f;
    desc.rootOutputAlpha = 0.f;
    desc.tipOutputAlpha = 0.74f;
    desc.idleOutputAlphaScale = 0.28f;
    desc.outputSmoothAlpha = 0.42f;
    desc.idleOutputSmoothAlpha = 0.72f;
    desc.outputDeadzoneDegree = 1.6f;
    desc.idleSettleStrength = 0.94f;
    desc.sleepVelocityThreshold = 0.035f;
    desc.sleepPoseDistance = 0.22f;
    desc.solverIterations = 5;

    if (characterType == PlayerCharacterType::Maelle)
    {
        desc.enableCollision = false;
        desc.collisionRadiusPadding = 0.f;
        desc.particleCollisionRadius = 0.f;
        desc.ownerMotionScale = 0.075f;
        desc.animatedMotionScale = 0.55f;
        desc.ownerMotionDeadzone = 0.030f;
        desc.ownerRotationDeadzoneDegree = 0.40f;
        desc.animatedMotionDeadzone = 0.120f;
        desc.maxExternalOffset = 0.85f;
        desc.damping = 0.82f;
        desc.tipDampingScale = 1.12f;
        desc.poseFollowStrength = 0.30f;
        desc.tipFollowScale = 0.38f;
        desc.maxBendAngleDegree = 54.f;
        desc.tipBendAngleScale = 2.05f;
        desc.bendLimitStrength = 0.48f;
        desc.surfaceConstraintStrength = 0.015f;
        desc.rootOutputAlpha = 0.12f;
        desc.tipOutputAlpha = 0.96f;
        desc.idleOutputAlphaScale = 0.55f;
        desc.outputSmoothAlpha = 0.36f;
        desc.idleOutputSmoothAlpha = 0.90f;
        desc.outputDeadzoneDegree = 3.0f;
        desc.idleSettleStrength = 0.98f;
        desc.sleepVelocityThreshold = 0.055f;
        desc.sleepPoseDistance = 0.26f;
        desc.groups = {
            { "Maelle_V2_Coat_01_01", 0.34f, 0.66f, 1.02f },
            { "Maelle_V2_Coat_02_01", 0.34f, 1.45f, 0.54f, 0.34f, 0.78f },
            { "Maelle_V2_Coat_03_01", 0.34f, 0.66f, 1.02f }
        };
        return;
    }

    if (characterType == PlayerCharacterType::Lune)
    {
        desc.enableCollision = false;
        desc.collisionRadiusPadding = 0.f;
        desc.particleCollisionRadius = 0.f;
        desc.ownerMotionScale = 0.075f;
        desc.animatedMotionScale = 0.55f;
        desc.ownerMotionDeadzone = 0.030f;
        desc.ownerRotationDeadzoneDegree = 0.40f;
        desc.animatedMotionDeadzone = 0.120f;
        desc.maxExternalOffset = 0.85f;
        desc.damping = 0.82f;
        desc.tipDampingScale = 1.12f;
        desc.poseFollowStrength = 0.30f;
        desc.tipFollowScale = 0.38f;
        desc.maxBendAngleDegree = 54.f;
        desc.tipBendAngleScale = 2.05f;
        desc.bendLimitStrength = 0.48f;
        desc.surfaceConstraintStrength = 0.015f;
        desc.rootOutputAlpha = 0.12f;
        desc.tipOutputAlpha = 0.96f;
        desc.idleOutputAlphaScale = 0.55f;
        desc.outputSmoothAlpha = 0.36f;
        desc.idleOutputSmoothAlpha = 0.90f;
        desc.outputDeadzoneDegree = 3.0f;
        desc.idleSettleStrength = 0.98f;
        desc.sleepVelocityThreshold = 0.055f;
        desc.sleepPoseDistance = 0.26f;
        desc.groups = {
            { "Lune_V3_Coat_01_01", 0.34f, 1.15f, 0.64f, 0.08f, 0.45f },
            { "Lune_V3_Coat_02_01", 0.34f, 1.15f, 0.64f, 0.08f, 0.45f },
            { "Lune_V3_Coat_03_01", 0.34f, 1.20f, 0.62f, 0.15f, 0.50f },
            { "Lune_V3_Coat_04_01", 0.34f, 1.20f, 0.62f, 0.15f, 0.50f },
            { "Lune_V3_Coat_05_01", 0.34f, 1.45f, 0.56f, 0.40f, 0.68f },
            { "Lune_V3_Coat_06_01", 0.34f, 1.75f, 0.50f, 1.00f, 1.00f },
            { "Lune_V3_Coat_07_01", 0.34f, 1.75f, 0.50f, 1.00f, 1.00f }
        };
        return;
    }

    if (characterType == PlayerCharacterType::None)
    {
        desc.enableCollision = false;
        desc.collisionRadiusPadding = 0.f;
        desc.particleCollisionRadius = 0.f;
        desc.ownerMotionScale = 0.075f;
        desc.animatedMotionScale = 0.55f;
        desc.ownerMotionDeadzone = 0.030f;
        desc.ownerRotationDeadzoneDegree = 0.40f;
        desc.animatedMotionDeadzone = 0.120f;
        desc.maxExternalOffset = 0.85f;
        desc.damping = 0.82f;
        desc.tipDampingScale = 1.12f;
        desc.poseFollowStrength = 0.30f;
        desc.tipFollowScale = 0.38f;
        desc.maxBendAngleDegree = 54.f;
        desc.tipBendAngleScale = 2.05f;
        desc.bendLimitStrength = 0.48f;
        desc.surfaceConstraintStrength = 0.015f;
        desc.rootOutputAlpha = 0.12f;
        desc.tipOutputAlpha = 0.96f;
        desc.idleOutputAlphaScale = 0.55f;
        desc.outputSmoothAlpha = 0.36f;
        desc.idleOutputSmoothAlpha = 0.90f;
        desc.outputDeadzoneDegree = 3.0f;
        desc.idleSettleStrength = 0.98f;
        desc.sleepVelocityThreshold = 0.055f;
        desc.sleepPoseDistance = 0.26f;
        desc.groups = {
            { "Simon_Coat_01_01", 0.34f, 1.62f, 0.42f, 0.82f, 1.00f },
            { "Simon_Coat_02_01", 0.34f, 1.62f, 0.42f, 0.82f, 1.00f },
            { "Simon_Coat_03_01", 0.34f, 1.62f, 0.42f, 0.82f, 1.00f },
            { "Simon_Coat_04_01", 0.34f, 1.62f, 0.42f, 0.82f, 1.00f },
            { "Simon_Coat_05_01", 0.34f, 1.62f, 0.42f, 0.82f, 1.00f },
            { "Simon_Coat_06_01", 0.34f, 1.62f, 0.42f, 0.82f, 1.00f }
        };
        return;
    }

    if (characterType == PlayerCharacterType::Gustave)
    {
        desc.enableCollision = false;
        desc.collisionRadiusPadding = 0.f;
        desc.particleCollisionRadius = 0.f;
        desc.ownerMotionScale = 0.075f;
        desc.animatedMotionScale = 0.55f;
        desc.ownerMotionDeadzone = 0.030f;
        desc.ownerRotationDeadzoneDegree = 0.40f;
        desc.animatedMotionDeadzone = 0.120f;
        desc.maxExternalOffset = 0.85f;
        desc.damping = 0.82f;
        desc.tipDampingScale = 1.12f;
        desc.poseFollowStrength = 0.30f;
        desc.tipFollowScale = 0.38f;
        desc.maxBendAngleDegree = 54.f;
        desc.tipBendAngleScale = 2.05f;
        desc.bendLimitStrength = 0.48f;
        desc.surfaceConstraintStrength = 0.015f;
        desc.rootOutputAlpha = 0.12f;
        desc.tipOutputAlpha = 0.96f;
        desc.idleOutputAlphaScale = 0.55f;
        desc.outputSmoothAlpha = 0.36f;
        desc.idleOutputSmoothAlpha = 0.90f;
        desc.outputDeadzoneDegree = 3.0f;
        desc.idleSettleStrength = 0.98f;
        desc.sleepVelocityThreshold = 0.055f;
        desc.sleepPoseDistance = 0.26f;
        desc.groups = {
            { "Noah_V2Coat_00_01", 0.34f, 1.04f, 0.93f },
            { "Noah_V2Coat_01_01", 0.34f, 1.03f, 0.94f },
            { "Noah_V2Coat_02_01", 0.34f, 1.03f, 0.94f },
            { "Noah_V2Coat_03_01", 0.34f, 1.03f, 0.94f },
            { "Noah_V2Coat_04_01", 0.34f, 1.03f, 0.94f },
            { "Noah_V2Coat_05_01", 0.34f, 1.03f, 0.94f },
            { "Noah_V2Coat_06_01", 0.34f, 1.03f, 0.94f },
            { "Noah_V2Coat_07_01", 0.34f, 1.03f, 0.94f },
            { "Noah_V2Coat_08_01", 0.34f, 1.62f, 0.42f, 0.82f, 1.00f }
        };
    }
}

NS_END

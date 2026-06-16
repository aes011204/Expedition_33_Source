#include "pch.h"
#include "ChainModifierPreset.h"

NS_BEGIN(Client)

namespace
{
    const Vec3 kCommonGravity{ 0.f, -780.f, 0.f };
    const Vec3 kCommonWindDirection{ Vec3{1.f, 0.f, 1.f} };
    constexpr float kScarfWindIntensity{ 0.45f };
    constexpr float kArmbandWindIntensity{ 0.12f };
    constexpr float kMonsterClothWindIntensity{ 0.07f };
    constexpr float kCoatWindIntensity{ 0.08f };

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

    ChainModifier::FloatCurveDesc MakeLinearCurve(float startValue, float endValue)
    {
        ChainModifier::FloatCurveDesc curve{};
        curve.keys = {
            { 0.f, startValue },
            { 1.f, endValue }
        };
        return curve;
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

    desc.shape.stiffness = 0.9f;
    desc.shape.poseRestoreStrength = 0.88f;
    desc.shape.maxBendAngleDegree = 18.f;
    desc.shape.pinRoot = true;

    desc.motion.damping = 0.35f;
    desc.motion.gravity = Vec3::Zero;
    desc.motion.movementInertiaScale = 0.f;
    desc.motion.maxMovementOffset = 0.f;
    desc.motion.solverIterations = 4;

    desc.collision.collisionRadiusPadding = 0.f;
    desc.collision.enableCollision = true;
    desc.collision.physicsAssetPath = GetPhysicsAssetPath(characterType);
    desc.collision.includedBones = GetScarfCollisionBones(characterType);
    desc.collision.debugDrawCapsules = true;

    desc.runtime.ownerTransform = ownerTransform;
    desc.runtime.windDirection = kCommonWindDirection;
    desc.runtime.windScale = kScarfWindIntensity;
    desc.runtime.useAnimatedPoseAsRest = true;

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
    desc.strengthAlpha = 1.f;
    desc.ownerTransform = ownerTransform;

    desc.enableCollision = true;
    desc.debugDrawCapsules = false;
    desc.physicsAssetPath = GetPhysicsAssetPath(characterType);
    desc.includedCollisionBones = GetScarfCollisionBones(characterType);
    desc.particleCollisionRadius = 1.4f;
    desc.collisionRadiusPadding = 0.04f;

    desc.windDirection = kCommonWindDirection;
    desc.windScale = 0.f;
    desc.ownerMotionScale = 0.009f;
    desc.animatedMotionScale = 0.22f;
    desc.ownerMotionDeadzone = 0.02f;
    desc.animatedMotionDeadzone = 0.09f;
    desc.maxExternalOffset = 1.45f;

    desc.damping = 0.72f;
    desc.tipDampingScale = 1.10f;
    desc.poseFollowStrength = 0.60f;
    desc.tipFollowScale = 0.26f;
    desc.maxBendAngleDegree = 20.f;
    desc.tipBendAngleScale = 2.25f;
    desc.bendLimitStrength = 0.88f;
    desc.surfaceConstraintStrength = 0.18f;
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
    desc.resetDistance = 3.f;
    desc.blendMode = AnimationModifier::ModifierBlendMode::ModifyReplace;

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
            { "Maelle_V4_Scarf_01_01", 0xffffffffu, 0.26f, 0.92f, 0.88f },
            { "Maelle_V4_Scarf_02_01", 0xffffffffu, 0.26f, 0.92f, 0.88f },
            { "Maelle_V4_Scarf_03_01", 0xffffffffu, 0.26f, 0.92f, 0.88f },
            { "Maelle_V4_Scarf_04_01", 0xffffffffu, 0.26f, 0.92f, 0.88f },
            { "Maelle_V4_Scarf_05_01", 0xffffffffu, 0.26f, 0.92f, 0.88f },
            { "Maelle_V4_Scarf_06_01", 0xffffffffu, 0.26f, 0.92f, 0.88f }
        };
        return;
    }

    if (characterType == PlayerCharacterType::Lune)
    {
        desc.groups = {
            { "Lune_V2_Scarf_01_01", 0xffffffffu, 0.20f, 0.82f, 0.96f },
            { "Lune_V2_Scarf_02_01", 0xffffffffu, 0.20f, 0.82f, 0.96f },
            { "Lune_V2_Scarf_03_01", 0xffffffffu, 0.20f, 0.82f, 0.96f },
            { "Lune_V2_Scarf_04_01", 0xffffffffu, 0.20f, 0.82f, 0.96f },
            { "Lune_V2_Scarf_05_01", 0xffffffffu, 0.20f, 0.82f, 0.96f },
            { "Lune_V2_Scarf_06_01", 0xffffffffu, 0.20f, 0.82f, 0.96f }
        };
        return;
    }

    if (characterType == PlayerCharacterType::Gustave)
    {
        desc.groups = {
            { "Noah_V2_cloth2", 0xffffffffu, 0.20f, 0.82f, 0.96f },
            { "Noah_V2_cloth1", 0xffffffffu, 0.20f, 0.82f, 0.96f },
            { "Noah_V2_cloth3", 0xffffffffu, 0.20f, 0.82f, 0.96f },
            { "Noah_V2_cloth4", 0xffffffffu, 0.20f, 0.82f, 0.96f }
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

    desc.shape.stiffness = 1.0f;
    desc.shape.poseRestoreStrength = 0.00005f;
    desc.shape.maxBendAngleDegree = 120.f;
    desc.shape.rootRotationOffsetDegrees = Vec3(-30.f, 0.f, 0.f);

    desc.motion.damping = 0.98f;
    desc.motion.gravity = kCommonGravity;
    desc.motion.movementInertiaScale = 0.18f;
    desc.motion.maxMovementOffset = 0.05f;
    desc.motion.solverIterations = 5;

    desc.collision.enableCollision = false;
    desc.collision.collisionRadiusPadding = 0.15f;
    desc.collision.physicsAssetPath = GetPhysicsAssetPath(characterType);
    desc.collision.debugDrawCapsules = false;
    desc.runtime.ownerTransform = ownerTransform;
    desc.runtime.windDirection = kCommonWindDirection;
    desc.runtime.windScale = kMonsterClothWindIntensity;
    desc.runtime.windStartRatio = 0.05f;
    desc.runtime.windWeightScale = 0.9f;

    desc.blendMode = AnimationModifier::ModifierBlendMode::ModifyReplace;
    return;
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
    desc.shape.pinRoot = false;
    desc.shape.rootRotationOffsetDegrees = Vec3(-30.f, 0.f, 0.f);

    desc.motion.damping = 0.965f;
    desc.motion.tipDampingScale = 1.08f;
    desc.motion.gravity = kCommonGravity;
    desc.motion.movementInertiaScale = 0.145f;
    desc.motion.tipMovementInertiaScale = 1.34f;
    desc.motion.maxMovementOffset = 0.042f;
    desc.motion.solverIterations = 5;

    desc.collision.enableCollision = false;
    desc.collision.collisionRadiusPadding = 0.15f;
    desc.collision.physicsAssetPath = GetPhysicsAssetPath(characterType);
    desc.collision.debugDrawCapsules = false;

    desc.runtime.ownerTransform = ownerTransform;
    desc.runtime.windDirection = kCommonWindDirection;
    desc.runtime.windScale = kMonsterClothWindIntensity;
    desc.runtime.windStartRatio = 0.05f;
    desc.runtime.windWeightScale = 0.9f;

    desc.blendMode = AnimationModifier::ModifierBlendMode::ModifyReplace;
    return;
}

void ChainModifierPreset::BuildEvequeClothDesc(
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
        
        { "Eveque_Top_Cloth_root_l" },
        
        { "Eveque_Top_Cloth_root_r" },
        
        { "Eveque_Leg_Cloth_root_l" },
        
        { "Eveque_Leg_Cloth_root_r" }
    };

    desc.shape.stiffness = 1.0f;
    desc.shape.tipStiffnessScale = 0.62f;
    desc.shape.poseRestoreStrength = 0.006f;
    desc.shape.tipPoseRestoreScale = 0.28f;
    desc.shape.maxBendAngleDegree = 105.f;
    desc.shape.tipMaxBendAngleScale = 1.22f;
    desc.shape.rootRotationOffsetDegrees = Vec3(-30.f, 0.f, 0.f);

    desc.motion.damping = 0.965f;
    desc.motion.tipDampingScale = 1.08f;
    desc.motion.gravity = kCommonGravity;
    desc.motion.movementInertiaScale = 0.145f;
    desc.motion.tipMovementInertiaScale = 1.34f;
    desc.motion.maxMovementOffset = 0.042f;
    desc.motion.solverIterations = 5;

    desc.collision.enableCollision = false;
    desc.collision.collisionRadiusPadding = 0.15f;
    desc.collision.physicsAssetPath = GetPhysicsAssetPath(characterType);
    desc.collision.debugDrawCapsules = false;

    desc.runtime.ownerTransform = ownerTransform;
    desc.runtime.windDirection = kCommonWindDirection;
    desc.runtime.windScale = kMonsterClothWindIntensity;

    desc.blendMode = AnimationModifier::ModifierBlendMode::ModifyReplace;
    return;
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

    desc.shape.stiffness = 0.42f;
    desc.shape.tipStiffnessScale = 0.55f;
    desc.shape.poseRestoreStrength = 0.18f;
    desc.shape.tipPoseRestoreScale = 0.65f;
    desc.shape.maxBendAngleDegree = 18.f;
    desc.shape.tipMaxBendAngleScale = 1.25f;
    desc.shape.pinRoot = true;
    desc.shape.rootRotationOffsetDegrees = Vec3::Zero;

    desc.motion.damping = 0.62f;
    desc.motion.gravity = kCommonGravity;
    desc.motion.movementInertiaScale = 0.08f;
    desc.motion.tipMovementInertiaScale = 1.35f;
    desc.motion.maxMovementOffset = 0.035f;
    desc.motion.solverIterations = 5;

    desc.collision.enableCollision = true;
    desc.collision.collisionRadiusPadding = 0.35f;
    desc.collision.physicsAssetPath = GetPhysicsAssetPath(characterType);
    desc.collision.debugDrawCapsules = false;

    desc.runtime.ownerTransform = ownerTransform;
    desc.runtime.windDirection = kCommonWindDirection;
    desc.runtime.windScale = kArmbandWindIntensity;
    desc.runtime.useOwnerMotionInertia = true;
    desc.runtime.transportParticlesWithAnimatedPose = true;
    desc.runtime.animatedPoseTransportAlpha = 0.45f;
    desc.runtime.animatedPoseTransportDeadzone = 0.025f;
    desc.runtime.useAnimatedPoseAsRest = true;

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

    desc.shape.stiffness = 0.5f;
    desc.shape.tipStiffnessScale = 0.35f;
    desc.shape.poseRestoreStrength = 0.06f;
    desc.shape.tipPoseRestoreScale = 0.75f;
    desc.shape.maxBendAngleDegree = 20.f;
    desc.shape.tipMaxBendAngleScale = 1.0f;
    desc.shape.surfaceConstraintStrength = 0.34f;
    desc.shape.pinRoot = true;

    desc.motion.damping = 0.65f;
    desc.motion.tipDampingScale = 1.1538461f;
    desc.motion.dampingCurve = MakeLinearCurve(0.65f, 0.75f);
    desc.motion.gravity = Vec3::Zero;
    // 우리 구현은 animated-pose transport와 별개로 관성 오프셋을 또 더하므로
    // 코트에서는 값을 낮춰야 "끝만 뒤로 끌려가는" 현상이 줄어든다.
    desc.motion.movementInertiaScale = 0.012f;
    desc.motion.tipMovementInertiaScale = 1.0f;
    desc.motion.maxMovementOffset = 0.01f;
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
    desc.runtime.windScale = 0.018f;
    desc.runtime.useOwnerMotionInertia = false;
    desc.runtime.useChainRootMotionInertia = false;
    desc.runtime.transportParticlesWithAnimatedPose = true;
    // Kawaii의 WorldDampingLocation/Rotation은 "얼마나 월드 변화 영향을 약하게 받을지"에 가까워서,
    // 우리 transport alpha로 옮길 때는 거의 반대로 해석해야 애니메이션 원형이 무너지지 않는다.
    desc.runtime.animatedPoseTransportAlpha = 0.2f;
    desc.runtime.animatedPoseTransportRotationAlpha = 1.f;
    desc.runtime.animatedPoseTransportTipScale = 0.8f;
    desc.runtime.animatedPoseTransportDeadzone = 0.025f;
    desc.runtime.useAnimatedPoseAsRest = true;
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
    desc.strengthAlpha = 1.f;
    desc.ownerTransform = ownerTransform;

    desc.enableCollision = true;
    desc.debugDrawCapsules = false;
    desc.physicsAssetPath = GetPhysicsAssetPath(characterType);
    desc.includedCollisionBones = GetCoatCollisionBones(characterType);
    desc.overrideCapsules = GetSourceCoatCapsules(characterType);
    if (!desc.overrideCapsules.empty())
    {
        desc.physicsAssetPath.clear();
        desc.includedCollisionBones.clear();
    }

    desc.particleCollisionRadius = 2.6f;
    desc.collisionRadiusPadding = 0.08f;
    desc.windDirection = kCommonWindDirection;
    desc.windScale = 0.f;
    desc.ownerMotionScale = 0.006f;
    desc.animatedMotionScale = 0.17f;
    desc.ownerMotionDeadzone = 0.02f;
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
    desc.targetFrameRate = 60.f;
    desc.resetDistance = 3.f;
    desc.blendMode = AnimationModifier::ModifierBlendMode::ModifyReplace;

    if (characterType == PlayerCharacterType::Maelle)
    {
        desc.groups = {
            { "Maelle_V2_Coat_01_01", 0xffffffffu, 0.34f, 0.66f, 1.02f },
            { "Maelle_V2_Coat_02_01", 0xffffffffu, 0.34f, 0.66f, 1.02f },
            { "Maelle_V2_Coat_03_01", 0xffffffffu, 0.34f, 0.66f, 1.02f }
        };
        return;
    }

    if (characterType == PlayerCharacterType::Lune)
    {
        desc.collisionRadiusPadding = 0.12f;
        desc.particleCollisionRadius = 3.0f;
        desc.groups = {
            { "Lune_V3_Coat_01_01", 0xffffffffu, 0.34f, 0.64f, 1.05f },
            { "Lune_V3_Coat_02_01", 0xffffffffu, 0.34f, 0.64f, 1.05f },
            { "Lune_V3_Coat_03_01", 0xffffffffu, 0.34f, 0.64f, 1.05f },
            { "Lune_V3_Coat_04_01", 0xffffffffu, 0.34f, 0.64f, 1.05f },
            { "Lune_V3_Coat_05_01", 0xffffffffu, 0.34f, 0.64f, 1.05f },
            { "Lune_V3_Coat_06_01", 0xffffffffu, 0.34f, 0.64f, 1.05f },
            { "Lune_V3_Coat_07_01", 0xffffffffu, 0.34f, 0.64f, 1.05f }
        };
        return;
    }

    if (characterType == PlayerCharacterType::Gustave)
    {
        desc.damping = 0.68f;
        desc.tipDampingScale = 1.16f;
        desc.poseFollowStrength = 0.59f;
        desc.tipFollowScale = 0.44f;
        desc.tipBendAngleScale = 1.92f;
        desc.rootOutputAlpha = 0.09f;
        desc.tipOutputAlpha = 0.86f;
        desc.outputSmoothAlpha = 0.36f;
        desc.groups = {
            { "Noah_V2Coat_00_01", 0xffffffffu, 0.34f, 1.04f, 0.93f },
            { "Noah_V2Coat_01_01", 0xffffffffu, 0.34f, 1.03f, 0.94f },
            { "Noah_V2Coat_02_01", 0xffffffffu, 0.34f, 1.03f, 0.94f },
            { "Noah_V2Coat_03_01", 0xffffffffu, 0.34f, 1.03f, 0.94f },
            { "Noah_V2Coat_04_01", 0xffffffffu, 0.34f, 1.03f, 0.94f },
            { "Noah_V2Coat_05_01", 0xffffffffu, 0.34f, 1.03f, 0.94f },
            { "Noah_V2Coat_06_01", 0xffffffffu, 0.34f, 1.03f, 0.94f },
            { "Noah_V2Coat_07_01", 0xffffffffu, 0.34f, 1.03f, 0.94f },
            { "Noah_V2Coat_08_01", 0xffffffffu, 0.34f, 1.03f, 0.94f }
        };
    }
}

NS_END

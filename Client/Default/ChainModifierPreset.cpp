#include "pch.h"
#include "ChainModifierPreset.h"

NS_BEGIN(Client)

namespace
{
    const Vec3 kCommonGravity{ 0.f, -780.f, 0.f };
    const Vec3 kCommonWindDirection{ Vec3{1.f, 0.f, 1.f} };
    constexpr float kScarfWindIntensity{ 0.45f };
    constexpr float kArmbandWindIntensity{ 0.12f };
    constexpr float kCoatWindIntensity{ 0.08f };

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
            return { "spine_05", "upperarm_r", "pelvis", "thigh_r", "thigh_l", "calf_l", "hand_l" };
        }

        return {};
    }

    vector<string> GetCoatCollisionBones(PlayerCharacterType characterType)
    {
        if (characterType == PlayerCharacterType::Gustave)
        {
            return { "pelvis", "thigh_l", "thigh_r", "calf_r", "calf_l" };
        }

        if (characterType == PlayerCharacterType::Lune)
        {
            return {
                "pelvis",
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
    desc.motion.solverIterations = 2;

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

    desc.runtime.ownerTransform = ownerTransform;
    desc.runtime.windDirection = kCommonWindDirection;
    desc.runtime.windScale = kArmbandWindIntensity;

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

    desc.shape.stiffness = 0.84f;
    desc.shape.tipStiffnessScale = 0.84f;
    desc.shape.poseRestoreStrength = 0.58f;
    desc.shape.tipPoseRestoreScale = 0.82f;
    desc.shape.maxBendAngleDegree = 7.f;
    desc.shape.tipMaxBendAngleScale = 1.24f;
    desc.shape.surfaceConstraintStrength = 0.24f;
    desc.shape.pinRoot = true;

    desc.motion.damping = 0.42f;
    desc.motion.gravity = Vec3(0.f, -120.f, 0.f);
    desc.motion.movementInertiaScale = 0.0f;
    desc.motion.tipMovementInertiaScale = 0.14f;
    desc.motion.maxMovementOffset = 0.006f;
    desc.motion.solverIterations = 2;

    desc.collision.enableCollision = true;
    desc.collision.collisionRadiusPadding = 0.14f;
    desc.collision.physicsAssetPath = GetPhysicsAssetPath(characterType);
    desc.collision.includedBones = GetCoatCollisionBones(characterType);
    desc.collision.debugDrawCapsules = false;

    desc.runtime.ownerTransform = ownerTransform;
    desc.runtime.windDirection = kCommonWindDirection;
    desc.runtime.windScale = 0.f;
    desc.runtime.useOwnerMotionInertia = false;
    desc.runtime.useChainRootMotionInertia = false;
    desc.runtime.transportParticlesWithAnimatedPose = true;
    desc.runtime.animatedPoseTransportAlpha = 0.88f;
    desc.runtime.animatedPoseTransportDeadzone = 0.025f;
    desc.runtime.useAnimatedPoseAsRest = true;
    desc.blendMode = AnimationModifier::ModifierBlendMode::ModifyReplace;

    if (characterType == PlayerCharacterType::Maelle)
    {
        desc.groups = {
            { "Maelle_V2_Coat_01_01"},
            { "Maelle_V2_Coat_02_01"},
            { "Maelle_V2_Coat_03_01"}
        };
        return;
    }

    if (characterType == PlayerCharacterType::Lune)
    {
        desc.collision.collisionRadiusPadding = 0.18f;
        desc.motion.solverIterations = 2;
        desc.groups = {
            { "Lune_V3_Coat_01_01"},
            { "Lune_V3_Coat_02_01"},
            { "Lune_V3_Coat_03_01"},
            { "Lune_V3_Coat_04_01"},
            { "Lune_V3_Coat_05_01", 0.94f, 1.05f, 1.f, 1.18f, 0.96f, 1.02f },
            { "Lune_V3_Coat_06_01", 0.94f, 1.05f, 1.f, 1.18f, 0.96f, 1.02f },
            { "Lune_V3_Coat_07_01", 0.94f, 1.05f, 1.f, 1.18f, 0.96f, 1.02f },
            { "Lune_CoatCollar_r_A_01", 1.08f, 0.88f, 1.f, 0.2f, 1.12f, 0.68f },
            { "Lune_CoatCollar_r_B_01", 1.08f, 0.88f, 1.f, 0.2f, 1.12f, 0.68f },
            { "Lune_CoatCollar_l_01", 1.08f, 0.88f, 1.f, 0.2f, 1.12f, 0.68f }
        };
        return;
    }

    if (characterType == PlayerCharacterType::Gustave)
    {
        desc.groups = {
            { "Noah_V2Coat_00_03", 0.94f, 1.05f, 1.f, 1.14f, 0.96f, 1.02f },
            { "Noah_V2Coat_01_03", 0.94f, 1.05f, 1.f, 1.14f, 0.96f, 1.02f },
            { "Noah_V2Coat_02_03", 0.94f, 1.05f, 1.f, 1.14f, 0.96f, 1.02f },
            { "Noah_V2Coat_03_04", 0.98f, 1.02f, 1.f, 1.02f, 0.98f, 0.92f },
            { "Noah_V2Coat_04_03", 0.94f, 1.05f, 1.f, 1.14f, 0.96f, 1.02f },
            { "Noah_V2Coat_05_03", 0.94f, 1.05f, 1.f, 1.14f, 0.96f, 1.02f },
            { "Noah_V2Coat_06_03", 0.94f, 1.05f, 1.f, 1.14f, 0.96f, 1.02f },
            { "Noah_V2Coat_07_03", 0.94f, 1.05f, 1.f, 1.14f, 0.96f, 1.02f },
            { "Noah_V2Coat_08_03", 0.94f, 1.05f, 1.f, 1.14f, 0.96f, 1.02f }
        };
    }

    return;
}

NS_END

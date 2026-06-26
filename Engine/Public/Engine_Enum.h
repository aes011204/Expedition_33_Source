#pragma once
#include "EnumFlags.h"

namespace Engine
{
    enum class WinMode { Full, Win };

    enum class Prototype { GameObject, UI, Comopnent };

    enum class RenderGroup
    {
        Priority,
        Shadow,
        NonBlend,
        NonLight,
        EffectMasked, // Mask-style effect
        Distortion,   // Distortion effect
        Blend,
        Glow,
        UI,
        UIEffect,
        AmbientVolume,
        END
    };

    enum class ShadowCasterType : uint8
    {
        None,       // No shadow
        Dynamic,    // Shadow pass every frame
    };

    enum class UI_RenderGroup
    {
        // UI render order. Extend later if needed.
        Background, // Staged UI rendered behind the player
        Foreground, // Regular UI drawn over the 3D scene without depth
        Overlay,    // Highest-priority UI drawn after UIEffect
    };

    enum class State { Right, Up, Look, Position, END };

    enum class D3DTS { View, Proj, END };

    enum class LightType { Directional, Point, END };

    enum class ModelType { NonAnim, Anim, END };

    enum class LogLevel
    {
        Trace,
        Debug,
        Info,
        Warn,
        Error,
        Critical,
        Off
    };

    enum class ObjectType
    {
        None,
        Player,
        Monster,
        Character,
        NPC,

        FreeCamera,
        TargetCamera,
        PreviewCamera,
        BattleTurnCamera,
        CinematicCamera,
        DialogueCamera,
        SoulLikeCamera,

        Terrain,
        Landscape,
        Sky,
        StaticMesh,
        Water,
        Foliage,
        Weapon,
        PartObject,
        Projectile,
        WorldActionTrigger,
        Effect,
        EffectEmitter,
        Trigger,
        UI,
        BattleStartSpawner,
        PuzzleTarget,
        SoulLikeController,
        Trigger_Cinematic,

    };

    enum class DeferredPass : uint8
    {
        Debug,
        Directional,
        Point,
        CombinedHDR,
        CombinedSimple,
        CombinedPreview = 5, // No shadow calculation in preview
        SSAO,
        SSAOBlur,
        Fog,
        FroxelDebug,
        AmbientVisibilityResolve
    };

    enum class PostProcessPass : uint8
    {
        Debug,
        BloomExtractHalf,
        BloomDownsample,
        BloomBlurH,
        BloomBlurV,
        BloomUpsampleAdd,
        ToneMapping,
        DistortionResolve,
        FroxelCompositeHDR,
        LightShaftMaskHalf,
        LightShaftRadialBlurHalf,
        LightShaftCompositeHDR,
        DOFPreFilterHalf,
        DOFBlurNearHalf,
        DOFBlurFarHalf,
        DOFCompositeHDR,

        END
    };

    enum class CollisionShape : uint8_t
    {
        AABB,
        OBB,
        Sphere,
        Capsule,

        END
    };

    enum class VtxAnimMeshPass : uint8
    {
        Opaque = 0,
        Masked,
        OpaqueTwoSided,
        MaskedTwoSided,
        Shadow,
        HairBlend = 8,

        END
    };

    enum class ConstantBuffer : uint8
    {
        Material        = 0,
        Camera          = 1,   // View, Proj Matrix
        Object          = 2,   // WorldMatrix
        Shadow          = 3,
        ScreenSpace     = 4,
        Light           = 5,
        SSAO            = 6,
        DeferredLighting = 7,
        PostProcess     = 8,
        Hair            = 9,
        DrawFeature     = 10,
        ScreenFx        = 11,
        FogZone         = 12,
        Water           = 13,
        LightShaft      = 14,
        PlayerRender    = 15,
        Froxel          = 16,
        FroxelDebug     = 17,
        VolumetricLightShaftZone = 18,

        END
    };

    enum class MaterialPassKey : uint8
    {
        Opaque = 0,
        OpaqueTwoSided,
        Masked,
        MaskedTwoSided,

        END
    };

    enum class ShaderType : uint8
    {
        VtxTex = 0,
        VtxNorTex,
        VtxMesh,
        VtxAnimMesh,
        VtxCube,
        BillboardParticle,
        UI,
        VtxInstanceMesh,
        VtxInstanceMeshExact,
        Deferred,
        PostProcess,

        END
    };

    enum class PostProcessFeature : uint32
    {
        None = 0,
        HDR         = 1 << 0,
        ToneMapping = 1 << 1,
        Exposure    = 1 << 2,
        Bloom       = 1 << 3,
        Gamma       = 1 << 4,
        LightShaft  = 1 << 5,
        DOF         = 1 << 6,

        All = (1 << 7) - 1
    };

    enum class GameplayInputMode
    {
        Normal,         // Default world gameplay
        UIOnly,         // UI input only, block world input
        Cutscene,       // Block world input like a cutscene
        BattleMenu      // Battle menu selection
    };

    enum class SoundChannel : uint32
    {
        BGM = 0,
        Effect,
        UI,
        Player,
        Monster,
        Boss,
        Ambient,
        Voice,
        System,
        END
    };


    enum MaterialChannelSource : uint32
    {
        MaterialChannel_Const0 = 0,
        MaterialChannel_Const1,
        MaterialChannel_R,
        MaterialChannel_G,
        MaterialChannel_B,
        MaterialChannel_A,
        MaterialChannel_MaxRGB,
        MaterialChannel_Luma,
    };


}

template <> struct Enable_Bitmask_Operators<Engine::PostProcessFeature> { static constexpr bool value = true; };



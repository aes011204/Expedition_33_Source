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
        EffectMasked,
        Distortion, // Effect용 Distortion
        Blend,
        UI,
        UIEffect,
        END
    };

    enum class ShadowCasterType : uint8
    {
        None,       // 그림자 X
        Dynamic,    // 매 프레임 Shadow Pass
    };

    enum class UI_RenderGroup
    {
        // UI Render 순서. 필요하다면 추후에 더 확장
        Background, // 플레이어 뒤 연출용 UI
        Foreground, // depth를 무시하고 화면 최상단에 그리는 UI
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

        FreeCamera,
        TargetCamera,
        PreviewCamera,
        BattleTurnCamera,
        CinematicCamera,

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
    };

    enum class DeferredPass : uint8
    {
        Debug,
        Directional,
        Point,
        CombinedHDR,
        CombinedSimple,
        CombinedPreview = 5, // Preview에서는 그림자 계산 X
        SSAO,
        SSAOBlur,
        Fog
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
        LightShaftMaskHalf,
        LightShaftRadialBlurHalf,
        LightShaftCompositeHDR,

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

        All = (1 << 6) - 1
    };

    enum class GameplayInputMode
    {
        Normal,         // 기본 월드 플레이
        UIOnly,         // UI 입력만 허용, 월드 입력차단
        Cutscene,       // 컷신처럼 월드 입력 차단
        BattleMenu      // 배틀 메뉴 선택
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


#pragma once
#include "EnumFlags.h"

namespace Engine
{
    enum class WinMode { Full, Win };

    enum class Prototype { GameObject, UI, Comopnent };

    enum class RenderGroup { Priority, Shadow, NonBlend, NonLight, Blend, UI, END };

    enum class ShadowCasterType : uint8
    {
        None,       // 그림자 X
        Dynamic,    // 매 프레임 Shadow Pass
        Static,     // Map Static Mesh 등
    };

    enum class UI_RenderGroup
    {
        // UI Render 순서. 필요하다면 추후에 더 확장
        Background, // 플레이어보다 뒤에 그려질거
        Foreground, // 플레이어 앞에
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

        Terrain,
        Sky,
        StaticMesh,
        Foliage,
        Weapon,
        PartObject,
        Projectile,
        Effect,
        EffectEmitter,
        Trigger,
        UI,
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
        SSAOBlur
    };

    enum class PostProcessPass : uint8
    {
        Debug,
        BloomExtract,
        BloomDownsample,
        BloomBlurH,
        BloomBlurV,
        ToneMapping,

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

        END
    };

    enum class ConstantBuffer : uint8
    {
        Material    = 0,
        Camera      = 1,   // View, Proj Matrix
        Object      = 2,   // WorldMatrix
        Shadow      = 3,
        Deferred    = 4,
        Light       = 5,
        SSAO        = 6,
        PostProcess = 7,

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

        All = (1 << 5) - 1
    };


}

template <> struct Enable_Bitmask_Operators<Engine::PostProcessFeature> { static constexpr bool value = true; };


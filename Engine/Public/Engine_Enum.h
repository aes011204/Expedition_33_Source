#pragma once

namespace Engine
{
    enum class WinMode { Full, Win };

    enum class Prototype { GameObject, UI, Comopnent };

    enum class RenderGroup { Priority, Shadow, NonBlend, NonLight, Blend, UI, END };

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

        Terrain,
        Sky,
        StaticMesh,
        Foliage,
        Weapon,
        PartObject,
        Projectile,
        Effect,
        Trigger,
        UI,
    };

    enum class DeferredPass : uint8_t
    {
        Debug,
        Directional,
        Point,
        Combined,
    };

    enum class CollisionShape : uint8_t
    {
        AABB,
        OBB,
        Sphere,
        Capsule,

        END
    };

    enum class VtxAnimMeshPass : uint8_t
    {
        Opaque = 0,
        Masked,
        OpaqueTwoSided,
        MaskedTwoSided,
        Shadow,

        END
    };

    enum class ConstantBuffer : uint8_t
    {
        Material = 0,

        END
    };

    enum class MaterialPassKey : uint8_t
    {
        Opaque = 0,
        OpaqueTwoSided,
        Masked,
        MaskedTwoSided,

        END
    };

    enum class ShaderType : uint8_t
    {
        VtxTex = 0,
        VtxNorTex,
        VtxMesh,
        VtxAnimMesh,
        VtxCube,
        BillboardParticle,
        UI,
        VtxInstanceMesh,

        END
    };

}

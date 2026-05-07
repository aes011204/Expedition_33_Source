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
        Weapon,
        PartObject,
        Projectile,
        Effect,
        Trigger,
        UI,
    };

    enum class DeferredPass : uint8
    {
        Debug,
        Directional,
        Point,
        Combined,
    };

    enum class CollisionShape : uint8
    {
        AABB,
        OBB,
        Sphere,
        Capsule,

        END
    };

}

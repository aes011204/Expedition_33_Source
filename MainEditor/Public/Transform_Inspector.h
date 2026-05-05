#pragma once

#include "Component_Inspector.h"

NS_BEGIN(MainEditor)

class Transform_Inspector final : public Component_Inspector
{
public:
    enum class ETransformSpace : uint8
    {
        Local,
        World
    };

public:
    Transform_Inspector() = default;
    ~Transform_Inspector() override = default;

public:
    void Draw_Inspector(const Shared<Component>& component) override;

private:
    static bool Draw_SpaceSelector(ETransformSpace& space);

private:
    static bool XYZ_DragFloat(
        const char* label,
        const ImVec4& color,
        float& value,
        float speed,
        const char* uniqueId,
        float fieldWidth,
        const char* format = "%.3f");

    static bool Draw_XYZRow(
        const char* id,
        Vec3& value,
        float speed,
        const char* format = "%.3f");

private:
    inline static ETransformSpace _space = ETransformSpace::Local;

public:
    static Shared<Transform_Inspector> Create();

};

NS_END

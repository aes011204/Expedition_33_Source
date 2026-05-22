#pragma once

#include "Component_Inspector.h"

NS_BEGIN(Engine)
class TransformCom;
NS_END

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
    enum class EClipboardTarget : uint8
    {
        Position,
        Rotation,
        Scale
    };

    struct TransformValueClipboard
    {
        Vec3 value = Vec3::Zero;
        EClipboardTarget target = EClipboardTarget::Position;
        ETransformSpace space = ETransformSpace::Local;
        bool hasValue = false;
    };

    static bool Draw_SpaceSelector(ETransformSpace& space);
    static bool Draw_TransformClipboardContextMenu(
        const Shared<TransformCom>& transform,
        EClipboardTarget target,
        Vec3& displayedValue,
        bool isWorld,
        const ImVec2& popupMin,
        const ImVec2& popupMax);
    static const char* Get_TargetLabel(EClipboardTarget target);

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
    inline static TransformValueClipboard _transformClipboard{};

public:
    static Shared<Transform_Inspector> Create();

};

NS_END

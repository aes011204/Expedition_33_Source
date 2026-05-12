#pragma once

#include "Component_Inspector.h"
#include "Collision_Define.h"
#include "Engine_Enum.h"

NS_BEGIN(MainEditor)

class Collider_Inspector : public Component_Inspector
{
public:
    Collider_Inspector() = default;
    ~Collider_Inspector() override = default;

public:
    void Draw_Inspector(const Shared<Component>& component) override;

private:
    static string Get_ShapeLabel(CollisionShape shape);
    static string Get_ChannelLabel(Collision_Channel channel);
    static string Get_PresetLabel(Collision_Preset preset);

public:
    static Shared<Collider_Inspector> Create();
};

NS_END

#pragma once
#include "Component_Inspector.h"

NS_BEGIN(MainEditor)

class AIController_Inspector final : public Component_Inspector
{
public:
    AIController_Inspector() = default;
    ~AIController_Inspector() override = default;

public:
    void Draw_Inspector(const Shared<Component>& component) override;

private:

public:
    static Shared<AIController_Inspector> Create();
};

NS_END

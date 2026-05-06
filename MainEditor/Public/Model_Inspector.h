#pragma once

#include "Component_Inspector.h"

NS_BEGIN(MainEditor)

class ModelCom_Inspector final : public Component_Inspector
{
public:
    ModelCom_Inspector() = default;
    ~ModelCom_Inspector() override = default;

public:
    void Draw_Inspector(const Shared<Component>& component) override;

private:
    static const char* Get_TextureTypeLabel(uint32 typeIndex);

public:
    static Shared<ModelCom_Inspector> Create();
};

NS_END

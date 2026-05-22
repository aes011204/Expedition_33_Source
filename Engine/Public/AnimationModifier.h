#pragma once
#include "Component.h"

NS_BEGIN(Engine)

    class ModelCom;

class ENGINE_DLL AnimationModifier : public Base
{
public:
    AnimationModifier() = default;
    virtual ~AnimationModifier() override = default;

    virtual void Evaluate_Modifier(ModelCom* model, float timeDelta, float alpha) PURE;

    void Set_Active(bool isActive) { _isActive = isActive; }
    bool Is_Active() const { return _isActive; }

protected:
    bool _isActive = true;

};

NS_END

#pragma once
#include "AnimNotify.h"
NS_BEGIN(Engine)
class GameObject;
NS_END
NS_BEGIN(Client)


class AN_Dissolve_Parts : public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_Dissolve_Parts)

public:
    enum class DissolveType
    {
        Switch, Vanish, END
    };

public:
    string Get_TypeName() const override { return "AN_Dissolve_Parts"; }
    void Execute(const AnimNotifyContext& context) override;

private:
    DissolveType _type = DissolveType::END;
    float _duration = 0.7f;

    wstring  _partName = L"";

    GameObject* Part = {};

};

NS_END

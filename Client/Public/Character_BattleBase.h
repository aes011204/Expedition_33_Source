#pragma once

#include "Character.h"

NS_BEGIN(Engine)
class ColliderCom;
NS_END

NS_BEGIN(Client)

class Character_BattleBase abstract : public Character
{
public:
    Character_BattleBase(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Character_BattleBase(const Character_BattleBase& prototype);
    virtual ~Character_BattleBase() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    Late_Update(float timeDelta) override;

protected:
    HRESULT Ready_Collider(const wstring& prototypeTag, void* colliderDesc, Collision_Preset preset);
    void    Update_Collider();

protected:
    Shared<ColliderCom> _colliderCom;

public:
    void Free() override;

};

NS_END

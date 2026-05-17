#pragma once

#include "Character_WorldBase.h"

NS_BEGIN(Engine)
class AIController;
NS_END

NS_BEGIN(Client)

class MonsterStatCom;

class Monster_WorldBase abstract : public Character_WorldBase
{
public:
    Monster_WorldBase(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Monster_WorldBase(const Monster_WorldBase& prototype);
    virtual ~Monster_WorldBase() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    Update(float timeDelta) override;
    void    Late_Update(float timeDelta) override;
    HRESULT Render() override;
    HRESULT Render_Shadow() override;

public:
    void            OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;
    virtual void    Set_BattleMonsterPrefabNames(vector<string>& outPrefabNames) const = 0;

protected:
    virtual HRESULT Ready_Components();

protected:
    Shared<AIController>         _aiCtrl;
    Shared<MonsterStatCom>       _combatStat;

    bool                         _isBattleStart = false;

public:
    void Free() override;

};

NS_END

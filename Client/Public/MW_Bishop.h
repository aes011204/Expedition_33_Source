#pragma once
#include "Monster_WorldBase.h"

NS_BEGIN(Client)

class Mon_Lancelier_Weapon;

class Mon_Bishop_Body;

class MW_Bishop :
    public Monster_WorldBase
{
    GENERATED_GAMEOBJECT(MW_Bishop, ObjectType::Monster)

public:
    MW_Bishop(const ComPtr<Device>& device, const ComPtr<Context>& context);
    MW_Bishop(const MW_Bishop& prototype);
    virtual ~MW_Bishop() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    Priority_Update(float timeDelta) override;
    void    Update(float timeDelta) override;
    HRESULT Render() override;
    HRESULT Render_Shadow() override;

public:
    void    OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;

protected:
    HRESULT Ready_Components() override;
    HRESULT Ready_PartObjects();

    virtual void    Set_BattleMonsterPrefabNames(vector<string>& outPrefabNames)const override;
private:
    Shared<Mon_Bishop_Body>      _body = {};
    //Shared<Mon_Lancelier_Weapon>      _weapon = {};

public:

public:
    static Shared<MW_Bishop> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END

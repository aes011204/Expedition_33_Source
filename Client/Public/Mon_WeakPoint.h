#pragma once

#include "Client_Defines.h"
#include "Monster_Part.h"

NS_BEGIN(Engine)
class ColliderCom;
NS_END

NS_BEGIN(Client)

class Monster_BattleBase;

class Mon_WeakPoint : public Monster_Part
{
    GENERATED_GAMEOBJECT(Mon_WeakPoint, ObjectType::Weapon)

public:
    struct  WEAK_DESC : Monster_Part::MONSTER_PART_DESC
    {
        const Matrix* socketMatrix = nullptr;
        string socketName{};
    };

public:
    Mon_WeakPoint(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Mon_WeakPoint(const Mon_WeakPoint& prototype);
    virtual ~Mon_WeakPoint() override = default;

public:
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void* arg) override;
    virtual void BeginPlay() override;
    virtual void Priority_Update(float timeDelta) override;
    virtual void Update(float timeDelta) override;
    virtual void Late_Update(float timeDelta) override;
    virtual HRESULT Render() override;
    virtual HRESULT Render_Shadow() override;
    virtual void From_Json(const json& data) override;
    virtual Matrix Get_GizmoWorldMatrix() const override;
    virtual bool Apply_GizmoWorldMatrix(const Matrix& gizmoWorldMatrix) override;

public:
    bool Try_Consume_BattleWeakPointHit();
    void Hide_BattleWeakPoint();
    void Reset_BattleWeakPoint();
    bool Is_BattleWeakPointEnabled() const { return _isBattleWeakPointEnabled; }
    Matrix Get_CollisionWorldMatrix() const;

    Shared<Monster_BattleBase> Resolve_BattleMonsterOwner() const;

private:
    virtual uint32 Decide_PassIndex(Shared<Material> mat) override;

private:
    const Matrix* Resolve_SocketMatrix();
    HRESULT       Ready_WeakPointCollider();
    void          Ready_DefaultWeakPointEffectSettings();
    void          Play_WeakPointEffect();

private:
    const Matrix* _socketMatrix = nullptr;
    string _socketName{};
    Matrix _offsetMatrix = {};

    Shared<ColliderCom> _colliderCom = {};
    bool _isBattleWeakPointEnabled = true;

public:
    static Shared<Mon_WeakPoint> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<GameObject> Clone(void* arg) override;
    virtual void Free() override;
};

NS_END

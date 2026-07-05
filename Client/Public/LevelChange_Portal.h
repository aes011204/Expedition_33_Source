#pragma once

#include "GameObject.h"

NS_BEGIN(Engine)
class ColliderCom;
NS_END

NS_BEGIN(Client)

class EffectCom;

class LevelChange_Portal : public GameObject
{
    GENERATED_GAMEOBJECT(LevelChange_Portal, ObjectType::Trigger)

public:
    LevelChange_Portal(const ComPtr<Device>& device, const ComPtr<Context>& context);
    LevelChange_Portal(const LevelChange_Portal& prototype);
    virtual ~LevelChange_Portal() = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    BeginPlay() override;
    void    Update(float timeDelta) override;
    void    Late_Update(float timeDelta) override;
    void    OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;
    void    OnStayOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;
    void    OnEndOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;

private:
    HRESULT Ready_Components();
    void    Play_PortalEffect();
    bool    Has_ActiveHeroOverlap() const;

private:
    Shared<ColliderCom> _colliderCom{};
    Shared<EffectCom>   _effectCom{};

    LevelType _targetLevel = LevelType::GameEntry;
    float _effectScale = 2.0f;
    uset<ColliderCom*> _playerOverlapColliders{};
    bool _transitionRequested = false;

public:
    static Shared<LevelChange_Portal> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END

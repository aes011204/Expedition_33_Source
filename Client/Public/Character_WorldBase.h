#pragma once

#include "Character.h"

NS_BEGIN(Engine)
class ColliderCom;
NS_END

NS_BEGIN(Client)

// Character 기반 WorldBase -> 이걸 상속받아서 Player_WorldBase, Monster_WorldBase로 나눠진다.
class Character_WorldBase abstract : public Character
{
public:
    Character_WorldBase(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Character_WorldBase(const Character_WorldBase& prototype);
    virtual ~Character_WorldBase() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    BeginPlay() override;
    void    Late_Update(float timeDelta) override;

protected:
    HRESULT Ready_Collider(const wstring& prototypeTag, void* colliderDesc, Collision_Preset preset);
    void    Update_Collider();
    void    Update_Culling(const Vec3& extents);

protected:
    Shared<ColliderCom> _colliderCom;
    BoundingBox         _cullingAABB{};

public:
    void Free() override;
};

NS_END

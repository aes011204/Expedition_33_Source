#include "MW_Lancelier.h"
#include "ClientInstance.h"
#include "AIController.h"
#include "GameInstance.h"
#include "Mon_Lancelier_Body.h"
#include "Mon_Lancelier_Weapon.h"
#include "Mon_WeakPoint.h"

IMPLEMENT_REFLECTION(MW_Lancelier)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "MW_Lancelier";
    info.category = "Monster";

    return true;
}

MW_Lancelier::MW_Lancelier(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_WorldBase(device, context)
{
    _worldMoveSpeedScale = 0.4f;
}

MW_Lancelier::MW_Lancelier(const MW_Lancelier& prototype)
    : Monster_WorldBase(prototype)
{
}

HRESULT MW_Lancelier::Initialize_Prototype()
{
    return Monster_WorldBase::Initialize_Prototype();
}

HRESULT MW_Lancelier::Initialize(void* arg)
{
    CHECK_FAILED(Monster_WorldBase::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_PartObjects(), E_FAIL);

    const Vec3 patrolOrigin = _transformCom->Get_Position();
    static const Vec3 patrolOffsets[] = {
        { 0.f, 0.f, 0.f },
        { 8.f, 0.f, -7.f },
        { 12.f, 0.f, 0.f },
        { 8.f, 0.f, 7.f },
        { 0.f, 0.f, 0.f },
        { -8.f, 0.f, -7.f },
        { -12.f, 0.f, 0.f },
        { -8.f, 0.f, 7.f },
        { 0.f, 0.f, 0.f }
    };

    vector<Vec3> patrolPoints;
    patrolPoints.reserve(_countof(patrolOffsets));

    for (const Vec3& offset : patrolOffsets)
        patrolPoints.push_back(patrolOrigin + offset);

    Set_PatrolPoints(std::move(patrolPoints));

    return S_OK;
}

void MW_Lancelier::Priority_Update(float timeDelta)
{
    Monster_WorldBase::Priority_Update(timeDelta);


}

void MW_Lancelier::Update(float timeDelta)
{
    if (CLIENT->Is_BattleActive())
        return;

    Monster_WorldBase::Update(timeDelta);
}

HRESULT MW_Lancelier::Render()
{
    return S_OK;
}

HRESULT MW_Lancelier::Render_Shadow()
{
    return S_OK;
}

void MW_Lancelier::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    Monster_WorldBase::OnBeginOverlap(self, other);

}

HRESULT MW_Lancelier::Ready_Components()
{
    CHECK_FAILED(Monster_WorldBase::Ready_Components(), E_FAIL);

    return S_OK;
}

HRESULT MW_Lancelier::Ready_PartObjects()
{
    Mon_Lancelier_Body::BODY_DESC bodyDesc{};
    bodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bodyDesc.modelType = ModelType::Anim;
    bodyDesc.modelTag = L"Model_Lancelier";

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Lancelier_Body"), TEXT("Part_Body"), &bodyDesc), E_FAIL); // 공용 몬스터 파츠 prototype은 Static에서 꺼낸다.

    _body = dynamic_pointer_cast<Mon_Lancelier_Body>(_partObjects[TEXT("Part_Body")]);
    CHECK_NULL(_body, E_FAIL);

    Mon_Lancelier_Weapon::WEAPON_DESC weaponDesc{};
    weaponDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    weaponDesc.modelType = ModelType::NonAnim;
    weaponDesc.modelTag = L"Model_Lancelier_Weapon";
    weaponDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("Weapon_R");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Lancelier_Weapon"), TEXT("Part_Weapon"), &weaponDesc), E_FAIL); // MapTest/GamePlay 공용 무기 파츠도 Static prototype을 재사용한다.

    _weapon = dynamic_pointer_cast<Mon_Lancelier_Weapon>(_partObjects[TEXT("Part_Weapon")]);
    CHECK_NULL(_weapon, E_FAIL);


    Mon_WeakPoint::WEAK_DESC weakDesc{};
    weakDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    weakDesc.allowMissingModel = true;
    weakDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("neck_02");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_MonsterWeak"), TEXT("Part_Weak"), &weakDesc), E_FAIL); 

    _weak = dynamic_pointer_cast<Mon_WeakPoint>(_partObjects[TEXT("Part_Weak")]);
    CHECK_NULL(_weapon, E_FAIL);


    return S_OK;
}

Shared<MW_Lancelier> MW_Lancelier::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<MW_Lancelier>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : MW_Lancelier");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> MW_Lancelier::Clone(void* arg)
{
    auto clone = make_shared<MW_Lancelier>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void MW_Lancelier::Free()
{
    Monster_WorldBase::Free();
}

#include "MW_Bishop.h"
#include "ClientInstance.h"
#include "Mon_Bishop_Body.h"
#include "Mon_Lancelier_Weapon.h"

IMPLEMENT_REFLECTION(MW_Bishop)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "MW_Bishop";
    info.category = "Monster";

    return true;
}


MW_Bishop::MW_Bishop(const ComPtr<Device>& device, const ComPtr<Context>& context) :Monster_WorldBase(device, context)
{
}

MW_Bishop::MW_Bishop(const MW_Bishop& prototype) : Monster_WorldBase(prototype)
{
}

HRESULT MW_Bishop::Initialize_Prototype()
{
    return Monster_WorldBase::Initialize_Prototype();
}

HRESULT MW_Bishop::Initialize(void* arg)
{
    CHECK_FAILED(Monster_WorldBase::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_PartObjects(), E_FAIL);

    return S_OK;
}

void MW_Bishop::Priority_Update(float timeDelta)
{
    Monster_WorldBase::Priority_Update(timeDelta);
}

void MW_Bishop::Update(float timeDelta)
{
    if (CLIENT->Is_BattleActive())
        return;

    Monster_WorldBase::Update(timeDelta);

    if (_body)
    {
        _body->Play_Animation(timeDelta);

        // Lancelier 월드 이동은 기존 Monster와 동일하게 body root motion 속도를 사용한다.
        const auto rootVelocity = _body->Get_RootTransformVelocity();
        _transformCom->Set_SpeedPerSec(rootVelocity.position.z * 0.4f/*Constants::Charcter::scaleOffset*/);
    }
}

HRESULT MW_Bishop::Render()
{
    return S_OK;
}

HRESULT MW_Bishop::Render_Shadow()
{
    return S_OK;
}

void MW_Bishop::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    Monster_WorldBase::OnBeginOverlap(self, other);
}

HRESULT MW_Bishop::Ready_Components()
{
    Monster_WorldBase::Ready_Components();
    return S_OK;
}

HRESULT MW_Bishop::Ready_PartObjects()
{
    Mon_Bishop_Body::BODY_DESC bodyDesc{};
    bodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bodyDesc.modelType = ModelType::Anim;
    bodyDesc.modelTag = L"Model_Bishop";

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Bishop_Body"), TEXT("Part_Body"), &bodyDesc), E_FAIL);

    _body = dynamic_pointer_cast<Mon_Bishop_Body>(_partObjects[TEXT("Part_Body")]);
    CHECK_NULL(_body, E_FAIL);


   // Mon_Lancelier_Weapon::WEAPON_DESC weaponDesc{};
   // weaponDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
   // weaponDesc.modelType = ModelType::NonAnim;
   // weaponDesc.modelTag = L"Model_Lancelier_Weapon";
   // //TODO
   //// weaponDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("Weapon_R");

   // CHECK_FAILED(Add_PartObject(ETOI(LevelType::GamePlay), TEXT("GameObject_Lancelier_Weapon"), TEXT("Part_Weapon"), &weaponDesc), E_FAIL);

   // _weapon = dynamic_pointer_cast<Mon_Lancelier_Weapon>(_partObjects[TEXT("Part_Weapon")]);
   // CHECK_NULL(_weapon, E_FAIL);

}

Shared<MW_Bishop> MW_Bishop::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<MW_Bishop>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : MW_Bishop");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> MW_Bishop::Clone(void* arg)
{
    auto clone = make_shared<MW_Bishop>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void MW_Bishop::Free()
{
    Monster_WorldBase::Free();
}

#include "Mon_Dualliste_Sword_R.h"

#include "Bounding_OBB.h"
#include "ColliderCom.h"
#include "EffectCom.h"

NS_BEGIN(Client)


IMPLEMENT_REFLECTION(Mon_Dualliste_Sword_R)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "Mon_Dualliste_Sword_R";
    info.category = "Weapon";


    return true;
}

Mon_Dualliste_Sword_R::Mon_Dualliste_Sword_R(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_Part(device, context)
{
}

Mon_Dualliste_Sword_R::Mon_Dualliste_Sword_R(const Mon_Dualliste_Sword_R& prototype)
    : Monster_Part(prototype)
{
}

HRESULT Mon_Dualliste_Sword_R::Initialize_Prototype()
{
    return Monster_Part::Initialize_Prototype();
}

HRESULT Mon_Dualliste_Sword_R::Initialize(void* arg)
{
    auto* desc = static_cast<SWORD_DESC*>(arg);
    SWORD_DESC fallbackDesc{};
    fallbackDesc.modelType = ModelType::NonAnim;
    fallbackDesc.modelTag = L"Model_Dualliste_Sword_R";

    if (desc == nullptr)
        desc = &fallbackDesc;

    _socketMatrix = desc->socketMatrix;

    _offsetMatrix = Matrix::CreateRotationX(XMConvertToRadians(-90.f));

    if (FAILED(Monster_Part::Initialize(desc)))
        return E_FAIL;

    return S_OK;
}

void Mon_Dualliste_Sword_R::Priority_Update(float timeDelta)
{
    Monster_Part::Priority_Update(timeDelta);
}

void Mon_Dualliste_Sword_R::Update(float timeDelta)
{
    Matrix socketMatrix = Matrix::Identity;
    if (_socketMatrix != nullptr)
        socketMatrix = *_socketMatrix;

    Update_CombinedWorldMatrix(_offsetMatrix * _transformCom->Get_WorldMatrix() * socketMatrix);

    if (_partEffectCom)
        _partEffectCom->Update(timeDelta);


    if (_attackRCom)
        _attackRCom->Update_Collider(Get_CombinedWorldMatrix());

    GAME->Add_Collider(_attackRCom, Get_CombinedWorldMatrix());

}

void Mon_Dualliste_Sword_R::Late_Update(float timeDelta)
{
    Monster_Part::Late_Update(timeDelta);
}

HRESULT Mon_Dualliste_Sword_R::Render()
{
    return Monster_Part::Render();
}

HRESULT Mon_Dualliste_Sword_R::Render_Shadow()
{
    return Monster_Part::Render_Shadow();
}

HRESULT Mon_Dualliste_Sword_R::Ready_Components(const MONSTER_PART_DESC& desc)
{

    Bounding_OBB::BoundingOBBDesc attackObbDesc{};
    attackObbDesc.center = Vec3(0.f, 1.0f, 1.0f);
    attackObbDesc.extents = Vec3(0.55f, 0.65f, 0.95f);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Collider_OBB"), _attackRCom, &attackObbDesc), E_FAIL);
    CHECK_FAILED(Rename_ComponentTag(TEXT("Com_Collider_OBB"), TEXT("Com_SL_MonsterSword_R")), E_FAIL);

    _attackRCom->Set_CollisionPreset(Collision_Preset::SL_MonsterAttack);
    _attackRCom->Set_IsActive(false);
    _attackRCom->Clear_Overlap();


    return Monster_Part::Ready_Components(desc);
}

uint32 Mon_Dualliste_Sword_R::Decide_PassIndex(Shared<Material> mat)
{
    const int opaquePass = 0;
    UNREFERENCED_PARAMETER(mat);
    return opaquePass;
}

Shared<Mon_Dualliste_Sword_R> Mon_Dualliste_Sword_R::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Mon_Dualliste_Sword_R>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> Mon_Dualliste_Sword_R::Clone(void* arg)
{
    auto clone = make_shared<Mon_Dualliste_Sword_R>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void Mon_Dualliste_Sword_R::Free()
{
    Monster_Part::Free();
}

NS_END

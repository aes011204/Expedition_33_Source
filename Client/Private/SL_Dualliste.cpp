#include "SL_Dualliste.h"

#include "Bounding_OBB.h"
#include "ClientInstance.h"
#include "ColliderCom.h"
#include "ModelCom.h"
#include "Mon_Dualliste_Body.h"
#include "Mon_Dualliste_Sword_L.h"
#include "Mon_Dualliste_Sword_R.h"
#include "MonsterStatCom.h"
#include "SL_DamageSystem.h"
#include "SL_Mon_StateMachineCom.h"
#include "AIController.h"
#include "ClientScreenFxManager.h"
#include "BlackBoard.h"
#include "ClientInstance.h"
#include "Player_WorldBase.h"

REGISTER_GLOBAL_ENUM(SL_Dualliste::Anim, "SL_DuallisteAnim")

IMPLEMENT_REFLECTION(SL_Dualliste)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "SL_Dualliste";
    info.category = "SoulLike";

    return true;
}

SL_Dualliste::SL_Dualliste(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : SL_Base(device, context)
{
}

SL_Dualliste::SL_Dualliste(const SL_Dualliste& prototype)
    : SL_Base(prototype)
{
}

HRESULT SL_Dualliste::Initialize_Prototype()
{
    return SL_Base::Initialize_Prototype();
}

HRESULT SL_Dualliste::Initialize(void* arg)
{
    CHECK_FAILED(SL_Base::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);
    CHECK_FAILED(Ready_PartObjects(), E_FAIL);

    return S_OK;
}

void SL_Dualliste::BeginPlay()
{
    SL_Base::BeginPlay();

   if (_attackRCom)
   {
       _attackRCom->Set_IsActive(false);
       _attackRCom->Clear_Overlap();
   }

   if (_attackLCom)
   {
       _attackLCom->Set_IsActive(false);
       _attackLCom->Clear_Overlap();
   }

   //_body->Set_AnimationByState(ETOI(Anim::Intro),0.f,1.f);

   if (_stateMachineCom)
       _stateMachineCom->Change_State(SL_MonState::Intro);
}

void SL_Dualliste::Priority_Update(float timeDelta)
{
    SL_Base::Priority_Update(timeDelta);
}

void SL_Dualliste::Update(float timeDelta)
{
    if (_combatStat == nullptr || _body == nullptr)
        return;

    if (CLIENT->Is_BattleActive())
        return;

    Update_BlackBoard();

    if (_aiCtrl && _isIntroDone)
        _aiCtrl->Update(timeDelta);

    if (_stateMachineCom)
        _stateMachineCom->Update_CurrentState(timeDelta);

    if (_body)
        _body->Play_Animation(timeDelta);

    SL_Base::Update(timeDelta);
}

void SL_Dualliste::Late_Update(float timeDelta)
{
    SL_Base::Late_Update(timeDelta);

    Update_Culling(Vec3(1.f, 2.f, 1.f));
}

HRESULT SL_Dualliste::Render()
{
    return SL_Base::Render();
}

HRESULT SL_Dualliste::Render_Shadow()
{
    return SL_Base::Render_Shadow();
}

void SL_Dualliste::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    SL_Base::OnBeginOverlap(self, other);

    if (self == nullptr || other == nullptr)
        return;

    if (self.get() != _attackRCom.get() &&
        self.get() != _attackLCom.get())
        return;

    if (other->Get_CollisionPreset() != Collision_Preset::SL_PlayerBody)
        return;

    const Shared<SL_Base> target = dynamic_pointer_cast<SL_Base>(other->Get_Owner());
    if (target == nullptr || target.get() == this || target->Is_Dead())
        return;

    SL_DamageInfo damageInfo{};
    if (!SL_DamageSystem::Apply_Damage(
        GetSharedPtr<GameObject>(),
        _combatStat,
        target,
        target->Get_Stat(),
        1.f,
        AttackType::BaseAttack,
        damageInfo))
    {
        return;
    }

    target->Apply_Damage(damageInfo);
}

Shared<CombatStatCom> SL_Dualliste::Get_Stat() const
{
    return _combatStat;
}

Shared<ColliderCom> SL_Dualliste::Get_AttackColliderL() const
{
    return _attackLCom;
}
Shared<ColliderCom> SL_Dualliste::Get_AttackColliderR() const
{
    return _attackRCom;
}

bool SL_Dualliste::Play_Anim(Anim anim, float blendDuration, bool loop)
{
    if (_body == nullptr)
        return false;

    if (_currentAnim == anim && loop)
        return true;

    if (_body->Set_AnimationByState(ETOI(anim), blendDuration, loop))
    {
        _currentAnim = anim;
        return true;
    }

    return false;
}

bool SL_Dualliste::Play_Anim(Anim anim, float blendDuration, bool loop, float startFrame)
{
    if (_body == nullptr)
        return false;

    if (_body->Set_AnimationByState(ETOI(anim), blendDuration, loop, startFrame))
    {
        _currentAnim = anim;
        return true;
    }

    return false;
}

void SL_Dualliste::Set_AnimPaused(bool paused)
{
    if (_body == nullptr || _body->Get_Model() == nullptr)
        return;

    _body->Get_Model()->Set_AnimationPaused(paused);
}

void SL_Dualliste::Set_AnimTimeSec(float timeSec)
{
    if (_body == nullptr || _body->Get_Model() == nullptr)
        return;

    _body->Get_Model()->Set_CurrentAnimationTimeSec(timeSec);
}

void SL_Dualliste::Refresh_AnimPose()
{
    if (_body == nullptr || _body->Get_Model() == nullptr)
        return;

    _body->Get_Model()->Refresh_CurrentAnimationPose();
}

bool SL_Dualliste::Is_AnimFinished() const
{
    return _body != nullptr && _body->Is_AnimationFinished();
}

bool SL_Dualliste::Is_Dead() const
{
    return _combatStat == nullptr || _combatStat->Get_AliveStatus() == EAliveStatus::Dead;
}

void SL_Dualliste::Apply_Damage(const SL_DamageInfo& damageInfo)
{
    if (damageInfo.target.get() != this)
        return;
}

void SL_Dualliste::Update_BlackBoard()
{
    if (_aiCtrl == nullptr || _transformCom == nullptr)
        return;

    const Shared<BlackBoard> bb = _aiCtrl->Get_BlackBoard();
    if (bb == nullptr)
        return;

    bb->Set_Value<bool>("IsDead", Is_Dead());

    const Shared<Player_WorldBase> player = CLIENT->Get_ActiveHero();
    if (player == nullptr || player->Get_Transform() == nullptr)
    {
        bb->Set_Value<bool>("CanAttack", false);
        return;
    }

    const Vec3 targetPos = player->Get_Transform()->Get_WorldPosition();
    bb->Set_Value<Vec3>("TargetPos", targetPos);

    const Vec3 myPos = _transformCom->Get_WorldPosition();
    const float distanceToTarget = Vec3::Distance(myPos, targetPos);
    bb->Set_Value<bool>("CanAttack", distanceToTarget <= 3.0f);
}

bool SL_Dualliste::Change_State(SL_MonState state)
{
	if (_stateMachineCom == nullptr)
		return false;

	return _stateMachineCom->Change_State(state);
}


HRESULT SL_Dualliste::Ready_Components()
{
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_AIController"), _aiCtrl), E_FAIL);
    _aiCtrl->Set_Owner(static_pointer_cast<SL_Dualliste>(shared_from_this()));

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_MonsterStat"), _combatStat), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_SL_Mon_StateMachine"), _stateMachineCom), E_FAIL);

    _combatStat->Set_RuntimeCombatProfile(150.f, 0, 18.f, 0.f, 0.f);

    Bounding_OBB::BoundingOBBDesc bodyObbDesc{};
    bodyObbDesc.center = Vec3(0.f, 1.2f, 0.f);
    bodyObbDesc.extents = Vec3(0.65f, 1.2f, 0.45f);

    CHECK_FAILED(Ready_Collider(L"Com_Collider_OBB", &bodyObbDesc, Collision_Preset::SL_MonsterBody), E_FAIL);
    CHECK_FAILED(Rename_ComponentTag(TEXT("Com_Collider_OBB"), TEXT("Com_SL_MonsterBody")), E_FAIL);

    return S_OK;
}

HRESULT SL_Dualliste::Ready_PartObjects()
{
    Mon_Dualliste_Body::BODY_DESC bodyDesc{};
    bodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bodyDesc.modelType = ModelType::Anim;
    bodyDesc.modelTag = L"Model_Dualliste";

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Dualliste_Body"), TEXT("Part_Body"), &bodyDesc), E_FAIL);

    _body = dynamic_pointer_cast<Mon_Dualliste_Body>(_partObjects[TEXT("Part_Body")]);
    CHECK_NULL(_body, E_FAIL);

    if (Shared<ModelCom> model = _body->Get_Model())
        model->Refresh_AnimStateMappingFromOwnerContext();

    Mon_Dualliste_Sword_L::SWORD_DESC swordLDesc{};
    swordLDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    swordLDesc.modelType = ModelType::NonAnim;
    swordLDesc.modelTag = L"Model_Dualliste_Sword_L";
    swordLDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("weapon_l");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Dualliste_Sword_L"), TEXT("Part_Sword_L"), &swordLDesc), E_FAIL);

    _swordL = dynamic_pointer_cast<Mon_Dualliste_Sword_L>(_partObjects[TEXT("Part_Sword_L")]);
    CHECK_NULL(_swordL, E_FAIL);

    Mon_Dualliste_Sword_R::SWORD_DESC swordRDesc{};
    swordRDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    swordRDesc.modelType = ModelType::NonAnim;
    swordRDesc.modelTag = L"Model_Dualliste_Sword_R";
    swordRDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("weapon_r");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Dualliste_Sword_R"), TEXT("Part_Sword_R"), &swordRDesc), E_FAIL);

    _swordR = dynamic_pointer_cast<Mon_Dualliste_Sword_R>(_partObjects[TEXT("Part_Sword_R")]);
    CHECK_NULL(_swordR, E_FAIL);

    return S_OK;
}

Shared<SL_Dualliste> SL_Dualliste::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<SL_Dualliste>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : SL_Dualliste");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> SL_Dualliste::Clone(void* arg)
{
    auto clone = make_shared<SL_Dualliste>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void SL_Dualliste::Free()
{
    SL_Base::Free();
}

#include "Mon_WeakPoint.h"
#include "ContainerObject.h"
#include "EffectCom.h"
#include "GameInstance.h"
#include "Monster_BattleBase.h"
#include "ColliderCom.h"
#include "Bounding_Sphere.h"

NS_BEGIN(Client)

Mon_WeakPoint::Mon_WeakPoint(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_Part(device, context)
{
}

Mon_WeakPoint::Mon_WeakPoint(const Mon_WeakPoint& prototype)
    : Monster_Part(prototype)
{
}

HRESULT Mon_WeakPoint::Initialize_Prototype()
{
    return Monster_Part::Initialize_Prototype();
}

HRESULT Mon_WeakPoint::Initialize(void* arg)
{
    auto* desc = static_cast<WEAK_DESC*>(arg);
    if (desc == nullptr)
        return E_FAIL;

    _socketMatrix = desc->socketMatrix;
    _socketName = desc->socketName;
    _offsetMatrix = Matrix::CreateRotationX(XMConvertToRadians(-90.f));

    if (FAILED(Monster_Part::Initialize(desc)))
        return E_FAIL;

    CHECK_FAILED(Ready_WeakPointCollider(), E_FAIL);

    return S_OK;
}

void Mon_WeakPoint::BeginPlay()
{
    Monster_Part::BeginPlay();

    // 약점 리셋
    Reset_BattleWeakPoint();
}

void Mon_WeakPoint::Priority_Update(float timeDelta)
{
    Monster_Part::Priority_Update(timeDelta);
}

void Mon_WeakPoint::Update(float timeDelta)
{
    if (const Matrix* socketMatrix = Resolve_SocketMatrix())
        Update_CombinedWorldMatrix(_offsetMatrix * _transformCom->Get_WorldMatrix() * (*socketMatrix));

    if (_partEffectCom)
        _partEffectCom->Update(timeDelta);

}

void Mon_WeakPoint::Late_Update(float timeDelta)
{
    Monster_Part::Late_Update(timeDelta);

    if (_colliderCom == nullptr || !_colliderCom->Get_IsActive() || !Is_Visible())
        return;

    const Matrix collisionWorldMatrix = Build_CollisionWorldMatrix();

    _colliderCom->Update_Collider(collisionWorldMatrix);

    GAME->Add_Collider(_colliderCom, collisionWorldMatrix);
}

HRESULT Mon_WeakPoint::Render()
{
    return Monster_Part::Render();
}

HRESULT Mon_WeakPoint::Render_Shadow()
{
    return Monster_Part::Render_Shadow();
}

const Matrix* Mon_WeakPoint::Resolve_SocketMatrix()
{
    if (!_socketName.empty())
    {
        const Shared<ContainerObject> ownerContainer = dynamic_pointer_cast<ContainerObject>(Get_Owner());
        if (ownerContainer)
        {
            if (auto* bodyPart = dynamic_cast<Monster_Part*>(ownerContainer->Get_PartObject(L"Part_Body")))
            {
                if (const Matrix* socketMatrix = bodyPart->Get_SocketBoneMatrixPtr(_socketName))
                {
                    _socketMatrix = socketMatrix;
                    return _socketMatrix;
                }
            }
        }
    }

    return _socketMatrix;
}

HRESULT Mon_WeakPoint::Ready_WeakPointCollider()
{
    Bounding_Sphere::FBoundingSphereDesc sphereDesc{};
    sphereDesc.center = Vec3::Zero;
    sphereDesc.radius = 0.32f;

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Collider_Sphere"), _colliderCom, &sphereDesc), E_FAIL);
    _colliderCom->Set_CollisionPreset(Collision_Preset::Monster_Body);
    _colliderCom->Set_IsActive(true);

    return S_OK;
}

void Mon_WeakPoint::Play_WeakPointEffect()
{
    if (_partEffectCom == nullptr)
        return;

    _partEffectCom->Stop_AllAttachedEffects();

    EffectCom::EffectPlayDesc desc{};
    desc.levelIndex = GAME->Current_LevelIndex();
    desc.effectName = L"Monster_Weak";
    desc.target = EffectCom::EffectPlayTarget::OwnerRoot;
    desc.layerTag = L"Layer_Effect";
    desc.followTarget = true;
    desc.scale = Vec3(0.3f);
    desc.requiresManualRelease = true;

    _partEffectCom->Play_Effect(desc);
}

Matrix Mon_WeakPoint::Build_CollisionWorldMatrix() const
{
    Vec3 scale = Vec3::One;
    Quat rotation = Quat::Identity;
    Vec3 translation = Vec3::Zero;

    Matrix collisionSourceMatrix = _combinedWorldMatrix;
    if (!collisionSourceMatrix.Decompose(scale, rotation, translation))
        return _combinedWorldMatrix;

    // 소켓/모델 스케일은 prefab collider 크기를 축소시키므로, collision은 위치/회전만 따라간다.
    return Matrix::CreateFromQuaternion(rotation) * Matrix::CreateTranslation(translation);
}

bool Mon_WeakPoint::Try_Consume_BattleWeakPointHit()
{
    if (!_isBattleWeakPointEnabled)
        return false;

    _isBattleWeakPointEnabled = false;
    Set_Visible(false);

    if (_colliderCom)
    {
        _colliderCom->Set_IsActive(false);
        _colliderCom->Clear_Overlap();
    }

    if (_partEffectCom)
        _partEffectCom->Stop_AllAttachedEffects();

    return true;
}

void Mon_WeakPoint::Hide_BattleWeakPoint()
{
    _isBattleWeakPointEnabled = false;
    Set_Visible(false);

    if (_colliderCom)
    {
        _colliderCom->Set_IsActive(false);
        _colliderCom->Clear_Overlap();
    }

    if (_partEffectCom)
        _partEffectCom->Stop_AllAttachedEffects();
}

void Mon_WeakPoint::Reset_BattleWeakPoint()
{
    _isBattleWeakPointEnabled = true;
    Set_Visible(true);

    if (_colliderCom)
    {
        _colliderCom->Set_IsActive(true);
        _colliderCom->Clear_Overlap();
    }

    Play_WeakPointEffect();
}

Shared<Monster_BattleBase> Mon_WeakPoint::Resolve_BattleMonsterOwner() const
{
    return dynamic_pointer_cast<Monster_BattleBase>(Get_Owner());
}

uint32 Mon_WeakPoint::Decide_PassIndex(Shared<Material> mat)
{
    const int OPAQUE_PASS = 0;
    return OPAQUE_PASS;
}

Shared<Mon_WeakPoint> Mon_WeakPoint::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Mon_WeakPoint>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> Mon_WeakPoint::Clone(void* arg)
{
    auto clone = make_shared<Mon_WeakPoint>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void Mon_WeakPoint::Free()
{
    Monster_Part::Free();
}

NS_END

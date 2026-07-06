#include "Mon_Simon_Hair.h"

#include "EffectCom.h"
#include "ChainModifierPreset.h"
#include "ModelCom.h"

NS_BEGIN(Client)

Mon_Simon_Hair::Mon_Simon_Hair(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_Part(device, context)
{
}

Mon_Simon_Hair::Mon_Simon_Hair(const Mon_Simon_Hair& prototype)
    : Monster_Part(prototype)
{
}

HRESULT Mon_Simon_Hair::Initialize_Prototype()
{
    return Monster_Part::Initialize_Prototype();
}

HRESULT Mon_Simon_Hair::Initialize(void* arg)
{
    auto* desc = static_cast<HEAD_DESC*>(arg);
    if (desc == nullptr)
        return E_FAIL;

    _socketMatrix = desc->socketMatrix;
    _collisionModel = desc->collisionModel;
    _collisionOwnerWorldMatrix = desc->collisionOwnerWorldMatrix;

    if (FAILED(Monster_Part::Initialize(desc)))
        return E_FAIL;

    if (_model)
    {
        _model->Set_LocalRootNode(_model->Get_BoneIndex("Simon_Strand_Hair_root"));
    }

    CHECK_FAILED(Ready_PhysicsModifier(), E_FAIL);

    return S_OK;
}

HRESULT Mon_Simon_Hair::Ready_PhysicsModifier()
{
    CHECK_NULL(_model, E_FAIL);

    PhysicsModifier::PhysicsModifierDesc hairDesc{};
    ChainModifierPreset::BuildSimonHairPhysicsDesc(
        hairDesc,
        _transformCom.get());

    if (hairDesc.chains.empty())
        return E_FAIL;

    hairDesc.ownerWorldMatrix = &_combinedWorldMatrix;
    hairDesc.collisionModel = _collisionModel;
    hairDesc.collisionOwnerWorldMatrix = _collisionOwnerWorldMatrix;

    _hairPhysicsModifier = PhysicsModifier::Create(&hairDesc);
    if (_hairPhysicsModifier == nullptr)
    {
        _hairPhysicsReady = false;
        return E_FAIL;
    }

    _hairPhysicsModifier->Bind_Model(_model.get());
    _hairPhysicsReady = true;
    return S_OK;
}

void Mon_Simon_Hair::Priority_Update(float timeDelta)
{
    Monster_Part::Priority_Update(timeDelta);
}

void Mon_Simon_Hair::Update(float timeDelta)
{
    if (_socketMatrix != nullptr)
        Update_CombinedWorldMatrix(_offsetMatrix * _transformCom->Get_WorldMatrix() * (*_socketMatrix));
    else
        Update_CombinedWorldMatrix(_transformCom->Get_WorldMatrix());

    if (_model)
    {
        if (_hairPhysicsReady && _hairPhysicsModifier)
        {
            _hairPhysicsModifier->Evaluate_Modifier(_model.get(), timeDelta);
        }
    }

    if (_partEffectCom)
        _partEffectCom->Update(timeDelta);
}

void Mon_Simon_Hair::Late_Update(float timeDelta)
{
    Monster_Part::Late_Update(timeDelta);
}

void Mon_Simon_Hair::Post_PhysX_Update(float timeDelta)
{
    Monster_Part::Post_PhysX_Update(timeDelta);
}

HRESULT Mon_Simon_Hair::Render()
{
    return Monster_Part::Render();
}

HRESULT Mon_Simon_Hair::Render_Shadow()
{
    return Monster_Part::Render_Shadow();
}

Matrix Mon_Simon_Hair::Get_GizmoWorldMatrix() const
{
    if (_transformCom == nullptr)
        return _combinedWorldMatrix;

    const Matrix parentMatrix = (nullptr != _parentMatrix) ? *_parentMatrix : Matrix::Identity;
    const Matrix socketMatrix = (nullptr != _socketMatrix) ? *_socketMatrix : Matrix::Identity;

    return _offsetMatrix * _transformCom->Get_WorldMatrix() * socketMatrix * parentMatrix;
}

bool Mon_Simon_Hair::Apply_GizmoWorldMatrix(const Matrix& gizmoWorldMatrix)
{
    if (_transformCom == nullptr)
        return false;

    const Matrix parentMatrix = (nullptr != _parentMatrix) ? *_parentMatrix : Matrix::Identity;
    const Matrix socketMatrix = (nullptr != _socketMatrix) ? *_socketMatrix : Matrix::Identity;
    Matrix localWorldMatrix = _offsetMatrix.Invert() * gizmoWorldMatrix * (socketMatrix * parentMatrix).Invert();

    Vec3 scale = Vec3::One;
    Quat rotation = Quat::Identity;
    Vec3 translation = Vec3::Zero;

    if (!localWorldMatrix.Decompose(scale, rotation, translation))
        return false;

    _transformCom->Set_WorldPosition(translation);
    _transformCom->Set_RotationQuaternion(rotation);
    _transformCom->Set_Scale(scale);

    Update_CombinedWorldMatrix(_offsetMatrix * _transformCom->Get_WorldMatrix() * socketMatrix);
    return true;
}

void Mon_Simon_Hair::Set_RuntimeCollisionSource(ModelCom* collisionModel, const Matrix* collisionOwnerWorldMatrix)
{
    _collisionModel = collisionModel;
    _collisionOwnerWorldMatrix = collisionOwnerWorldMatrix;
}

const Engine::ClassReflectionInfo* Mon_Simon_Hair::Get_RuntimeReflectionInfo() const
{
    return _hairPhysicsModifier ? _hairPhysicsModifier->Get_RuntimeReflectionInfo() : nullptr;
}

void* Mon_Simon_Hair::Get_RuntimeReflectionTarget()
{
    return _hairPhysicsModifier ? _hairPhysicsModifier->Get_RuntimeReflectionTarget() : nullptr;
}

void Mon_Simon_Hair::On_RuntimeReflectionChanged()
{
    if (!_hairPhysicsReady || !_model || !_hairPhysicsModifier)
        return;

    _hairPhysicsModifier->RuntimeRebuild(_model.get());
}

void Mon_Simon_Hair::RuntimeResetPose()
{
    if (!_hairPhysicsReady || !_model || !_hairPhysicsModifier)
        return;

    _hairPhysicsModifier->RuntimeResetPose(_model.get());
}

void Mon_Simon_Hair::RuntimeRebuild()
{
    if (!_model || !_hairPhysicsModifier)
        return;

    _hairPhysicsModifier->RuntimeRebuild(_model.get());
    _hairPhysicsReady = true;
}

void Mon_Simon_Hair::Build_RuntimeDebugCapsules(vector<PhysicsModifier::RuntimeDebugCapsule>& outCapsules) const
{
    outCapsules.clear();
    if (!_hairPhysicsReady || !_hairPhysicsModifier)
        return;

    _hairPhysicsModifier->Build_RuntimeDebugCapsules(outCapsules);
}

void Mon_Simon_Hair::Build_RuntimeDebugSpheres(vector<PhysicsModifier::RuntimeDebugSphere>& outSpheres) const
{
    outSpheres.clear();
    if (!_hairPhysicsReady || !_hairPhysicsModifier)
        return;

    _hairPhysicsModifier->Build_RuntimeDebugSpheres(outSpheres);
}

void Mon_Simon_Hair::Build_RuntimeDebugSegments(vector<PhysicsModifier::RuntimeDebugSegment>& outSegments) const
{
    outSegments.clear();
    if (!_hairPhysicsReady || !_hairPhysicsModifier)
        return;

    _hairPhysicsModifier->Build_RuntimeDebugSegments(outSegments);
}

uint32 Mon_Simon_Hair::Decide_PassIndex(Shared<Material> mat)
{
    return mat->Get_PassIndex();
}

Shared<Mon_Simon_Hair> Mon_Simon_Hair::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Mon_Simon_Hair>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> Mon_Simon_Hair::Clone(void* arg)
{
    auto clone = make_shared<Mon_Simon_Hair>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void Mon_Simon_Hair::Free()
{
    Monster_Part::Free();
}

NS_END

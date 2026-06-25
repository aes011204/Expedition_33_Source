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
        if (_useHairAnimationPlayback)
        {
            _model->Set_Animation("Simon_Hair", true);
        }
    }

    CHECK_FAILED(Ready_Runtime(), E_FAIL);

    return S_OK;
}

HRESULT Mon_Simon_Hair::Ready_Runtime()
{
    SimonHairRuntime::Desc hairDesc{};
    ChainModifierPreset::BuildSimonHairHardcodedDesc(
        hairDesc,
        _transformCom.get(),
        PlayerCharacterType::None);

    if (_model && !hairDesc.chains.empty())
    {
        _hairRuntime.Initialize(_model.get(), hairDesc);
        _hairRuntimeReady = _hairRuntime.Is_Built();
    }

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
        if (_useHairAnimationPlayback)
        {
            _model->Play_Animation(timeDelta);
        }

        if (_hairRuntimeReady)
        {
            const Matrix motionDriverWorld = Get_CombinedWorldMatrix();
            const Matrix collisionOwnerWorld =
                _collisionOwnerWorldMatrix ? *_collisionOwnerWorldMatrix : motionDriverWorld;
            _hairRuntime.Prepare_Simulation(
                _model.get(),
                _collisionModel,
                timeDelta,
                motionDriverWorld,
                collisionOwnerWorld,
                motionDriverWorld);
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

    if (!_model || !_hairRuntimeReady)
        return;

    const Matrix motionDriverWorld = Get_CombinedWorldMatrix();
    const Matrix collisionOwnerWorld =
        _collisionOwnerWorldMatrix ? *_collisionOwnerWorldMatrix : motionDriverWorld;
    _hairRuntime.Finalize_Simulation(
        _model.get(),
        _collisionModel,
        timeDelta,
        motionDriverWorld,
        collisionOwnerWorld,
        motionDriverWorld);
}

HRESULT Mon_Simon_Hair::Render()
{
    return Monster_Part::Render();
}

HRESULT Mon_Simon_Hair::Render_Shadow()
{
    return Monster_Part::Render_Shadow();
}

void Mon_Simon_Hair::Set_RuntimeCollisionSource(ModelCom* collisionModel, const Matrix* collisionOwnerWorldMatrix)
{
    _collisionModel = collisionModel;
    _collisionOwnerWorldMatrix = collisionOwnerWorldMatrix;
}

const Engine::ClassReflectionInfo* Mon_Simon_Hair::Get_RuntimeReflectionInfo() const
{
    return _hairRuntime.Get_RuntimeReflectionInfo();
}

void* Mon_Simon_Hair::Get_RuntimeReflectionTarget()
{
    return _hairRuntime.Get_RuntimeReflectionTarget();
}

void Mon_Simon_Hair::On_RuntimeReflectionChanged()
{
    if (!_hairRuntimeReady || !_model)
        return;

    const Matrix motionDriverWorld = Get_CombinedWorldMatrix();
    const Matrix collisionOwnerWorld =
        _collisionOwnerWorldMatrix ? *_collisionOwnerWorldMatrix : motionDriverWorld;
    _hairRuntime.On_RuntimeReflectionChanged(
        _model.get(),
        _collisionModel,
        motionDriverWorld,
        collisionOwnerWorld,
        motionDriverWorld);
}

void Mon_Simon_Hair::RuntimeResetPose()
{
    if (!_hairRuntimeReady || !_model)
        return;

    const Matrix motionDriverWorld = Get_CombinedWorldMatrix();
    const Matrix collisionOwnerWorld =
        _collisionOwnerWorldMatrix ? *_collisionOwnerWorldMatrix : motionDriverWorld;
    _hairRuntime.ResetPose(_model.get(), _collisionModel, motionDriverWorld, collisionOwnerWorld, motionDriverWorld);
}

void Mon_Simon_Hair::RuntimeRebuild()
{
    if (!_model)
        return;

    const Matrix motionDriverWorld = Get_CombinedWorldMatrix();
    const Matrix collisionOwnerWorld =
        _collisionOwnerWorldMatrix ? *_collisionOwnerWorldMatrix : motionDriverWorld;
    _hairRuntime.RuntimeRebuild(
        _model.get(),
        _collisionModel,
        motionDriverWorld,
        collisionOwnerWorld,
        motionDriverWorld);
    _hairRuntimeReady = _hairRuntime.Is_Built();
}

void Mon_Simon_Hair::Build_RuntimeDebugCapsules(vector<SimonHairRuntime::RuntimeDebugCapsule>& outCapsules) const
{
    outCapsules.clear();
    if (!_hairRuntimeReady)
        return;

    _hairRuntime.Build_RuntimeDebugCapsules(outCapsules);
}

void Mon_Simon_Hair::Build_RuntimeDebugSpheres(vector<SimonHairRuntime::RuntimeDebugSphere>& outSpheres) const
{
    outSpheres.clear();
    if (!_hairRuntimeReady)
        return;

    _hairRuntime.Build_RuntimeDebugSpheres(outSpheres);
}

void Mon_Simon_Hair::Build_RuntimeDebugSegments(vector<SimonHairRuntime::RuntimeDebugSegment>& outSegments) const
{
    outSegments.clear();
    if (!_hairRuntimeReady)
        return;

    _hairRuntime.Build_RuntimeDebugSegments(outSegments);
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

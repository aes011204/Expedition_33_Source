#include "Mon_Dualliste_Body.h"

#include "ChainModifierPreset.h"
#include "ModelCom.h"
#include "PhysicsModifier.h"

IMPLEMENT_REFLECTION(Mon_Dualliste_Body)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    
    return true;
}

Mon_Dualliste_Body::Mon_Dualliste_Body(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_Part(device, context)
{
}

Mon_Dualliste_Body::Mon_Dualliste_Body(const Mon_Dualliste_Body& prototype)
    : Monster_Part(prototype)
{
}

HRESULT Mon_Dualliste_Body::Initialize_Prototype()
{
    return Monster_Part::Initialize_Prototype();
}

HRESULT Mon_Dualliste_Body::Initialize(void* arg)
{
     CHECK_FAILED(Monster_Part::Initialize(arg), E_FAIL);

    _rootIndex = _model ? _model->Get_BoneIndex("root") : -1;
     if (_model && _rootIndex >= 0)
     {
         _model->Set_LocalRootNode(_rootIndex);
     }

     CHECK_FAILED(Ready_Modifier(), E_FAIL);

     _model->Set_RootMotionEnabled(true);
    return S_OK;
}

HRESULT Mon_Dualliste_Body::Ready_Modifier()
{
    if (_model == nullptr)
        return E_FAIL;

    PhysicsModifier::PhysicsModifierDesc hairbandDesc{};
    ChainModifierPreset::BuildDuallisteHairbandPhysicsDesc(
        hairbandDesc,
        _transformCom.get());

    if (!hairbandDesc.chains.empty())
    {
        if (auto modifier = PhysicsModifier::Create(&hairbandDesc))
            _model->Add_AnimModifier(modifier);
    }

    return S_OK;
}

void Mon_Dualliste_Body::Priority_Update(float timeDelta)
{
    Monster_Part::Priority_Update(timeDelta);
}

void Mon_Dualliste_Body::Update(float timeDelta)
{
    Monster_Part::Update(timeDelta);
}

void Mon_Dualliste_Body::Late_Update(float timeDelta)
{
    Monster_Part::Late_Update(timeDelta);
}

HRESULT Mon_Dualliste_Body::Render()
{
    return Monster_Part::Render();
}

HRESULT Mon_Dualliste_Body::Render_Shadow()
{
    return Monster_Part::Render_Shadow();
}

Shared<Mon_Dualliste_Body> Mon_Dualliste_Body::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Mon_Dualliste_Body>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : Mon_Dualliste_Body");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> Mon_Dualliste_Body::Clone(void* arg)
{
    auto clone = make_shared<Mon_Dualliste_Body>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : Mon_Dualliste_Body");
        return nullptr;
    }

    return clone;
}

void Mon_Dualliste_Body::Free()
{
    Monster_Part::Free();
}

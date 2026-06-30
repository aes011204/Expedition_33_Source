#include "Mon_Abbest_Body.h"

#include "ModelCom.h"
#include "ChainModifierPreset.h"
#include "ChainModifier.h"

NS_BEGIN(Client)

Mon_Abbest_Body::Mon_Abbest_Body(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_Part(device, context)
{
}

Mon_Abbest_Body::Mon_Abbest_Body(const Mon_Abbest_Body& prototype)
    : Monster_Part(prototype)
{
}

HRESULT Mon_Abbest_Body::Initialize_Prototype()
{
    return Monster_Part::Initialize_Prototype();
}

HRESULT Mon_Abbest_Body::Initialize(void* arg)
{
    if (FAILED(Monster_Part::Initialize(arg)))
        return E_FAIL;

    _rootIndex = _model->Get_BoneIndex("root");
    _model->Set_LocalRootNode(_rootIndex);

    //_model->Set_RootMotionEnabled(false);

    ChainModifier::ChainModifierDesc clothDesc{};
    ChainModifierPreset::BuildAbbestClothDesc(
        clothDesc,
        _transformCom.get(),
        PlayerCharacterType::None);

    if (auto modifier = ChainModifier::Create(&clothDesc))
    {
        _model->Add_AnimModifier(modifier);
    }

    return S_OK;
}

void Mon_Abbest_Body::Priority_Update(float timeDelta)
{
    Monster_Part::Priority_Update(timeDelta);
}

void Mon_Abbest_Body::Update(float timeDelta)
{
    Monster_Part::Update(timeDelta);
}

void Mon_Abbest_Body::Late_Update(float timeDelta)
{
    Monster_Part::Late_Update(timeDelta);
}

HRESULT Mon_Abbest_Body::Render()
{
    return Monster_Part::Render();
}

HRESULT Mon_Abbest_Body::Render_Shadow()
{
    return Monster_Part::Render_Shadow();
}

Shared<Mon_Abbest_Body> Mon_Abbest_Body::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Mon_Abbest_Body>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> Mon_Abbest_Body::Clone(void* arg)
{
    auto clone = make_shared<Mon_Abbest_Body>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void Mon_Abbest_Body::Free()
{
    Monster_Part::Free();
}

NS_END

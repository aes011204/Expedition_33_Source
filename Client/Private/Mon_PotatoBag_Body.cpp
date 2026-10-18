#include "Mon_PotatoBag_Body.h"

#include "ModelCom.h"
#include "ChainModifierPreset.h"
#include "ChainModifier.h"

NS_BEGIN(Client)

Mon_PotatoBag_Body::Mon_PotatoBag_Body(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_Part(device, context)
{
}

Mon_PotatoBag_Body::Mon_PotatoBag_Body(const Mon_PotatoBag_Body& prototype)
    : Monster_Part(prototype)
{
}

HRESULT Mon_PotatoBag_Body::Initialize_Prototype()
{
    return Monster_Part::Initialize_Prototype();
}

HRESULT Mon_PotatoBag_Body::Initialize(void* arg)
{
    if (FAILED(Monster_Part::Initialize(arg)))
        return E_FAIL;

    _rootIndex = _model->Get_BoneIndex("root");
    _model->Set_LocalRootNode(_rootIndex);

    CHECK_FAILED(Ready_Modifier(), E_FAIL);

    return S_OK;
}

HRESULT Mon_PotatoBag_Body::Ready_Modifier()
{
    ChainModifier::ChainModifierDesc hairDesc{};
    ChainModifierPreset::BuildPotatoBagHairDesc(
        hairDesc,
        _transformCom.get(),
        PlayerCharacterType::None);

    if (!hairDesc.groups.empty())
    {
        if (auto modifier = ChainModifier::Create(&hairDesc))
        {
            _model->Add_AnimModifier(modifier);
        }
    }

    ChainModifier::ChainModifierDesc clothBackDesc{};
    ChainModifierPreset::BuildPotatoBagClothBackDesc(
        clothBackDesc,
        _transformCom.get(),
        PlayerCharacterType::None);

    if (!clothBackDesc.groups.empty())
    {
        if (auto modifier = ChainModifier::Create(&clothBackDesc))
        {
            _model->Add_AnimModifier(modifier);
        }
    }

    return S_OK;
}

void Mon_PotatoBag_Body::Priority_Update(float timeDelta)
{
    Monster_Part::Priority_Update(timeDelta);
}

void Mon_PotatoBag_Body::Update(float timeDelta)
{
    Monster_Part::Update(timeDelta);
}

void Mon_PotatoBag_Body::Late_Update(float timeDelta)
{
    Monster_Part::Late_Update(timeDelta);
}

HRESULT Mon_PotatoBag_Body::Render()
{
    return Monster_Part::Render();
}

HRESULT Mon_PotatoBag_Body::Render_Shadow()
{
    return Monster_Part::Render_Shadow();
}

Shared<Mon_PotatoBag_Body> Mon_PotatoBag_Body::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Mon_PotatoBag_Body>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> Mon_PotatoBag_Body::Clone(void* arg)
{
    auto clone = make_shared<Mon_PotatoBag_Body>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void Mon_PotatoBag_Body::Free()
{
    Monster_Part::Free();
}

NS_END

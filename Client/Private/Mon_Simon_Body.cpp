#include "Mon_Simon_Body.h"

#include "ModelCom.h"
#include "ChainModifierPreset.h"
#include "ChainModifier.h"
#include "OffsetAnimationModifier.h"
#include "PhysicsModifier.h"

NS_BEGIN(Client)

Mon_Simon_Body::Mon_Simon_Body(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_Part(device, context)
{
}

Mon_Simon_Body::Mon_Simon_Body(const Mon_Simon_Body& prototype)
    : Monster_Part(prototype)
{
}

HRESULT Mon_Simon_Body::Initialize_Prototype()
{
    return Monster_Part::Initialize_Prototype();
}

HRESULT Mon_Simon_Body::Initialize(void* arg)
{
    if (FAILED(Monster_Part::Initialize(arg)))
        return E_FAIL;

    _rootIndex = _model->Get_BoneIndex("root");
    _model->Set_LocalRootNode(_rootIndex);

    CHECK_FAILED(Ready_Modifier(), E_FAIL);

    return S_OK;
}

HRESULT Mon_Simon_Body::Ready_Modifier()
{
    OffsetAnimationModifier::OffsetModifierDesc oneShotOffsetDesc{};
    oneShotOffsetDesc.modifierTag = DefaultOneShotOffsetModifierTag;
    oneShotOffsetDesc.baseModelCom = _model.get();
    oneShotOffsetDesc.animName = "Simon_Hit";
    oneShotOffsetDesc.blendMode = AnimationModifier::ModifierBlendMode::ModifyAddictive;
    oneShotOffsetDesc.blendInTime = 0.02f;
    oneShotOffsetDesc.blendOutTime = 0.08f;
    oneShotOffsetDesc.timeRatio_Center = 0.f;
    oneShotOffsetDesc.offsetBoneRanges = {
        { "spine_02", "clavicle_l" },
        { "spine_02", "clavicle_r" }
    };

    if (auto modifier = OffsetAnimationModifier::Create(&oneShotOffsetDesc))
    {
        modifier->Set_Active(false);
        _model->Add_AnimModifier(modifier);
    }

    ChainModifier::ChainModifierDesc armbandDesc{};
    ChainModifierPreset::BuildArmbandDesc(
        armbandDesc,
        _transformCom.get(),
        PlayerCharacterType::Gustave);

    if (!armbandDesc.groups.empty())
    {
        if (auto modifier = ChainModifier::Create(&armbandDesc))
        {
            _model->Add_AnimModifier(modifier);
        }
    }

    PhysicsModifier::PhysicsModifierDesc coatDesc{};
    ChainModifierPreset::BuildSimonCoatPhysicsDesc(
        coatDesc,
        _transformCom.get());

    if (!coatDesc.chains.empty())
    {
        if (auto modifier = PhysicsModifier::Create(&coatDesc))
        {
            _model->Add_AnimModifier(modifier);
        }
    }

    return S_OK;
}

bool Mon_Simon_Body::Play_OffsetModifierOnce(
    const wstring& modifierTag,
    float strengthAlpha,
    float durationSec,
    float startRatio,
    float playRate)
{
    if (_model == nullptr)
        return false;

    auto modifier = dynamic_cast<OffsetAnimationModifier*>(
        _model->Find_AnimModifierByTag(modifierTag));

    if (modifier == nullptr)
        return false;

    OffsetAnimationModifier::OneShotPlaybackDesc desc{};
    desc.strengthAlpha = strengthAlpha;
    desc.durationSec = durationSec;
    desc.startRatio = startRatio;
    desc.playRate = playRate;
    modifier->Play_Once(desc);

    return true;
}

bool Mon_Simon_Body::Play_DefaultOffsetOnce(
    float strengthAlpha,
    float durationSec,
    float startRatio,
    float playRate)
{
    return Play_OffsetModifierOnce(
        DefaultOneShotOffsetModifierTag,
        strengthAlpha,
        durationSec,
        startRatio,
        playRate);
}

void Mon_Simon_Body::Priority_Update(float timeDelta)
{
    Monster_Part::Priority_Update(timeDelta);
}

void Mon_Simon_Body::Update(float timeDelta)
{
    Monster_Part::Update(timeDelta);
}

void Mon_Simon_Body::Late_Update(float timeDelta)
{
    Monster_Part::Late_Update(timeDelta);
}

HRESULT Mon_Simon_Body::Render()
{
    return Monster_Part::Render();
}

HRESULT Mon_Simon_Body::Render_Shadow()
{
    return Monster_Part::Render_Shadow();
}

Shared<Mon_Simon_Body> Mon_Simon_Body::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Mon_Simon_Body>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> Mon_Simon_Body::Clone(void* arg)
{
    auto clone = make_shared<Mon_Simon_Body>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void Mon_Simon_Body::Free()
{
    Monster_Part::Free();
}

NS_END

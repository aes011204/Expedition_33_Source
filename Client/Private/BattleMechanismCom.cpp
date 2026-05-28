#include "BattleMechanismCom.h"

IMPLEMENT_REFLECTION(BattleMechanismCom)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "BattleMechanism";

    return true;
}

BattleMechanismCom::BattleMechanismCom(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Component(device, context)
{
}

BattleMechanismCom::BattleMechanismCom(const BattleMechanismCom& Prototype)
    : Component(Prototype),
    _heroId(Prototype._heroId),
    _gustave(Prototype._gustave),
    _lune(Prototype._lune),
    _maelle(Prototype._maelle)
{
}

HRESULT BattleMechanismCom::Initialize_Prototype()
{
    return Component::Initialize_Prototype();

}

HRESULT BattleMechanismCom::Initialize(void* arg)
{
    return Component::Initialize(arg);

}

void BattleMechanismCom::BeginPlay()
{
    Component::BeginPlay();

}

void BattleMechanismCom::Update(float timeDelta)
{

}

json BattleMechanismCom::To_Json() const
{
    return Component::To_Json();
}

void BattleMechanismCom::From_Json(const json& data)
{
    Component::From_Json(data);
}

void BattleMechanismCom::Reset_ForHero(HeroId heroId)
{
    _heroId = heroId;
    _gustave = {};
    _lune = {};
    _maelle = {};

    if (_heroId == HeroId::Gustave)
    {
        _gustave.currentCharge = 0;
        _gustave.maxCharge = 10;
    }
    else if (_heroId == HeroId::Maelle)
    {
        _maelle.currentStance = MaelleStance::Stanceless;
    }
}

void BattleMechanismCom::Add_GustaveCharge(int32 amount)
{
    if (!Is_Gustave() || amount <= 0)
        return;

    _gustave.currentCharge = min(_gustave.currentCharge + amount, _gustave.maxCharge);
}

int32 BattleMechanismCom::Consume_AllGustaveCharge()
{
    if (!Is_Gustave())
        return 0;

    const int32 consumed = _gustave.currentCharge;
    _gustave.currentCharge = 0;
    return consumed;
}

void BattleMechanismCom::Clear_GustaveCharge()
{
    if (!Is_Gustave())
        return;

    _gustave.currentCharge = 0;
}

void BattleMechanismCom::Set_MaelleStance(MaelleStance stance)
{
    if (!Is_Maelle())
        return;

    _maelle.currentStance = stance;
}

void BattleMechanismCom::Reset_MaelleStance()
{
    if (!Is_Maelle())
        return;

    _maelle.currentStance = MaelleStance::Stanceless;
}

void BattleMechanismCom::Apply_MaelleSkillTransition(const string& skillKey)
{
    if (!Is_Maelle())
        return;

    // TODO : 일단은 하드코딩으로 각 스킬을 받아서 태세 변경을 진행하는데,
    // TODO : 추후에 스킬 데이터 자체에 어떤 태세로 변경될지까지 넣어줘야한다.
    if (skillKey == "OffensiveSwitch")
    {
        _maelle.currentStance = MaelleStance::Offensive;
    }
    else if (skillKey == "FireRise")
    {
        _maelle.currentStance = MaelleStance::Virtuose;
    }
    else if (skillKey == "Percee")
    {
        _maelle.currentStance = MaelleStance::Defensive;
    }
}

Shared<BattleMechanismCom> BattleMechanismCom::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<BattleMechanismCom>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : BattleMechanismCom");
        return nullptr;
    }

    return instance;
}

Shared<Component> BattleMechanismCom::Clone(void* arg)
{
    auto clone = make_shared<BattleMechanismCom>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : BattleMechanismCom");
        return nullptr;
    }

    return clone;
}

void BattleMechanismCom::Free()
{
    Component::Free();
}

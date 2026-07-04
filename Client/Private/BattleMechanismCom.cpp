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

void BattleMechanismCom::Apply_WeaponMilestoneBattleStart(const string& characterId, const string& weaponId,
    uint32 enhanceLevel)
{
    if (weaponId.empty() || enhanceLevel < 8)
        return;

    if (characterId == "Gustave")
    {
        if (!Is_Gustave())
            return;

        if (weaponId == "Lanceram")
            Add_GustaveCharge(3);
        else if (weaponId == "Baguette")
            Add_GustaveCharge(5);
        else if (weaponId == "Tireso")
            Add_GustaveCharge(Get_GustaveMaxCharge());

        return;
    }

    if (characterId == "Lune")
    {
        if (!Is_Lune())
            return;

        if (weaponId == "Colim")
            Add_LuneStains({ ElementalType::Fire });
        else if (weaponId == "Baguette")
            Add_LuneStains({ ElementalType::Ice, ElementalType::Fire });
        else if (weaponId == "Snowim")
            Add_LuneStains({ ElementalType::Earth, ElementalType::Lightning, ElementalType::Fire });

        return;
    }

    if (characterId == "Maelle")
    {
        if (!Is_Maelle())
            return;

        if (weaponId == "Baguette")
            Set_MaelleStance(MaelleStance::Offensive);
        else if (weaponId == "Medalum")
            Set_MaelleStance(MaelleStance::Defensive);
        else if (weaponId == "Plenum")
            Set_MaelleStance(MaelleStance::Virtuose);
    }
}

bool BattleMechanismCom::Can_UseGustaveOvercharge() const
{
    return Is_Gustave() && _gustave.currentCharge >= _gustave.maxCharge; // 과충전 사용 가능
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

ElementalType BattleMechanismCom::Get_LuneStain(uint32 index) const
{
    if (!Is_Lune() || index >= _lune.stainCount)
        return ElementalType::None;

    return _lune.stains[index];
}

void BattleMechanismCom::Add_LuneStains(initializer_list<ElementalType> elements)
{
    if (!Is_Lune())
        return;

    bool changed = false;

    for (ElementalType element : elements)
    {
        if (!Is_LuneStainElement(element))
            continue;

        if (_lune.stainCount < kLuneMaxStains)
        {
            _lune.stains[_lune.stainCount++] = element;
        }
        else
        {
            for (uint32 i = 1; i < kLuneMaxStains; ++i)
                _lune.stains[i - 1] = _lune.stains[i];

            _lune.stains[kLuneMaxStains - 1] = element;
        }

        changed = true;
    }

    if (changed)
        ++_lune.visualRevision; 
}

void BattleMechanismCom::Clear_LuneStains()
{
    if (!Is_Lune())
        return;

    _lune = {};
}

bool BattleMechanismCom::Consume_LuneStain(ElementalType element)
{
    if (!Is_Lune() || !Is_LuneStainElement(element))
        return false;

    for (uint32 i = 0; i < _lune.stainCount; ++i)
    {
        if (_lune.stains[i] != element)
            continue;

        for (uint32 j = i + 1; j < _lune.stainCount; ++j)
            _lune.stains[j - 1] = _lune.stains[j];

        --_lune.stainCount;
        _lune.stains[_lune.stainCount] = ElementalType::None;
        ++_lune.visualRevision;
        return true;
    }

    return false;
}

bool BattleMechanismCom::Is_LuneStainElement(ElementalType element)
{
    switch (element)
    {
    case ElementalType::Fire:
    case ElementalType::Ice:
    case ElementalType::Lightning:
    case ElementalType::Earth:
    case ElementalType::Light:
        return true;
    default:
        return false;
    }
}

float BattleMechanismCom::Get_MaelleOutgoingDamageMultiplier() const
{
    if (!Is_Maelle())
        return 1.f;

    switch (_maelle.currentStance)
    {
    case MaelleStance::Offensive:
        return 1.5f; // 공격 태세: 주는 피해 +50%

    case MaelleStance::Virtuose:
        return 2.f; // 명인 태세: 주는 피해 200%

    case MaelleStance::Defensive:
    case MaelleStance::Stanceless:
    default:
        return 1.f;
    }
}

float BattleMechanismCom::Get_MaelleIncomingDamageMultiplier() const
{
    if (!Is_Maelle())
        return 1.f;

    switch (_maelle.currentStance)
    {
    case MaelleStance::Offensive:
        return 1.5f; // 공격 태세: 받는 피해 +50%

    case MaelleStance::Defensive:
        return 0.5f; // 방어 태세: 받는 피해 -50%

    case MaelleStance::Virtuose:
    case MaelleStance::Stanceless:
    default:
        return 1.f;
    }
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

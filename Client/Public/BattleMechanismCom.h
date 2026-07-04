#pragma once

#include "Battle_Enum.h"
#include "Component.h"

NS_BEGIN(Client)

// 플레이어별 스킬 메커니즘
class BattleMechanismCom : public Component
{
    GENERATED_COMPONENT(BattleMechanismCom)

public:
    static constexpr uint32 kLuneMaxStains = 4;

    struct GustaveMechanism
    {
        int32 currentCharge = 0;
        int32 maxCharge = 10;
    };

    struct LuneMechanism
    {
        array<ElementalType, kLuneMaxStains> stains
        {
            ElementalType::None, ElementalType::None, ElementalType::None, ElementalType::None 
        };

        uint32 stainCount = 0;
        uint32 visualRevision = 0;
    };

    enum class MaelleStance
    {
        Stanceless, // 태세 없음
        Offensive,  // 공격 태세
        Defensive,  // 방어 태세
        Virtuose,   // 명인 태세
        END
    };

    struct MaelleMechanism
    {
        MaelleStance currentStance = MaelleStance::Stanceless;
    };

public:
    BattleMechanismCom(const ComPtr<Device>& device, const ComPtr<Context>& context);
    BattleMechanismCom(const BattleMechanismCom& Prototype);
    virtual ~BattleMechanismCom() = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    BeginPlay() override;
    void    Update(float timeDelta);

    json    To_Json() const override;
    void    From_Json(const json& data) override;

public:
    void    Reset_ForHero(HeroId heroId);
    void    Apply_WeaponMilestoneBattleStart(
                const string& characterId,
                const string& weaponId,
                uint32 enhanceLevel);


    HeroId  Get_HeroId() const { return _heroId; }

    bool    Is_Gustave() const { return _heroId == HeroId::Gustave; }
    bool    Is_Lune() const { return _heroId == HeroId::Lune; }
    bool    Is_Maelle() const { return _heroId == HeroId::Maelle; }

public: /* 구스타브 */
    int32   Get_GustaveCharge() const { return _gustave.currentCharge; }
    int32   Get_GustaveMaxCharge() const { return _gustave.maxCharge; }

    bool    Can_UseGustaveOvercharge() const;

    void    Add_GustaveCharge(int32 amount = 1);
    int32   Consume_AllGustaveCharge(); // 과충전 사용
    void    Clear_GustaveCharge();

public: /* 루네 */
    uint32        Get_LuneStainCount() const { return _lune.stainCount; }
    ElementalType Get_LuneStain(uint32 index) const;
    uint32        Get_LuneVisualRevision() const { return _lune.visualRevision; }

    void          Add_LuneStains(initializer_list<ElementalType> elements);
    void          Clear_LuneStains();
    bool          Consume_LuneStain(ElementalType element);

    bool          Is_LuneStainElement(ElementalType element);

public: /* 마엘 */
    MaelleStance Get_MaelleStance() const { return _maelle.currentStance; }

    // 받는 피해, 주는 피해
    float   Get_MaelleOutgoingDamageMultiplier() const;
    float   Get_MaelleIncomingDamageMultiplier() const;

    void    Set_MaelleStance(MaelleStance stance);
    void    Reset_MaelleStance(); // 전투 시작 시 태세 없음으로 초기화


private:
    GustaveMechanism _gustave{};
    LuneMechanism    _lune{};
    MaelleMechanism  _maelle{};

    HeroId           _heroId = HeroId::END;

public:
    static Shared<BattleMechanismCom> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<Component> Clone(void* arg) override;
    void Free() override;
    
};

NS_END

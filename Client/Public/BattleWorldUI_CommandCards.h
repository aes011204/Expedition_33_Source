#pragma once

#include "Battle_Enum.h"
#include "GameObject.h"
#include "GameplayUI_Types.h"

NS_BEGIN(Client)

class BattleWorldUI_CommandCards final : public GameObject, public IRuntimeWorldUI_EditorTarget
{
public:
    BattleWorldUI_CommandCards(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ~BattleWorldUI_CommandCards() override;

public:
    HRESULT Initialize(void* arg) override;
    void    Show_ForBattleState(
        const Vec3& anchorWorldPos,
        BattleTurnState currentState,
        uint32 curAp,
        uint32 maxAp,
        HeroId actorHeroId,
        float timeDelta);
    HRESULT Warmup_ForBattleState(uint32 curAp, uint32 maxAp, HeroId actorHeroId);
    void    Hide();

    void    Late_Update(float timeDelta) override;
    HRESULT Render() override;
    Shared<GameObject> Clone(void* arg) override;
    void    Free() override;

public:
    HeroId Get_EditorWorldPlacementHeroId() const override;
    void Set_EditorWorldPlacementHeroId(HeroId heroId) override;
    RuntimeWorldUIPlacementTuning Get_EditorWorldPlacementTuning() const override;
    void Set_EditorWorldPlacementTuning(
        const RuntimeWorldUIPlacementTuning& tuning,
        bool saveImmediately) override;
    void Reload_EditorWorldPlacementTuning() override;
    void Reset_EditorWorldPlacementTuning() override;

public:
    static Shared<BattleWorldUI_CommandCards> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);

private:
    struct Impl;
    Unique<Impl> _impl{};
};

NS_END

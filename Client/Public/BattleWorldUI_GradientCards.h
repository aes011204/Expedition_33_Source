#pragma once

#include "Battle_Enum.h"
#include "GameObject.h"
#include "GameplayUI_Types.h"

NS_BEGIN(Client)

class BattleWorldUI_GradientCards final : public GameObject, public IRuntimeWorldUI_EditorTarget
{
public:
    BattleWorldUI_GradientCards(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ~BattleWorldUI_GradientCards() override;

public:
    HRESULT Initialize(void* arg) override;
    void    Show_ForBattleState(
        const Vec3& anchorWorldPos,
        BattleTurnState currentState,
        HeroId actorHeroId,
        float timeDelta);
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
    static Shared<BattleWorldUI_GradientCards> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);

private:
    struct Impl;
    Unique<Impl> _impl{};
};

NS_END

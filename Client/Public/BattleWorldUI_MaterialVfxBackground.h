#pragma once

#include "Battle_Enum.h"
#include "GameObject.h"
#include "GameplayUI_Types.h"

NS_BEGIN(Client)

class BattleWorldUI_MaterialVfxBackground final : public GameObject, public IRuntimeWorldUI_EditorTarget
{
public:
    BattleWorldUI_MaterialVfxBackground(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ~BattleWorldUI_MaterialVfxBackground() override;

public:
    HRESULT Initialize(void* arg) override;
    HRESULT Warmup_ForBattleState(HeroId actorHeroId);
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
    LevelType Get_EditorWorldPlacementLevelType() const override;
    void Set_EditorWorldPlacementLevelType(LevelType levelType) override;
    HeroId Get_EditorWorldPlacementHeroId() const override;
    void Set_EditorWorldPlacementHeroId(HeroId heroId) override;
    uint32 Get_EditorWorldPlacementVariantCount() const override;
    uint32 Get_EditorWorldPlacementVariantIndex() const override;
    const char* Get_EditorWorldPlacementVariantLabel(uint32 index) const override;
    void Set_EditorWorldPlacementVariantIndex(uint32 index) override;
    RuntimeWorldUIPlacementTuning Get_EditorWorldPlacementTuning() const override;
    void Set_EditorWorldPlacementTuning(
        const RuntimeWorldUIPlacementTuning& tuning,
        bool saveImmediately) override;
    void Reload_EditorWorldPlacementTuning() override;
    void Reset_EditorWorldPlacementTuning() override;

public:
    static Shared<BattleWorldUI_MaterialVfxBackground> Create(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context);

private:
    struct Impl;
    Unique<Impl> _impl{};
};

NS_END

#pragma once

#include "Battle_Enum.h"
#include "GameObject.h"

NS_BEGIN(Client)

class BattleWorldUI_SkillCards final : public GameObject
{
public:
    BattleWorldUI_SkillCards(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ~BattleWorldUI_SkillCards() override;

public:
    HRESULT Initialize(void* arg) override;
    void    Show_ForBattleState(
        const Vec3& anchorWorldPos,
        BattleTurnState currentState,
        const BattleChar& actor,
        const string& selectedSkillKey);
    void    Hide();

    void    Late_Update(float timeDelta) override;
    HRESULT Render() override;
    Shared<GameObject> Clone(void* arg) override;
    void    Free() override;

public:
    static Shared<BattleWorldUI_SkillCards> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);

private:
    struct Impl;
    Unique<Impl> _impl{};
};

NS_END

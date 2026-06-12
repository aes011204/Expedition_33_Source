#pragma once

#include "Battle_Enum.h"
#include "GameObject.h"

NS_BEGIN(Client)

class BattleWorldUI_CommandCards final : public GameObject
{
public:
    BattleWorldUI_CommandCards(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ~BattleWorldUI_CommandCards() override;

public:
    HRESULT Initialize(void* arg) override;
    void    Show_ForBattleState(const Vec3& anchorWorldPos, BattleTurnState currentState);
    void    Hide();

    void    Late_Update(float timeDelta) override;
    HRESULT Render() override;
    Shared<GameObject> Clone(void* arg) override;
    void    Free() override;

public:
    static Shared<BattleWorldUI_CommandCards> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);

private:
    struct Impl;
    Unique<Impl> _impl{};
};

NS_END

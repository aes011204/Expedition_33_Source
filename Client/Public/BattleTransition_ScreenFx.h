#pragma once
#include "ScreenFx_Instance.h"

NS_BEGIN(Engine)
class ShaderCom;
NS_END

NS_BEGIN(Client)

class BattleTransition_ScreenFx final : public ScreenFx_Instance
{
     GENERATED_SCREEN_FX(BattleTransition_ScreenFx)
public:
    struct BattleTransitionDesc : public ScreenFxDesc
    {
        float edgeSoftness{ 0.05f };
        float maxRadius{ 1.5f };
    };

public:
    BattleTransition_ScreenFx() = default;
    ~BattleTransition_ScreenFx() override = default;

public:
    HRESULT Initialize(ScreenFxDesc* desc) override;
    void Update_ScreenFx(float timeDelta) override;
    void Evaluate_At(float normalizedTime) override;
    HRESULT Bind_FxConstantBuffer(ShaderCom* shader) override;
    HRESULT Bind_ShaderResources(const Shared<ShaderCom>& shader) override;

private:
    BattleTransitionDesc _originalDesc{};
    float _currentRadius{};

public:
    static Shared<BattleTransition_ScreenFx> Create(BattleTransitionDesc* desc);
};

NS_END

#pragma once
#include "EffectInstancePool.h"
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
        bool useBlackSource{ false };
    };

public:
    BattleTransition_ScreenFx() = default;
    ~BattleTransition_ScreenFx() override;

public:
    HRESULT Initialize(ScreenFxDesc* desc) override;
    void Update_ScreenFx(float timeDelta) override;
    void Evaluate_At(float normalizedTime) override;
    HRESULT Bind_FxConstantBuffer(ShaderCom* shader) override;
    HRESULT Bind_ShaderResources(const Shared<ShaderCom>& shader) override;
    static const wchar_t* Get_RoseFlareEffectName();
    static const wchar_t* Get_RoseFlareLayerTag();
    static uint32 Get_RoseFlareLayerLevelIndex();

private:
    void RoseFlare();
    void Release_RoseFlare();

private:
    EffectInstancePool::AttachedHandle _roseFlareHandle{};
    bool _isRoseFlare{ false };
    
    ID3D11ShaderResourceView* _smudgeMaskSRV = nullptr;

    BattleTransitionDesc _originalDesc{};
    float _currentRadius{};

public:
    static Shared<BattleTransition_ScreenFx> Create(BattleTransitionDesc* desc);
};

NS_END

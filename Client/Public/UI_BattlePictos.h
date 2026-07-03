#pragma once

#include "UI_RuntimePanelBase.h"
#include "Client_Enum.h"

NS_BEGIN(Engine)
class UI_Text;
NS_END

NS_BEGIN(Client)

class UI_BattlePictos final : public UI_RuntimePanelBase
{
public:
    struct ActivationDesc
    {
        HeroId heroId = HeroId::END;
        string pictosId{};
        wstring displayName{};
        Vec2 anchorPosition{};
        uint32 stackIndex = 0;
        uint32 activationCount = 1;
    };

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void Update(float timeDelta) override;

    void Play_Activation(const ActivationDesc& desc);
    void Close() override;
    void Close_Immediately();

    HeroId Get_HeroId() const { return _heroId; }
    const string& Get_PictosId() const { return _pictosId; }
    uint32 Get_StackIndex() const { return _stackIndex; }
    float Get_Elapsed() const { return _elapsed; }

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void Reset_VisualState();
    void Begin_Deactivation();
    void Play_TargetAnimation(const wchar_t* animationName);
    void Stop_TargetAnimations();

private:
    Shared<UI_Text> _labelText{};
    Shared<UI_Text> _stackXText{};
    Shared<UI_Text> _stackValueText{};

    HeroId _heroId = HeroId::END;
    string _pictosId{};
    uint32 _stackIndex = 0;
    uint32 _activationCount = 1;
    float _elapsed = 0.f;
    bool _isDeactivating = false;

public:
    static Unique<UI_BattlePictos> Create(uint32 levelIndex);
    void Free() override;
};

NS_END

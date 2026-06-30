#pragma once

#include "UI_RuntimePanelBase.h"

NS_BEGIN(Engine)
class UI_Text;
NS_END

NS_BEGIN(Client)

class UI_GetChroma final : public UI_RuntimePanelBase
{
public:
    HRESULT Initialize(uint32 levelIndex) override;
    void Update(float timeDelta) override;

    void Open_ForChromaAmounts(const vector<uint32>& chromaAmounts);
    void Close() override;
    void Close_Immediately();

    static Unique<UI_GetChroma> Create(uint32 levelIndex);

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void Open_NextChroma();
    void Finish_CurrentAndOpenNext();
    void Close_Root();

private:
    Shared<Engine::UI_Text> _chromaValueText{};

    uint32 _activeChromaAmount = 0;
    vector<uint32> _queuedChromaAmounts{};

    float _elapsed = 0.f;
    float _animationDuration = 1.5f;
};

NS_END

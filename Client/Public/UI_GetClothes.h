#pragma once

#include "UI_RuntimePanelBase.h"

NS_BEGIN(Engine)
class UI_Base;
NS_END

NS_BEGIN(Client)

class UI_GetClothes final : public UI_RuntimePanelBase
{
public:
    HRESULT Initialize(uint32 levelIndex) override;
    void Update(float timeDelta) override;

    void Open_ForPresetIds(const vector<string>& presetIds);
    void Close() override;
    void Close_Immediately();

    static Unique<UI_GetClothes> Create(uint32 levelIndex);

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void Open_NextPreset();
    void Finish_CloseAndOpenNext();
    void Close_AllRoots();
    Shared<Engine::UI_Base> Resolve_Root(const string& presentationId) const;
    bool Is_PresetQueuedOrOpen(const string& presentationId) const;
    const wchar_t* Resolve_IntroAnimationName(const string& presentationId) const;
    const wchar_t* Resolve_OutroAnimationName(const string& presentationId) const;
    void Play_TargetAnimation(const wchar_t* animationName);
    void Stop_TargetAnimations();

private:
    Shared<Engine::UI_Base> _baguetteRoot{};
    Shared<Engine::UI_Base> _potatoBagRoot{};

    string _activePresentationId{};
    vector<string> _queuedPresentationIds{};

    bool _isClosingWithOutro = false;
    float _openElapsed = 0.f;
    float _closeElapsed = 0.f;
    float _outroDuration = 1.f;
};

NS_END



#pragma once

#include "Client_Defines.h"
#include "UI_RuntimePanelBase.h"

NS_BEGIN(Engine)
class UI_Text;
NS_END

NS_BEGIN(Client)

class UI_LevelAnnounce final : public UI_RuntimePanelBase
{
public:
    UI_LevelAnnounce() = default;
    ~UI_LevelAnnounce() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

public:
    void    Play_ForLevel(LevelType levelType);
    void    Close() override;
    void    Close_Immediately();

    static bool Is_SupportedLevel(LevelType levelType);

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void    Play_Animation();
    void    Stop_Animation();

    static const wchar_t* Resolve_LevelDisplayName(LevelType levelType);

private:
    Shared<UI_Text> _currentLevelText{};
    Shared<UI_Text> _currentLevelBgText{};
    vector<Shared<UI_Base>> _animationTargets{};

    LevelType _currentLevelType = LevelType::END;
    float _elapsed = 0.f;
    float _lifeTime = 0.f;

public:
    static Unique<UI_LevelAnnounce> Create(uint32 levelIndex);
    void Free() override;
};

NS_END

#pragma once

#include "UI_RuntimePanelBase.h"

NS_BEGIN(Engine)
class UI_Base;
class UI_Text;
NS_END

NS_BEGIN(Client)

class UI_GetCharacter final : public UI_RuntimePanelBase
{
public:
    HRESULT Initialize(uint32 levelIndex) override;
    void Update(float timeDelta) override;

    void Open_ForHeroes(const vector<HeroId>& heroIds);
    void Close() override;
    void Close_Immediately();

    static Unique<UI_GetCharacter> Create(uint32 levelIndex);

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void Play_TargetAnimation(const wchar_t* animationName);
    void Stop_TargetAnimations();

    void Set_HeroPanelVisible(HeroId heroId, bool visible);
    void Set_HeroLevelText(HeroId heroId);
    bool Contains_Hero(HeroId heroId) const;

private:
    Shared<UI_Base> _lunePanel{};
    Shared<UI_Base> _maellePanel{};
    Shared<UI_Text> _luneLevelText{};
    Shared<UI_Text> _maelleLevelText{};
    vector<HeroId> _showingHeroes{};
};

NS_END


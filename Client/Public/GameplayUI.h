#pragma once

#include "Base.h"

NS_BEGIN(Engine)
class UI_Base;
class UI_Text;
class UI_Button;
NS_END

NS_BEGIN(Client)

class SkillTree_Panel;
class CharacterStatus_Panel;
class UI_BattleTransition;

// 실제로 렌더링되는 UI 가 아니라, 플레이 중 UI 들을 로드/토글/바인딩 등 처리할 컨트롤러라고 생각
class GameplayUI : public Base
{
public:
    GameplayUI() = default;
    ~GameplayUI() override = default;

public:
    HRESULT Initialize(uint32 levelIndex);
    void    Update(float timeDelta);

    void    Play_BattleEnterTransition();
    void    Stop_BattleEnterTransition();
    bool    Is_BattleEnterTransitionPlaying() const;

private:
    Unique<CharacterStatus_Panel>   _characterStatusPanel{};
    Unique<SkillTree_Panel>         _skillTreePanel{};

    // 전투 진입 테스트용 페이드 UI
    Shared<UI_BattleTransition>     _battleTransition{};

public:
    static Unique<GameplayUI> Create(uint32 levelIndex);
    void Free() override;
};

NS_END

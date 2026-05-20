#pragma once

#include "Base.h"
#include "GameplayUI_Types.h"

NS_BEGIN(Client)

class GameplayUI_World;
class GameplayUI_Battle;

// 실제로 렌더링되는 UI 가 아니라, 플레이 중 UI 들을 로드/토글/바인딩 등 처리할 컨트롤러라고 생각
class GameplayUI : public Base
{
public:
    enum class GameplayUIInputMode
    {
        World,  // 월드 UI 입력만 허용
        Battle, // 배틀 UI 입력만 허용
    };

public:
    GameplayUI() = default;
    ~GameplayUI() override;

public:
    HRESULT Initialize(uint32 levelIndex);
    void    Update(float timeDelta);

    void    Play_BattleEnterTransition();
    void    Stop_BattleEnterTransition();
    bool    Is_BattleEnterTransitionPlaying() const;

    bool    Is_AnyMenuOpen() const;
    GameplayMenu Get_CurrentMenu() const; // 현재 최상위 메뉴 확인용

private:
    // 이번 프레임에 어떤 UI 입력인지. World ? Battle ?
    GameplayUIInputMode Resolve_InputMode() const;
    void                Handle_InputModeChanged(GameplayUIInputMode previousMode, GameplayUIInputMode currentMode);

private:
    Unique<GameplayUI_World>    _worldUI;  // 월드 UI는 GameplayUI 하나만 소유
    Shared<GameplayUI_Battle>   _battleUI; // 배틀 UI는 delegate 직접 구독 때문에 Shared로 둔다.

    GameplayUIInputMode         _lastInputMode = GameplayUIInputMode::World;

public:
    static Shared<GameplayUI> Create(uint32 levelIndex);
    void Free() override;
};

NS_END

#pragma once

#include "Base.h"

NS_BEGIN(Engine)
class UI_Base;
class UI_Text;
class UI_Button;
NS_END

NS_BEGIN(Client)

class CharacterStatus_Panel;

// 실제로 렌더링되는 UI 가 아니라, 플레이 중 UI 들을 로드/토글/바인딩 등 처리할 컨트롤러라고 생각
class GameplayUI : public Base
{
public:
    GameplayUI() = default;
    ~GameplayUI() override = default;

public:
    HRESULT Initialize(uint32 levelIndex);
    void    Update(float timeDelta);

private:
    Unique<CharacterStatus_Panel> _characterStatusPanel{};

public:
    static Unique<GameplayUI> Create(uint32 levelIndex);
    void Free() override;
};

NS_END

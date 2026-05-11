#pragma once

#include "Base.h"

NS_BEGIN(Engine)
class UI_Base;
class UI_Text;
class UI_Button;
NS_END

NS_BEGIN(Client)

// 실제로 렌더링되는 UI 가 아니라, 플레이 중 UI 들을 로드/토글/바인딩 등 처리할 컨트롤러라고 생각
class GameplayUI : public Base
{
public:
    GameplayUI() = default;
    ~GameplayUI() override = default;

public:
    HRESULT Initialize(uint32 levelIndex);
    void    Update(float timeDelta);

public:
    HRESULT Load_CharacterStatus(uint32 levelIndex);

    void    Toggle_CharacterStatus();
    void    Set_CharacterStatusOpen(bool isOpen);

private:
    Shared<UI_Base> Find_UI(
        const umap<wstring, Shared<UI_Base>>& uiByName, const wstring& name);

private:
    uint32 _levelIndex = 0;

    umap<wstring, Shared<UI_Base>> _characterStatusUIByName{};

    Shared<UI_Base> _characterStatusRoot{};
    bool _isCharacterStatusOpen = false;

public:
    static Unique<GameplayUI> Create(uint32 levelIndex);
    void Free() override;
};

NS_END

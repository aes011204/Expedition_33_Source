#pragma once
#include "Client_Defines.h"
#include "Level_WindowTitleFps.h"
#include "Level.h"

NS_BEGIN(Engine)
class UI_Base;
class UI_Button;
NS_END

NS_BEGIN(Client)

class Level_GameEntry final : public Level
{
public:
    Level_GameEntry(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual ~Level_GameEntry() = default;

public:
    virtual HRESULT Initialize() override;
    virtual void Update(float timeDelta) override;
    virtual HRESULT Render() override;

private:
    WindowTitleFpsState _windowTitleFps = {};

private:
    void    Handle_Button();

private:
    HRESULT Ready_Layer_UI(const wstring& strLayerTag);

private:
    umap<wstring, Shared<UI_Base>> _uiByName{};
    Shared<UI_Button> _firstLevelButton = nullptr;
    Shared<UI_Button> _secondLevelButton = nullptr;
    Shared<UI_Button> _thirdLevelButton = nullptr;
    bool _wasFirstLevelHovered = false;
    bool _wasSecondLevelHovered = false;
    bool _wasThirdLevelHovered = false;

public:
    static Shared<Level_GameEntry> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual void Free() override;
};

NS_END


#pragma once

#include "Client_Defines.h"
#include "Level_WindowTitleFps.h"
#include "Level.h"

NS_BEGIN(Engine)
class UI_Base;
class UI_Button;
NS_END

NS_BEGIN(Client)

class Level_MainTitle final : public Level
{
public:
	Level_MainTitle(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual ~Level_MainTitle() = default;

public:
	virtual HRESULT Initialize() override;
    virtual void BeginPlay() override;
	virtual void Update(float timeDelta) override;
	virtual HRESULT Render() override;

private:
    WindowTitleFpsState _windowTitleFps = {};

private:
    void    Handle_Button();

private:
    HRESULT Ready_Layer_UI(const wstring& strLayerTag);
    HRESULT Ready_Layer_Camera(const wstring& strLayerTag);

private:
    umap<wstring, Shared<UI_Base>> _uiByName{};
	Shared<UI_Button> _continueButton = nullptr; // "계속하기" 버튼
	Shared<UI_Button> _exitButton = nullptr;
    bool _wasContinueHovered = false;
    bool _wasExitHovered = false;

public:
	static Shared<Level_MainTitle> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual void Free() override;
};

NS_END

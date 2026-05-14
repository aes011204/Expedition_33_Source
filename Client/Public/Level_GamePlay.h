#pragma once

#include "Client_Defines.h"
#include "Level.h"

NS_BEGIN(Engine)
class UI_Base;
class UI_Button;
class UI_Text;
NS_END

NS_BEGIN(Client)

class Level_GamePlay final : public Level
{
public:
	Level_GamePlay(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual ~Level_GamePlay() = default;

public:
	HRESULT Initialize() override;
    void    BeginPlay() override;
	void    Update(float fTimeDelta) override;
	HRESULT Render() override;
private:
	HRESULT Ready_Lights();
    HRESULT Ready_Layer_UI(const wstring& strLayerTag);
	HRESULT Ready_Layer_Camera(const wstring& strLayerTag);
	HRESULT Ready_Layer_BackGround(const wstring& strLayerTag);
	HRESULT Ready_Layer_Monster(const wstring& strLayerTag);
	HRESULT Ready_Layer_Player(const wstring& strLayerTag);

private:
    unordered_map<wstring, Shared<UI_Base>> _uiByName{};

    Shared<UI_Text>   _hpText{};
    Shared<UI_Text>   _expText{};
    Shared<UI_Base>   _hpGauge{};
    Shared<UI_Base>   _expGauge{};
    Shared<UI_Button> _addHpButton{};
    Shared<UI_Button> _removeHpButton{};
    Shared<UI_Button> _addExpButton{};
    Shared<UI_Button> _removeExpButton{};

    Vec2  _hpGaugeOriginalSize{ 0.f, 0.f };
    Vec2  _expGaugeOriginalSize{ 0.f, 0.f };
    int32 _testCurrentHp{ 1000 };
    int32 _testMaxHp{ 1000 };
    int32 _testCurrentExp{ 200 };
    int32 _testMaxExp{ 200 };

public:
	static Shared<Level_GamePlay> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual void Free() override;
};

NS_END

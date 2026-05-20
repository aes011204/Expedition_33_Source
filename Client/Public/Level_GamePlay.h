#pragma once

#include "Client_Defines.h"
#include "Level.h"

NS_BEGIN(Engine)
class GameObject;
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
    HRESULT Ready_Layer_Effect(const wstring& strLayerTag);

    void    Sync_LuneRightHandEffect();

private:
    Shared<GameObject> _luneRightHandEffect{ nullptr };

public:
	static Shared<Level_GamePlay> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual void Free() override;
};

NS_END

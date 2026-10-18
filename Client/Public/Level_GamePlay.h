#pragma once

#include "Client_Defines.h"
#include "Level_WindowTitleFps.h"
#include "StreamingLevel.h"
#include "MapLoader.h"
#include "LightingPipeline.h"
#include "PostProcessPipeline.h"

NS_BEGIN(Engine)
class GameObject;
class UI_Base;
class UI_Button;
class UI_Text;
NS_END

NS_BEGIN(Client)

class Level_GamePlay final : public StreamingLevel
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
    HRESULT Ready_ChunkMap();
    HRESULT Ready_Lights();
    HRESULT Ready_Layer_UI(const wstring& strLayerTag);
	HRESULT Ready_Layer_Camera(const wstring& strLayerTag);
	HRESULT Ready_Layer_BackGround(const wstring& strLayerTag);
	HRESULT Ready_Layer_Monster(const wstring& strLayerTag);
    HRESULT Ready_Layer_Player(const wstring& strLayerTag);
    HRESULT Ready_Layer_Effect(const wstring& strLayerTag);
    HRESULT Ready_Layer_Puzzle(const wstring& strLayerTag);
    HRESULT Ready_Layer_Trigger(const wstring& strLayerTag);
    HRESULT Ready_Layer_BattleStartSpawner(const wstring& strLayerTag);
    HRESULT Ready_Layer_AmbientVisibility(const wstring& strLayerTag);
    void Disable_LandscapeLOD();

    void    Respawn_CinematicTrigger();
    void    Apply_BishopCinematicLightDirection();
    void    Apply_GamePlayRenderParams();
    void    Restore_GamePlayRenderParams();

private:
    WindowTitleFpsState _windowTitleFps = {};
    DeferredLightingCB _savedDeferredLightingCB{};
    SSAOCB _savedSSAOCB{};
    PostProcessFeature _savedPostProcessFeatures = PostProcessFeature::None;
    vector<LIGHT_DESC> _savedLightDescs{};
    bool _hasSavedGamePlayRenderParams = false;

public:
	static Shared<Level_GamePlay> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual void Free() override;
};

NS_END

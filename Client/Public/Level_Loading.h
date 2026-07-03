#pragma once

#include "Client_Defines.h"
#include "Level.h"

NS_BEGIN(Client)

class Loader;

class Level_Loading final : public Level
{
public:
	Level_Loading(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual ~Level_Loading() = default;

public:
	virtual HRESULT Initialize(LevelType eNextLevelID);
	virtual void    Update(float fTimeDelta) override;
	virtual HRESULT Render() override;

private:
	LevelType			    _nextLevel = { LevelType::END };
	Shared<Loader>          _loader = nullptr;
	chrono::steady_clock::time_point _loadingStartTime = {};
	bool                    _printedLoadCompleteLog = false;
	bool                    _hasRenderedFirstFrame = false;
	uint32                  _postFirstRenderFrameCount = 0;
    bool                    _requestedNextLevelTransition = false;

private:
    HRESULT Ready_LoadingUIRenderResources();
	HRESULT Ready_Layer_BackGround(const wchar_t* pLayerTag);
	HRESULT Ready_Layer_UI(const wchar_t* pLayerTag);
    void    Queue_BattleEffectPrewarmForNextLevel();

public:
	static Shared<Level_Loading> Create(const ComPtr<Device>& device, const ComPtr<Context>& context, LevelType eNextLevelID);
	virtual void Free() override;
};

NS_END

#pragma once

#include "Client_Defines.h"
#include "Level.h"

/* 1. 로딩 장면을 구성해주기위한 객체를 생성해주고 업데이트와 렌더의 반복적이 ㄴ수행. */
/* 2. 로딩 다음에 등장할 레벨을 위한 자원을 로드해준다.(Loader) == 로더를 생성해준다. */

NS_BEGIN(Client)

class Level_Loading final : public Level
{
private:
	Level_Loading(ComPtr<Device> device, ComPtr<Context> context);
	virtual ~Level_Loading() = default;

public:
	virtual HRESULT Initialize(LevelType eNextLevelID);
	virtual void Update(float fTimeDelta) override;
	virtual HRESULT Render() override;

private:
	LevelType			_nextLevel = { LevelType::END };
	Shared<class Loader> _loader = nullptr;

private:
	HRESULT Ready_Layer_BackGround(const wchar_t* pLayerTag);
	HRESULT Ready_Layer_UI(const wchar_t* pLayerTag);

public:
	static Shared<Level_Loading> Create(ComPtr<Device> device, ComPtr<Context> context, LevelType eNextLevelID);
	virtual void Free() override;
};

NS_END

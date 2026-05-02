#pragma once

#include "Client_Defines.h"
#include "Level.h"

NS_BEGIN(Client)

class Level_Logo final : public Level
{
public:
	Level_Logo(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual ~Level_Logo() = default;

public:
	virtual HRESULT Initialize() override;
	virtual void Update(float timeDelta) override;
	virtual HRESULT Render() override;

private:
	HRESULT Ready_Layer_BackGround(const wstring& strLayerTag);

public:
	static Shared<Level_Logo> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual void Free() override;
};

NS_END

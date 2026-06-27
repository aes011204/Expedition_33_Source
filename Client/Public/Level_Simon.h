#pragma once
#include "Client_Defines.h"
#include "Level_WindowTitleFps.h"
#include "Level.h"

// SmallLevel_SimonArea

NS_BEGIN(Client)

class Level_Simon final : public Level
{
public:
    Level_Simon(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual ~Level_Simon() = default;

public:
    virtual HRESULT Initialize() override;
    virtual void Update(float fTimeDelta) override;
    virtual HRESULT Render() override;

private:
    WindowTitleFpsState _windowTitleFps = {};

private:
    HRESULT Ready_Lights();

public:
    static Shared<Level_Simon> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual void Free() override;
};

NS_END

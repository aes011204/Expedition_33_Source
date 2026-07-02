#pragma once
#include "Client_Defines.h"
#include "Level_WindowTitleFps.h"
#include "Level.h"

// SmallLevel_SimonArea

NS_BEGIN(Client)

class Level_Stage01 final : public Level
{
public:
    Level_Stage01(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual ~Level_Stage01() = default;

public:
    virtual HRESULT Initialize() override;
    void BeginPlay() override;
    virtual void Update(float fTimeDelta) override;
    virtual HRESULT Render() override;

private:
    WindowTitleFpsState _windowTitleFps = {};

private:
    HRESULT Ready_Lights();
    HRESULT Ready_Layer_Player(const wstring& strLayerTag);
    HRESULT Ready_Layer_Camera(const wstring& strLayerTag);
    HRESULT Ready_Layer_UI(const wstring& strLayerTag);
    HRESULT Ready_Layer_BackGround(const wstring& strLayerTag);

public:
    static Shared<Level_Stage01> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual void Free() override;
};

NS_END

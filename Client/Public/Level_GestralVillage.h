#pragma once
#include "Client_Defines.h"
#include "Delegate.h"
#include "LightingPipeline.h"
#include "Level_WindowTitleFps.h"
#include "Level.h"

NS_BEGIN(Client)

class BattleStartSpawner;

class Level_GestralVillage final : public Level
{
public:
    Level_GestralVillage(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual ~Level_GestralVillage() = default;

public:
    virtual HRESULT Initialize() override;
    virtual void BeginPlay() override;
    virtual void Update(float fTimeDelta) override;
    virtual HRESULT Render() override;

public:
    const Shared<BattleStartSpawner> Get_BssPotatoBag() const { return _bssPotatoBag; }

private:
    HRESULT Ready_Lights();
    HRESULT Ready_Environments();
    HRESULT Ready_Layer_UI(const wstring& strLayerTag);
	HRESULT Ready_Layer_Camera(const wstring& strLayerTag);
    HRESULT Ready_Layer_BackGround(const wstring& strLayerTag);
    HRESULT Ready_Layer_Player(const wstring& strLayerTag);
    HRESULT Ready_Layer_NPC(const wstring& strLayerTag);
	HRESULT Ready_Layer_Monster(const wstring& strLayerTag);
    HRESULT Ready_Layer_Effect(const wstring& strLayerTag);
    HRESULT Ready_Layer_ETC(const wstring& strLayerTag);

    void    UnlockRules_GestralVillage();

    void    Play_MapBGM();
    void    Apply_StylizedLightContrast();
    void    Restore_StylizedLightContrast();

private:
    WindowTitleFpsState _windowTitleFps = {};
    FDelegateHandle _editorPlayRuntimeHandle{};
    DeferredLightingCB _savedDeferredLightingCB{};
    bool _hasSavedDeferredLightingCB = false;

private:
    Shared<BattleStartSpawner> _bssPotatoBag;

public:
    static Shared<Level_GestralVillage> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual void Free() override;
};

NS_END

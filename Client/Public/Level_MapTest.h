#pragma once
#include "Client_Defines.h"
#include "Level.h"

NS_BEGIN(Client)

class Level_MapTest final : public Level
{
public:
    Level_MapTest(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual ~Level_MapTest() = default;

public:
    virtual HRESULT Initialize() override;
    virtual void    BeginPlay() override;
    virtual void    Update(float fTimeDelta) override;
    virtual HRESULT Render() override;

private:
    HRESULT Ready_Lights();

public:
    static Shared<Level_MapTest> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual void Free() override;
};

NS_END

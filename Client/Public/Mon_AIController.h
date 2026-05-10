#pragma once
#include "Client_Defines.h"
#include"AIController.h"

NS_BEGIN(Client)
class Mon_AIController :
    public AIController
{

    GENERATED_COMPONENT(Mon_AIController)

public:
    Mon_AIController(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Mon_AIController(const Mon_AIController& prototype);
    virtual ~Mon_AIController() = default;
public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;

    void Update(float timeDelta);
public:

private:
    static Shared<Mon_AIController> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<Component> Clone(void* arg) override;
    virtual void Free() override;
};

NS_END

#pragma once
#include "Base.h"
#include "Delegate.h"
#include "Shader_CBuffer_Define.h"

NS_BEGIN(Engine)

class ShaderCom;
class ScreenFx_Manager;

class ENGINE_DLL ScreenFx_Instance abstract : public Base
{
public:
    using EndPredicate = function<bool>;
    DECLARE_DELEGATE(EndCondition, bool&);

    struct ScreenFxDesc
    {
        uint32 passType{};
        const function<void(bool&)> endPredicate{};
    };

public:
    ScreenFx_Instance();
    virtual ~ScreenFx_Instance() override = default;

public:
    virtual HRESULT Initialize(ScreenFxDesc* desc);
    bool Check_EndCondition();
    virtual void Update_ScreenFx(float timeDelta) { }
    virtual HRESULT Bind_FxConstantBuffer(ShaderCom* shader) PURE;
    virtual HRESULT Bind_ShaderResources(ShaderCom* shader) { return S_OK; }
    
public:
    uint32                  Get_PassIndex() const { return ETOI(_passType); }

protected:
    uint32                  _passType{};
    ScreenFxCB              _bufferData{};
    EndCondition            _endCondition{};

};

NS_END


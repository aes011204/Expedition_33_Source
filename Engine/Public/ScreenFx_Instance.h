#pragma once
#include "Base.h"
#include "Delegate.h"

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
        wstring tag{};
        float duration{};
        function<void(bool&)> endPredicate{};
    };

    virtual ClassReflectionInfo& Get_ReflectionInfo()
    {
        static ClassReflectionInfo empty{};
        return empty;
    }

public:
    ScreenFx_Instance();
    virtual ~ScreenFx_Instance() override = default;

public:
    virtual HRESULT Initialize(ScreenFxDesc* desc);
    bool Check_EndCondition();
    virtual void Update_ScreenFx(float timeDelta);
    virtual HRESULT Bind_FxConstantBuffer(ShaderCom* shader) PURE;
    virtual HRESULT Bind_ShaderResources(const Shared<ShaderCom>& shader);
    
public:
    uint32 Get_PassIndex() const { return _passType; }
    const wstring& Get_Tag() const { return _tag; }
    float Get_Duration() const { return _duration; }
    void Set_Duration(float duration) { _duration = duration; }
    float Get_ElapsedTime() const { return _elapsedTime; }
    void Set_ElapsedTime(float time) { _elapsedTime = time; }
    virtual void Evaluate_At(float normalizedTime) {}

protected:
    float                   _duration{ 0.f };
    float                   _elapsedTime{ 0.f };

    wstring                 _tag{};
    uint32                  _passType{};
    EndCondition            _endCondition{};

};

NS_END


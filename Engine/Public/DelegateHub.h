#pragma once

#include "Base.h"
#include "Delegate.h"

NS_BEGIN(Engine)

DECLARE_DELEGATE(FOnEditorStopRuntime);
DECLARE_DELEGATE(FOnEditorPlayRuntime);
DECLARE_DELEGATE(FOnEditorPauseRuntime);
DECLARE_DELEGATE(FOnEditorResumeRuntime);

// 그냥 전역으로 사용할 델리게이트들 모아놓는 클래스

class ENGINE_DLL DelegateHub final : public Base
{
public:
    DelegateHub() = default;
    ~DelegateHub() override = default;

public:
    FOnEditorStopRuntime   OnEditorStopRuntime;
    FOnEditorPlayRuntime   OnEditorPlayRuntime;
    FOnEditorPauseRuntime  OnEditorPauseRuntime;
    FOnEditorResumeRuntime OnEditorResumeRuntime;

public:
    void Free() override;
};

NS_END

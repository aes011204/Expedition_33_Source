#include "ANS_AnimSpeed.h"

#include "ModelCom.h"

IMPLEMENT_REFLECTION(ANS_AnimSpeed)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_FLOAT("애니메이션 배속 배율", _frameSpeed, 0.01f, 10.f, 0.01f);

    return true;
}

void ANS_AnimSpeed::On_Begin(const AnimNotifyContext& context)
{
    if (context.model == nullptr)
        return;

    _restoreFrameSpeed = context.model->Get_FrameSpeed(); // 기존 phase playRate나 preview 속도를 보존한다.
    _isApplied = true;

    const float speedMultiplier = max(0.01f, _frameSpeed);
    const float frameSpeed = max(0.01f, _restoreFrameSpeed * speedMultiplier);
    context.model->Set_FrameSpeed(frameSpeed);
}

void ANS_AnimSpeed::On_Tick(const AnimNotifyContext& context)
{
    UNREFERENCED_PARAMETER(context);
}

void ANS_AnimSpeed::On_End(const AnimNotifyContext& context)
{
    if (context.model == nullptr || !_isApplied)
        return;

    context.model->Set_FrameSpeed(_restoreFrameSpeed); // 구간 진입 직전 값으로 복구한다.
    _restoreFrameSpeed = 1.f;
    _isApplied = false;
}

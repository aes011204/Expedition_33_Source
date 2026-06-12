#pragma once

#include "AnimNotify.h"

NS_BEGIN(Client)

// 단체 패링 이후, 돌아가는 타이밍 조절용 노티파이.
// 루네의 카운터 애니메이션이 제일 길어서, 루네 Counter에 세팅
class AN_CounterReturnAll : public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_CounterReturnAll)

public:
    string Get_TypeName() const override { return "AN_CounterReturnAll"; }
    void Execute(const AnimNotifyContext& context) override;
};

NS_END

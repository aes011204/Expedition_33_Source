#pragma once
#include "Base.h"

NS_BEGIN(Engine)

class AnimationNotify final : public Base
{
public:
    struct AnimationNotify_Block
    {
        
    };
    struct AnimationNotify_Instance
    {

    };

private:
    HRESULT Initialize();

public:
    AnimationNotify();
    AnimationNotify(const AnimationNotify& rhs);
    ~AnimationNotify() override = default;

public:
    void Add_Notify() {};
    void Remove_Notify() {};
    void Reset_Notify() {};

public:
    void Update_Notify(float timeDelta);

private:
    

public:
    static Shared<AnimationNotify> Create();
    Shared<AnimationNotify> Clone();
    void Free() override;
};

NS_END

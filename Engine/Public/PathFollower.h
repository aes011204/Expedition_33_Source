#pragma once
#include "Base.h"

NS_BEGIN(Engine)
class ENGINE_DLL PathFollower :
    public Base
{
public:
    PathFollower();
    virtual ~PathFollower();
public:
    void Update(float deltaTime, Vec3& position, float speed);
   
    bool Is_Finished();
    void Request_Move(Vec3& start, Vec3& target);
    void Stop_Move(const Vec3& currentPos);

private:
    Vec3 _position = {};
    Vec3 _target = {};

    bool _fin = {false};

public:

    static Shared<PathFollower> Create();
   // virtual Shared<Component> Clone(void* arg) override;
    virtual void Free() override;
    
};

NS_END

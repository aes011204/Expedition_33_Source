#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class ColliderCom;

class ENGINE_DLL Collision_Manager : public Base
{
public:
    Collision_Manager() = default;
    virtual ~Collision_Manager() = default;

public:
    HRESULT Initialize();
    void    Update();

#ifdef _DEBUG
    void Render_Debug();
#endif

public:
    void    Add_Collider(Shared<ColliderCom> collider);
    void    Clear_Colliders();

private:
    vector<Weak<ColliderCom>> _colliders;
    bool    _isDebug = true;

public:
    static Unique<Collision_Manager> Create();
    virtual void Free() override;

};

NS_END

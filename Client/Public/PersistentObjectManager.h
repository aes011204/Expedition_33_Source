#pragma once
#include "Client_Defines.h"
#include "Base.h"

NS_BEGIN(Engine)
class GameObject;
NS_END

NS_BEGIN(Client)

class PersistentObjectManager : public Base
{
public:
    PersistentObjectManager() = default;
    virtual ~PersistentObjectManager() = default;

private:
    HRESULT SpawnStaticCamera();
    HRESULT SpawnStaticPlayer();
    HRESULT SpawnStaticSky();

public:
    HRESULT Spawn_StaticObjects();
    Shared<GameObject> Find_Player() const;

public:
    static Unique<PersistentObjectManager> Create();
    virtual void Free() override;
};

NS_END

#pragma once
NS_BEGIN(Engine)

struct EvtHandle
{
    size_t id = 0;
    bool IsValid() const { return id != 0;}
};

struct EvtSelectedGameObject
{
    Shared<GameObject> gameObject;   
};

struct EvtClearSelection {};

NS_END


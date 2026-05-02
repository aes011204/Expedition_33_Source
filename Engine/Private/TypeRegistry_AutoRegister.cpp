#include "TypeRegistry_AutoRegister.h"

vector<TypeRegistry::ComponentTypeInfo>& Get_PendingComponentTypeInfos()
{
    static vector<TypeRegistry::ComponentTypeInfo> infos{};
    return infos;
}

vector<TypeRegistry::GameObjectTypeInfo>& Get_PendingGameObjectTypeInfos()
{
    static vector<TypeRegistry::GameObjectTypeInfo> infos{};
    return infos;
}

vector<TypeRegistry::BTNodeTypeInfo>& Get_PendingBTNodeTypeInfos()
{
    static vector<TypeRegistry::BTNodeTypeInfo> infos{};
    return infos;
}

ComponentTypeAutoRegistrar::ComponentTypeAutoRegistrar(const TypeRegistry::ComponentTypeInfo& info)
{
    Get_PendingComponentTypeInfos().push_back(info);
}

GameObjectTypeAutoRegistrar::GameObjectTypeAutoRegistrar(const TypeRegistry::GameObjectTypeInfo& info)
{
    Get_PendingGameObjectTypeInfos().push_back(info);
}

BTNodeTypeAutoRegistrar::BTNodeTypeAutoRegistrar(const TypeRegistry::BTNodeTypeInfo& info)
{
    Get_PendingBTNodeTypeInfos().push_back(info);
}

HRESULT Flush_PendingTypeRegistrations(TypeRegistry& registry)
{
    for (const auto& info : Get_PendingComponentTypeInfos())
    {
        if (FAILED(registry.Register_ComponentType(info)))
            return E_FAIL;
    }

    for (const auto& info : Get_PendingGameObjectTypeInfos())
    {
        if (FAILED(registry.Register_GameObjectType(info)))
            return E_FAIL;
    }

    for (const auto& info : Get_PendingBTNodeTypeInfos())
    {
        if (FAILED(registry.Register_BTNodeType(info)))
            return E_FAIL;
    }

    return S_OK;
}


#include "RegistorFactory.h"
#include "TypeRegistry.h"

namespace Engine
{
vector<ComponentRegistorInfo>& Get_PendingComponentTypeInfos()
{
    static vector<ComponentRegistorInfo> infos{};
    return infos;
}

vector<GameObjectRegistorInfo>& Get_PendingGameObjectTypeInfos()
{
    static vector<GameObjectRegistorInfo> infos{};
    return infos;
}

vector<BTNodeRegistorInfo>& Get_PendingBTNodeTypeInfos()
{
    static vector<BTNodeRegistorInfo> infos{};
    return infos;
}

bool Register_ComponentType_Pending(const char* className, const ComponentCreateFn& createFn, ClassReflectionInfo* reflectionInfo)
{
    if (nullptr == className || nullptr == reflectionInfo || !createFn)
        return false;

    Get_PendingComponentTypeInfos().push_back({ className, createFn, reflectionInfo });
    return true;
}

bool Register_GameObjectType_Pending(const char* className, ObjectType objectType, const GameObjectCreateFn& createFn, ClassReflectionInfo* reflectionInfo)
{
    if (nullptr == className || nullptr == reflectionInfo || !createFn)
        return false;

    Get_PendingGameObjectTypeInfos().push_back({ className, objectType, createFn, reflectionInfo });
    return true;
}

bool Register_BTNodeType_Pending(const char* className, const string& nodeCategory, const BTNodeCreateFn& createFn, ClassReflectionInfo* reflectionInfo)
{
    if (nullptr == className || nullptr == reflectionInfo || !createFn)
        return false;

    Get_PendingBTNodeTypeInfos().push_back({ className, nodeCategory, createFn, reflectionInfo });
    return true;
}

HRESULT Flush_PendingTypeRegistrations(TypeRegistry& registry)
{
    for (const auto& info : Get_PendingComponentTypeInfos())
    {
        TypeRegistry::ComponentTypeInfo typeInfo{};
        typeInfo.className = info.className;
        typeInfo.createFn = info.createFn;
        typeInfo.reflectionInfo = info.reflectionInfo;

        if (FAILED(registry.Register_ComponentType(typeInfo)))
            return E_FAIL;
    }

    for (const auto& info : Get_PendingGameObjectTypeInfos())
    {
        TypeRegistry::GameObjectTypeInfo typeInfo{};
        typeInfo.className = info.className;
        typeInfo.objectType = info.objectType;
        typeInfo.createFn = info.createFn;
        typeInfo.reflectionInfo = info.reflectionInfo;

        if (FAILED(registry.Register_GameObjectType(typeInfo)))
            return E_FAIL;
    }

    for (const auto& info : Get_PendingBTNodeTypeInfos())
    {
        TypeRegistry::BTNodeTypeInfo typeInfo{};
        typeInfo.className = info.className;
        typeInfo.nodeCategory = info.nodeCategory;
        typeInfo.createFn = info.createFn;
        typeInfo.reflectionInfo = info.reflectionInfo;

        if (FAILED(registry.Register_BTNodeType(typeInfo)))
            return E_FAIL;
    }

    return S_OK;
}
}

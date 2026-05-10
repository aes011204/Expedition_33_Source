#include "TypeRegistry.h"

HRESULT TypeRegistry::Register_ComponentType(const ComponentTypeInfo& info)
{
    if (info.className.empty() || !info.createFn)
        return E_FAIL;

    if (_componentTypes.contains(info.className))
        return E_FAIL;

    _componentTypes.emplace(info.className, info);

    if (nullptr != info.reflectionInfo)
    {
        info.reflectionInfo->className = info.className;
        info.reflectionInfo->displayName = info.className;
        info.reflectionInfo->type = RegisteredType::Component;
    }

    return S_OK;
}

HRESULT TypeRegistry::Register_GameObjectType(const GameObjectTypeInfo& info)
{
    if (info.className.empty() || !info.createFn)
        return E_FAIL;

    if (_gameObjectTypes.contains(info.className))
        return E_FAIL;

    _gameObjectTypes.emplace(info.className, info);

    if (nullptr != info.reflectionInfo)
    {
        info.reflectionInfo->className = info.className;
        info.reflectionInfo->displayName = info.className;
        info.reflectionInfo->type = RegisteredType::GameObject;
    }

    return S_OK;
}

HRESULT TypeRegistry::Register_BTNodeType(const BTNodeTypeInfo& info)
{
    if (info.className.empty() || !info.createFn)
        return E_FAIL;

    if (_btNodeTypes.contains(info.className))
        return E_FAIL;

    _btNodeTypes.emplace(info.className, info);

    if (nullptr != info.reflectionInfo)
    {
        info.reflectionInfo->className = info.className;
        info.reflectionInfo->displayName = info.className;
        info.reflectionInfo->type = RegisteredType::BTNode;
    }

    return S_OK;
}

HRESULT TypeRegistry::Register_AnimNotifyType(const AnimNotifyTypeInfo& info)
{
    if (info.className.empty() || !info.createFn)
        return E_FAIL;

    if (_animNotifyTypes.contains(info.className))
        return E_FAIL;

    _animNotifyTypes.emplace(info.className, info);

    if (nullptr != info.reflectionInfo)
    {
        info.reflectionInfo->className = info.className;
        info.reflectionInfo->displayName = info.className;
        info.reflectionInfo->type = RegisteredType::AnimNotify;
    }

    return S_OK;
}

HRESULT TypeRegistry::Register_AnimNotifyStateType(const AnimNotifyStateTypeInfo& info)
{
    if (info.className.empty() || !info.createFn)
        return E_FAIL;

    if (_animNotifyStateTypes.contains(info.className))
        return E_FAIL;

    _animNotifyStateTypes.emplace(info.className, info);

    if (nullptr != info.reflectionInfo)
    {
        info.reflectionInfo->className = info.className;
        info.reflectionInfo->displayName = info.className;
        info.reflectionInfo->type = RegisteredType::AnimNotifyState;
    }

    return S_OK;
}

const TypeRegistry::ComponentTypeInfo* TypeRegistry::Find_ComponentType(const string& className) const
{
    auto iter = _componentTypes.find(className);
    return (iter == _componentTypes.end()) ? nullptr : &iter->second;
}

const TypeRegistry::GameObjectTypeInfo* TypeRegistry::Find_GameObjectType(const string& className) const
{
    auto iter = _gameObjectTypes.find(className);
    return (iter == _gameObjectTypes.end()) ? nullptr : &iter->second;
}

void TypeRegistry::Visit_GameObjectTypes(const function<void(const GameObjectTypeInfo&)>& visitor) const
{
    if (!visitor)
        return;

    for (const auto& [_, info] : _gameObjectTypes)
        visitor(info);
}

void TypeRegistry::Visit_AnimNotifyTypes(const function<void(const AnimNotifyTypeInfo&)>& visitor) const
{
    if (!visitor)
        return;

    for (const auto& [_, info] : _animNotifyTypes)
        visitor(info);
}

void TypeRegistry::Visit_AnimNotifyStateTypes(const function<void(const AnimNotifyStateTypeInfo&)>& visitor) const
{
    if (!visitor)
        return;

    for (const auto& [_, info] : _animNotifyStateTypes)
        visitor(info);
}

const TypeRegistry::BTNodeTypeInfo* TypeRegistry::Find_BTNodeType(const string& className) const
{
    auto iter = _btNodeTypes.find(className);
    return (iter == _btNodeTypes.end()) ? nullptr : &iter->second;
}

const TypeRegistry::AnimNotifyTypeInfo* TypeRegistry::Find_AnimNotifyType(const string& className) const
{
     auto iter = _animNotifyTypes.find(className);
    return (iter == _animNotifyTypes.end()) ? nullptr : &iter->second;
}

const TypeRegistry::AnimNotifyStateTypeInfo* TypeRegistry::Find_AnimNotifyStateType(const string& className) const
{
    auto iter = _animNotifyStateTypes.find(className);
    return (iter == _animNotifyStateTypes.end()) ? nullptr : &iter->second;
}

Shared<Component> TypeRegistry::Create_Component(
    const string& className,
    const ComPtr<Device>& device,
    const ComPtr<Context>& context) const
{
    const auto* info = Find_ComponentType(className);
    return (info == nullptr) ? nullptr : info->createFn(device, context);
}

Shared<GameObject> TypeRegistry::Create_GameObject(
    const string& className,
    const ComPtr<Device>& device,
    const ComPtr<Context>& context) const
{
    const auto* info = Find_GameObjectType(className);
    return (info == nullptr) ? nullptr : info->createFn(device, context);
}

Shared<Base> TypeRegistry::Create_BTNode(const string& className) const
{
    const auto* info = Find_BTNodeType(className);
    return (info == nullptr) ? nullptr : info->createFn();
}

Shared<AnimNotify> TypeRegistry::Create_AnimNotify(const string& className) const
{
    const auto* info = Find_AnimNotifyType(className);
    return (info == nullptr) ? nullptr : info->createFn();
}

Shared<AnimNotifyState> TypeRegistry::Create_AnimNotifyState(const string& className) const
{
    const auto* info = Find_AnimNotifyStateType(className);
    return (info == nullptr) ? nullptr : info->createFn();
}

void TypeRegistry::Visit_BTNodeTypes(const function<void(const BTNodeTypeInfo&)>& visitor) const
{
    for (const auto& [_, info] : _btNodeTypes)
    {
        visitor(info);
    }
}

Unique<TypeRegistry> TypeRegistry::Create()
{
    return Unique<TypeRegistry>{ new TypeRegistry() };
}

void TypeRegistry::Free()
{
    _componentTypes.clear();
    _gameObjectTypes.clear();
    _btNodeTypes.clear();
    _animNotifyTypes.clear();
    _animNotifyStateTypes.clear();

    Base::Free();
}

#pragma once

#include "Base.h"
#include "Reflection_Types.h"

NS_BEGIN(Engine)

class Component;
class GameObject;
class AnimNotify;
class AnimNotifyState;

class TypeRegistry final : public Base
{
public:
    // Component
    using ComponentCreateFn =
        function<Shared<Component>(const ComPtr<Device>&, const ComPtr<Context>&)>;

    // GameObject
    using GameObjectCreateFn =
        function<Shared<GameObject>(const ComPtr<Device>&, const ComPtr<Context>&)>;

    // TODO : Behavior Tree는 추가로 바꿀 예정
    using BTNodeCreateFn =
        function<Shared<Base>()>;

    // AnimNotify
    using AnimNotifyCreateFn =
        function<Shared<AnimNotify>()>;

    // AnimNotifyState
    using AnimNotifyStateCreateFn =
        function<Shared<AnimNotifyState>()>;

    struct ComponentTypeInfo
    {
        string className = {};
        ComponentCreateFn createFn = {};
        ClassReflectionInfo* reflectionInfo = nullptr;
    };

    struct GameObjectTypeInfo
    {
        string className = {};
        ObjectType objectType = ObjectType::None;
        GameObjectCreateFn createFn = {};
        ClassReflectionInfo* reflectionInfo = nullptr;
    };

    struct BTNodeTypeInfo
    {
        string className = {};
        string nodeCategory = {};
        BTNodeCreateFn createFn = {};
        ClassReflectionInfo* reflectionInfo = nullptr;
    };

    struct AnimNotifyTypeInfo
    {
        string className = {};
        AnimNotifyCreateFn createFn = {};
        ClassReflectionInfo* reflectionInfo = nullptr;
    };

    struct AnimNotifyStateTypeInfo
    {
        string className = {};
        AnimNotifyStateCreateFn createFn = {};
        ClassReflectionInfo* reflectionInfo = nullptr;
    };

public:
    HRESULT Register_ComponentType(const ComponentTypeInfo& info);
    HRESULT Register_GameObjectType(const GameObjectTypeInfo& info);
    HRESULT Register_BTNodeType(const BTNodeTypeInfo& info);
    HRESULT Register_AnimNotifyType(const AnimNotifyTypeInfo& info);
    HRESULT Register_AnimNotifyStateType(const AnimNotifyStateTypeInfo& info);

    const ComponentTypeInfo*        Find_ComponentType(const string& className) const;
    const GameObjectTypeInfo*       Find_GameObjectType(const string& className) const;
    const BTNodeTypeInfo*           Find_BTNodeType(const string& className) const;
    const AnimNotifyTypeInfo*       Find_AnimNotifyType(const string& className) const;
    const AnimNotifyStateTypeInfo*  Find_AnimNotifyStateType(const string& className) const;

    void Visit_GameObjectTypes(const function<void(const GameObjectTypeInfo&)>& visitor) const;
    void Visit_AnimNotifyTypes(const function<void(const AnimNotifyTypeInfo&)>& visitor) const;
    void Visit_AnimNotifyStateTypes(const function<void(const AnimNotifyStateTypeInfo&)>& visitor) const;

    Shared<Component> Create_Component(
        const string& className,
        const ComPtr<Device>& device,
        const ComPtr<Context>& context) const;

    Shared<GameObject> Create_GameObject(
        const string& className,
        const ComPtr<Device>& device,
        const ComPtr<Context>& context) const;

    Shared<Base> Create_BTNode(const string& className) const;

    Shared<AnimNotify> Create_AnimNotify(const string& className) const;
    Shared<AnimNotifyState> Create_AnimNotifyState(const string& className) const;

public:
    void Visit_BTNodeTypes(const function<void(const BTNodeTypeInfo&)>& visitor) const;

public:
    static Unique<TypeRegistry> Create();
    void Free() override;

private:
    map<string, ComponentTypeInfo>          _componentTypes{};
    map<string, GameObjectTypeInfo>         _gameObjectTypes{};
    map<string, BTNodeTypeInfo>             _btNodeTypes{};

    map<string, AnimNotifyTypeInfo>         _animNotifyTypes{};
    map<string, AnimNotifyStateTypeInfo>    _animNotifyStateTypes{};
};

NS_END

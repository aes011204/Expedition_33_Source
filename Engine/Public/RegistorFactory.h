#pragma once

#include "Reflection_Types.h"

#ifndef ENGINE_DLL
#ifdef ENGINE_EXPORTS
#define ENGINE_DLL __declspec(dllexport)
#else
#define ENGINE_DLL __declspec(dllimport)
#endif
#endif

namespace Engine
{
    class Base;
    class Component;
    class GameObject;
    class TypeRegistry;

    class AnimNotify;
    class AnimNotifyState;

    using ComponentCreateFn = std::function<Shared<Component>(const ComPtr<Device>&, const ComPtr<Context>&)>;
    using GameObjectCreateFn = std::function<Shared<GameObject>(const ComPtr<Device>&, const ComPtr<Context>&)>;
    using BTNodeCreateFn = std::function<Shared<Base>()>;
    using AnimNotifyCreateFn = std::function<Shared<AnimNotify>()>;
    using AnimNotifyStateCreateFn = std::function<Shared<AnimNotifyState>()>;

    struct ComponentRegistorInfo
    {
        std::string className = {};
        ComponentCreateFn createFn = {};
        ClassReflectionInfo* reflectionInfo = nullptr;
    };

    struct GameObjectRegistorInfo
    {
        std::string className = {};
        ObjectType objectType = ObjectType::None;
        GameObjectCreateFn createFn = {};
        ClassReflectionInfo* reflectionInfo = nullptr;
    };

    struct BTNodeRegistorInfo
    {
        std::string className = {};
        std::string nodeCategory = {};
        BTNodeCreateFn createFn = {};
        ClassReflectionInfo* reflectionInfo = nullptr;
    };

    struct AnimNotifyRegistorInfo
    {
        std::string className = {};
        AnimNotifyCreateFn createFn = {};
        ClassReflectionInfo* reflectionInfo = nullptr;
    };

    struct AnimNotifyStateRegistorInfo
    {
        std::string className = {};
        AnimNotifyStateCreateFn createFn = {};
        ClassReflectionInfo* reflectionInfo = nullptr;
    };

    template<typename T>
    Shared<Component> Create_ComponentPrototypeInstance(const ComPtr<Device>& device, const ComPtr<Context>& context)
    {
        auto instance = make_shared<T>(device, context);
        if (FAILED(instance->Initialize_Prototype()))
            return nullptr;

        return static_pointer_cast<Component>(instance);
    }

    template<typename T>
    Shared<GameObject> Create_GameObjectPrototypeInstance(const ComPtr<Device>& device, const ComPtr<Context>& context)
    {
        auto instance = make_shared<T>(device, context);
        if (FAILED(instance->Initialize_Prototype()))
            return nullptr;

        return static_pointer_cast<GameObject>(instance);
    }

    template<typename T>
    Shared<Base> Create_BTNodePrototypeInstance()
    {
        auto instance = make_shared<T>();
        return static_pointer_cast<Base>(instance);
    }

    template<typename T>
    Shared<AnimNotify> Create_AnimNotifyPrototypeInstance()
    {
        auto instance = make_shared<T>();
        return static_pointer_cast<AnimNotify>(instance);
    }

    template<typename T>
    Shared<AnimNotifyState> Create_AnimNotifyStatePrototypeInstance()
    {
        auto instance = make_shared<T>();
        return static_pointer_cast<AnimNotifyState>(instance);
    }


    ENGINE_DLL bool Register_ComponentType_Pending(const char* className, const ComponentCreateFn& createFn, ClassReflectionInfo* reflectionInfo);
    ENGINE_DLL bool Register_GameObjectType_Pending(const char* className, ObjectType objectType, const GameObjectCreateFn& createFn, ClassReflectionInfo* reflectionInfo);
    ENGINE_DLL bool Register_BTNodeType_Pending(const char* className, const std::string& nodeCategory, const BTNodeCreateFn& createFn, ClassReflectionInfo* reflectionInfo);

    ENGINE_DLL bool Register_AnimNotifyType_Pending(const char* className, const AnimNotifyCreateFn& createFn, ClassReflectionInfo* reflectionInfo);
    ENGINE_DLL bool Register_AnimNotifyStateType_Pending(const char* className, const AnimNotifyStateCreateFn& createFn, ClassReflectionInfo* reflectionInfo);
	ENGINE_DLL bool Register_GlobalEnum_Pending(const char* displayName, const std::vector<std::string>& enumNames, const std::vector<int>& enumValues);

    ENGINE_DLL HRESULT Flush_PendingTypeRegistrations(TypeRegistry& registry);
}

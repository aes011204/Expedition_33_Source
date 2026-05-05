#pragma once

#include "Reflection_Types.h"
#include "RegistorFactory.h"

#define STRINGIZE2(x) #x
#define STRINGIZE(x) STRINGIZE2(x)
#define WIDEN2(x) L##x
#define WIDEN(x) WIDEN2(x)
#define WSTRINGIZE(x) WIDEN(STRINGIZE(x))

#define GENERATED_BASE_TYPE_BODY(ClassName)                                        \
public:                                                                            \
    using SelfType = ClassName;                                                    \
    static const wchar_t* StaticClassName() { return WSTRINGIZE(ClassName); }      \
    static const char* StaticClassNameA() { return STRINGIZE(ClassName); }         \
    static bool Register_Properties();                                             \
    static Engine::ClassReflectionInfo& GetStaticReflectionInfo()                  \
    {                                                                              \
        static Engine::ClassReflectionInfo info = []()                             \
        {                                                                          \
            Engine::ClassReflectionInfo newInfo{};                                 \
            newInfo.className = STRINGIZE(ClassName);                              \
            newInfo.displayName = STRINGIZE(ClassName);                            \
            return newInfo;                                                        \
        }();                                                                       \
        return info;                                                               \
    }                                                                              \
    Engine::ClassReflectionInfo& Get_ReflectionInfo() override                     \
    {                                                                              \
        return GetStaticReflectionInfo();                                          \
    }                                                                              \
private:                                                                           \
    bool _name_setter_ = [this]()                                                  \
    {                                                                              \
        this->Set_Name(StaticClassName());                                         \
        return true;                                                               \
    }();

#define GENERATED_COMPONENT(ClassName)                                             \
    GENERATED_BASE_TYPE_BODY(ClassName)                                            \
public:                                                                            \
    inline static const bool _auto_register_component_ =                           \
        Engine::Register_ComponentType_Pending(                                    \
            STRINGIZE(ClassName),                                                  \
            [](const ComPtr<Device>& device, const ComPtr<Context>& context)       \
                -> Shared<Engine::Component>                                       \
            {                                                                      \
                return Engine::Create_ComponentPrototypeInstance<ClassName>(device, context); \
            },                                                                     \
            &ClassName::GetStaticReflectionInfo());

#define GENERATED_GAMEOBJECT(ClassName, ObjectTypeValue)                           \
    GENERATED_BASE_TYPE_BODY(ClassName)                                            \
public:                                                                            \
    static constexpr Engine::ObjectType StaticObjectType() { return ObjectTypeValue; } \
    Engine::ObjectType Get_ObjectType() const override                             \
    {                                                                              \
        return StaticObjectType();                                                 \
    }                                                                              \
    inline static const bool _auto_register_gameobject_ =                          \
        Engine::Register_GameObjectType_Pending(                                   \
            STRINGIZE(ClassName),                                                  \
            ObjectTypeValue,                                                       \
            [](const ComPtr<Device>& device, const ComPtr<Context>& context)       \
                -> Shared<Engine::GameObject>                                      \
            {                                                                      \
                return Engine::Create_GameObjectPrototypeInstance<ClassName>(device, context); \
            },                                                                     \
            &ClassName::GetStaticReflectionInfo());

#define GENERATED_BTNODE(ClassName, NodeCategory)                                  \
    GENERATED_BASE_TYPE_BODY(ClassName)                                            \
public:                                                                            \
    inline static const bool _auto_register_btnode_ =                              \
        Engine::Register_BTNodeType_Pending(                                       \
            STRINGIZE(ClassName),                                                  \
            NodeCategory,                                                          \
            []() -> Shared<Engine::Base>                                           \
            {                                                                      \
                return Engine::Create_BTNodePrototypeInstance<ClassName>();        \
            },                                                                     \
            &ClassName::GetStaticReflectionInfo());

#define IMPLEMENT_REFLECTION(ClassName)                                            \
    namespace                                                                      \
    {                                                                              \
        const bool g_auto_register_properties_##ClassName =                        \
            ClassName::Register_Properties();                                      \
    }                                                                              \
    bool ClassName::Register_Properties()

#define REFLECT_BEGIN(ClassName)                                                   \
    namespace                                                                      \
    {                                                                              \
        const bool g_reflect_##ClassName = []()                                    \
        {                                                                          \
            using SelfType = ClassName;                                            \
            auto& info = ClassName::GetStaticReflectionInfo();

#define REFLECT_END                                                                \
            return true;                                                           \
        }();                                                                       \
    }

#define PROPERTY_FLOAT(DisplayName, Member, MinValue, MaxValue, DragSpeed)         \
    info.properties.push_back(Engine::PropertyInfo{                                \
        STRINGIZE(Member), DisplayName, STRINGIZE(Member),                         \
        Engine::PropertyType::Float, offsetof(SelfType, Member),                   \
        MinValue, MaxValue, DragSpeed })

#define PROPERTY_INT(DisplayName, Member, MinValue, MaxValue, DragSpeed)           \
    info.properties.push_back(Engine::PropertyInfo{                                \
        STRINGIZE(Member), DisplayName, STRINGIZE(Member),                         \
        Engine::PropertyType::Int, offsetof(SelfType, Member),                     \
        MinValue, MaxValue, DragSpeed })

#define PROPERTY_BOOL(DisplayName, Member)                                         \
    info.properties.push_back(Engine::PropertyInfo{                                \
        STRINGIZE(Member), DisplayName, STRINGIZE(Member),                         \
        Engine::PropertyType::Bool, offsetof(SelfType, Member) })

#define PROPERTY_VEC3(DisplayName, Member, DragSpeed)                              \
    info.properties.push_back(Engine::PropertyInfo{                                \
        STRINGIZE(Member), DisplayName, STRINGIZE(Member),                         \
        Engine::PropertyType::Vec3, offsetof(SelfType, Member),                    \
        0.f, 0.f, DragSpeed })

#define PROPERTY_STRING(DisplayName, Member)                                       \
    info.properties.push_back(Engine::PropertyInfo{                                \
        STRINGIZE(Member), DisplayName, STRINGIZE(Member),                         \
        Engine::PropertyType::String, offsetof(SelfType, Member) })

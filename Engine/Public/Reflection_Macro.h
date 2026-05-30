#pragma once

#include "Reflection_Types.h"
#include "RegistorFactory.h"

#define STRINGIZE2(x) #x
#define STRINGIZE(x) STRINGIZE2(x)
#define WIDEN2(x) L##x
#define WIDEN(x) WIDEN2(x)
#define WSTRINGIZE(x) WIDEN(STRINGIZE(x))
#define EXPAND(x) x

#define _REGISTER_GLOBAL_ENUM_IMPL(EnumType, DisplayName, Line) \
    namespace { \
        const bool g_auto_register_global_enum_##Line = []() { \
            std::vector<std::string> names; \
            std::vector<int> values; \
            for (const auto& [val, name] : magic_enum::enum_entries<EnumType>()) { \
                names.push_back(std::string(name)); \
                values.push_back(static_cast<int>(val)); \
            } \
            return Engine::Register_GlobalEnum_Pending(DisplayName, names, values); \
        }(); \
    }

#define REGISTER_GLOBAL_ENUM(EnumType, DisplayName) _REGISTER_GLOBAL_ENUM_IMPL(EnumType, DisplayName, __LINE__)

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


#define GENERATED_ANIM_NOTIFY(ClassName)                                           \
    GENERATED_BASE_TYPE_BODY(ClassName)                                            \
public:                                                                            \
    inline static const bool _auto_register_animnotify_ =                          \
        Engine::Register_AnimNotifyType_Pending(                                   \
            STRINGIZE(ClassName),                                                  \
            []() -> Shared<Engine::AnimNotify>                                     \
            {                                                                      \
                return Engine::Create_AnimNotifyPrototypeInstance<ClassName>();    \
            },                                                                     \
            &ClassName::GetStaticReflectionInfo());

#define GENERATED_ANIM_NOTIFY_STATE(ClassName)                                     \
    GENERATED_BASE_TYPE_BODY(ClassName)                                            \
public:                                                                            \
    inline static const bool _auto_register_animnotifystate_ =                     \
        Engine::Register_AnimNotifyStateType_Pending(                              \
            STRINGIZE(ClassName),                                                  \
            []() -> Shared<Engine::AnimNotifyState>                                \
            {                                                                      \
                return Engine::Create_AnimNotifyStatePrototypeInstance<ClassName>(); \
            },                                                                     \
            &ClassName::GetStaticReflectionInfo());

#define GENERATED_SCREEN_FX(ClassName)                                             \
    GENERATED_BASE_TYPE_BODY(ClassName)                                            \
public:                                                                            \
    inline static const bool _auto_register_screenfx_ =                            \
        Engine::Register_ScreenFxType_Pending(                                     \
            STRINGIZE(ClassName),                                                  \
            []() -> Shared<Engine::ScreenFx_Instance>                              \
            {                                                                      \
                return make_shared<ClassName>();                                   \
            },                                                                     \
            &ClassName::GetStaticReflectionInfo());

#define SELECT_PROPERTY_FLOAT_MACRO(_1, _2, _3, _4, _5, NAME, ...) NAME
#define SELECT_PROPERTY_INT_MACRO(_1, _2, _3, _4, _5, NAME, ...) NAME
#define SELECT_PROPERTY_VEC2_MACRO(_1, _2, _3, NAME, ...) NAME
#define SELECT_PROPERTY_VEC3_MACRO(_1, _2, _3, NAME, ...) NAME
#define SELECT_PROPERTY_VEC4_MACRO(_1, _2, _3, NAME, ...) NAME

#define PROPERTY_FLOAT_CHOOSER(...)                                                \
    EXPAND(SELECT_PROPERTY_FLOAT_MACRO(__VA_ARGS__, PROPERTY_FLOAT_5, PROPERTY_FLOAT_5, PROPERTY_FLOAT_5, PROPERTY_FLOAT_2))

#define PROPERTY_INT_CHOOSER(...)                                                  \
    EXPAND(SELECT_PROPERTY_INT_MACRO(__VA_ARGS__, PROPERTY_INT_5, PROPERTY_INT_5, PROPERTY_INT_5, PROPERTY_INT_2))

#define PROPERTY_VEC2_CHOOSER(...)                                                 \
    EXPAND(SELECT_PROPERTY_VEC2_MACRO(__VA_ARGS__, PROPERTY_VEC2_3, PROPERTY_VEC2_2))

#define PROPERTY_VEC3_CHOOSER(...)                                                 \
    EXPAND(SELECT_PROPERTY_VEC3_MACRO(__VA_ARGS__, PROPERTY_VEC3_3, PROPERTY_VEC3_2))

#define PROPERTY_VEC4_CHOOSER(...)                                                 \
    EXPAND(SELECT_PROPERTY_VEC4_MACRO(__VA_ARGS__, PROPERTY_VEC4_3, PROPERTY_VEC4_2))

#define PROPERTY_FLOAT_2(DisplayName, Member)                                      \
    info.properties.push_back(Engine::PropertyInfo{                                \
        STRINGIZE(Member), DisplayName, STRINGIZE(Member),                         \
        Engine::PropertyType::Float, offsetof(SelfType, Member),                   \
        0.f, 0.f, 0.1f })

#define PROPERTY_FLOAT_5(DisplayName, Member, MinValue, MaxValue, DragSpeed)       \
    info.properties.push_back(Engine::PropertyInfo{                                \
        STRINGIZE(Member), DisplayName, STRINGIZE(Member),                         \
        Engine::PropertyType::Float, offsetof(SelfType, Member),                   \
        MinValue, MaxValue, DragSpeed })

#define PROPERTY_FLOAT(...)                                                         \
    EXPAND(PROPERTY_FLOAT_CHOOSER(__VA_ARGS__)(__VA_ARGS__))

#define PROPERTY_INT_2(DisplayName, Member)                                        \
    info.properties.push_back(Engine::PropertyInfo{                                \
        STRINGIZE(Member), DisplayName, STRINGIZE(Member),                         \
        Engine::PropertyType::Int, offsetof(SelfType, Member),                     \
        0.f, 0.f, 1.f })

#define PROPERTY_INT_5(DisplayName, Member, MinValue, MaxValue, DragSpeed)         \
    info.properties.push_back(Engine::PropertyInfo{                                \
        STRINGIZE(Member), DisplayName, STRINGIZE(Member),                         \
        Engine::PropertyType::Int, offsetof(SelfType, Member),                     \
        MinValue, MaxValue, DragSpeed })

#define PROPERTY_INT(...)                                                           \
    EXPAND(PROPERTY_INT_CHOOSER(__VA_ARGS__)(__VA_ARGS__))

#define PROPERTY_BOOL(DisplayName, Member)                                         \
    info.properties.push_back(Engine::PropertyInfo{                                \
        STRINGIZE(Member), DisplayName, STRINGIZE(Member),                         \
        Engine::PropertyType::Bool, offsetof(SelfType, Member) })

#define PROPERTY_VEC2_2(DisplayName, Member)                                       \
    info.properties.push_back(Engine::PropertyInfo{                                \
        STRINGIZE(Member), DisplayName, STRINGIZE(Member),                         \
        Engine::PropertyType::Vec2, offsetof(SelfType, Member),                    \
        0.f, 0.f, 0.1f })

#define PROPERTY_VEC2_3(DisplayName, Member, DragSpeed)                            \
    info.properties.push_back(Engine::PropertyInfo{                                \
        STRINGIZE(Member), DisplayName, STRINGIZE(Member),                         \
        Engine::PropertyType::Vec2, offsetof(SelfType, Member),                    \
        0.f, 0.f, DragSpeed })

#define PROPERTY_VEC2(...)                                                          \
    EXPAND(PROPERTY_VEC2_CHOOSER(__VA_ARGS__)(__VA_ARGS__))

#define PROPERTY_VEC3_2(DisplayName, Member)                                       \
    info.properties.push_back(Engine::PropertyInfo{                                \
        STRINGIZE(Member), DisplayName, STRINGIZE(Member),                         \
        Engine::PropertyType::Vec3, offsetof(SelfType, Member),                    \
        0.f, 0.f, 0.1f })

#define PROPERTY_VEC3_3(DisplayName, Member, DragSpeed)                            \
    info.properties.push_back(Engine::PropertyInfo{                                \
        STRINGIZE(Member), DisplayName, STRINGIZE(Member),                         \
        Engine::PropertyType::Vec3, offsetof(SelfType, Member),                    \
        0.f, 0.f, DragSpeed })

#define PROPERTY_VEC3(...)                                                          \
    EXPAND(PROPERTY_VEC3_CHOOSER(__VA_ARGS__)(__VA_ARGS__))

#define PROPERTY_VEC4_2(DisplayName, Member)                                       \
    info.properties.push_back(Engine::PropertyInfo{                                \
        STRINGIZE(Member), DisplayName, STRINGIZE(Member),                         \
        Engine::PropertyType::Vec4, offsetof(SelfType, Member),                    \
        0.f, 0.f, 0.1f })

#define PROPERTY_VEC4_3(DisplayName, Member, DragSpeed)                            \
    info.properties.push_back(Engine::PropertyInfo{                                \
        STRINGIZE(Member), DisplayName, STRINGIZE(Member),                         \
        Engine::PropertyType::Vec4, offsetof(SelfType, Member),                    \
        0.f, 0.f, DragSpeed })

#define PROPERTY_VEC4(...)                                                          \
    EXPAND(PROPERTY_VEC4_CHOOSER(__VA_ARGS__)(__VA_ARGS__))

#define PROPERTY_COLOR(DisplayName, Member)                                        \
    info.properties.push_back(Engine::PropertyInfo{                                \
        STRINGIZE(Member), DisplayName, STRINGIZE(Member),                         \
        Engine::PropertyType::Color, offsetof(SelfType, Member),                   \
        0.f, 1.f, 0.01f })

#define PROPERTY_ENUM(DisplayName, Member, EnumType)                               \
    {                                                                              \
        Engine::PropertyInfo prop{};                                               \
        prop.name = STRINGIZE(Member);                                             \
        prop.displayName = DisplayName;                                            \
        prop.jsonKey = STRINGIZE(Member);                                          \
        prop.type = Engine::PropertyType::Enum;                                    \
        prop.offset = offsetof(SelfType, Member);                                  \
        auto entries = magic_enum::enum_entries<EnumType>();                       \
        for (const auto& [enumValue, enumName] : entries)                          \
        {                                                                          \
            prop.enumNames.push_back(string(enumName));                            \
            prop.enumValues.push_back(static_cast<int>(enumValue));                \
        }                                                                          \
        info.properties.push_back(std::move(prop));                                \
    }

#define PROPERTY_STRING(DisplayName, Member)                                       \
    info.properties.push_back(Engine::PropertyInfo{                                \
        STRINGIZE(Member), DisplayName, STRINGIZE(Member),                         \
        Engine::PropertyType::String, offsetof(SelfType, Member) })

#define PROPERTY_WSTRING(DisplayName, Member)                                      \
    info.properties.push_back(Engine::PropertyInfo{                                \
        STRINGIZE(Member), DisplayName, STRINGIZE(Member),                         \
        Engine::PropertyType::WString, offsetof(SelfType, Member) })

#define PROPERTY_BBKEY_VEC3(DisplayName, Member)                                   \
    {                                                                              \
        Engine::PropertyInfo prop{};                                               \
        prop.name = STRINGIZE(Member);                                             \
        prop.displayName = DisplayName;                                            \
        prop.jsonKey = STRINGIZE(Member);                                          \
        prop.type = Engine::PropertyType::String;                                  \
        prop.offset = offsetof(SelfType, Member);                                  \
        prop.isBlackboardKey = true;                                               \
        prop.blackboardValueType = Engine::BlackboardValueType::Vec3;              \
        info.properties.push_back(std::move(prop));                                \
    }

#define PROPERTY_BBKEY_FLOAT(DisplayName, Member)                                  \
    {                                                                              \
        Engine::PropertyInfo prop{};                                               \
        prop.name = STRINGIZE(Member);                                             \
        prop.displayName = DisplayName;                                            \
        prop.jsonKey = STRINGIZE(Member);                                          \
        prop.type = Engine::PropertyType::String;                                  \
        prop.offset = offsetof(SelfType, Member);                                  \
        prop.isBlackboardKey = true;                                               \
        prop.blackboardValueType = Engine::BlackboardValueType::Float;             \
        info.properties.push_back(std::move(prop));                                \
    }

#define PROPERTY_BBKEY_BOOL(DisplayName, Member)                                   \
    {                                                                              \
        Engine::PropertyInfo prop{};                                               \
        prop.name = STRINGIZE(Member);                                             \
        prop.displayName = DisplayName;                                            \
        prop.jsonKey = STRINGIZE(Member);                                          \
        prop.type = Engine::PropertyType::String;                                  \
        prop.offset = offsetof(SelfType, Member);                                  \
        prop.isBlackboardKey = true;                                               \
        prop.blackboardValueType = Engine::BlackboardValueType::Bool;              \
        info.properties.push_back(std::move(prop));                                \
    }

#define PROPERTY_BBKEY_INT(DisplayName, Member)                                    \
    {                                                                              \
        Engine::PropertyInfo prop{};                                               \
        prop.name = STRINGIZE(Member);                                             \
        prop.displayName = DisplayName;                                            \
        prop.jsonKey = STRINGIZE(Member);                                          \
        prop.type = Engine::PropertyType::String;                                  \
        prop.offset = offsetof(SelfType, Member);                                  \
        prop.isBlackboardKey = true;                                               \
        prop.blackboardValueType = Engine::BlackboardValueType::Int;               \
        info.properties.push_back(std::move(prop));                                \
    }

#define GENERATED_SKILLSCRIPT(ClassName)                                      \
public:                                                                       \
    static const char* StaticClassNameA() { return #ClassName; }              \
private:                                                                      \
    inline static const bool _auto_register_skill_script_ =                   \
        Client::SkillScript::Register_SkillScriptType(                        \
            #ClassName,                                                       \
            []() -> Shared<Client::SkillScript>                               \
            {                                                                 \
                return static_pointer_cast<Client::SkillScript>(make_shared<ClassName>()); \
            });

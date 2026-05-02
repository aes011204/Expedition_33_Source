#pragma once

#include "Reflection_Types.h"
#include "RegistorFactory.h"

namespace Engine
{
/* ------------------------------------------ */
/*                캐스팅 매크로                */
/* ------------------------------------------ */

#define ETOI(ENUMCLASS)         static_cast<unsigned int>(ENUMCLASS)

/* ------------------------------------------ */
/*               CRT Debug Heap               */
/* ------------------------------------------ */

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <stdlib.h>

#ifndef DBG_NEW
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#define new DBG_NEW
#endif
#endif

/* ------------------------------------------ */
/*          편의성 메시지 박스 매크로           */
/* ------------------------------------------ */

// 디버그 모드일 때만 메세지 박스 활성화
#if defined(_DEBUG)
#define MSG_BOX(_message)       MessageBox(nullptr, TEXT(_message), L"System Message", MB_OK)
#define MSG_BOX_S(_message)     MessageBox(nullptr, _message, L"System Message", MB_OK)
#else
#define MSG_BOX(_message)       ((void)0)
#define MSG_BOX_S(_message)     ((void)0)
#endif

/* ------------------------------------------ */
/*                 로깅 매크로                 */
/* ------------------------------------------ */

#define LOG_TRACE(...)          SPDLOG_TRACE(__VA_ARGS__)
#define LOG_DEBUG(...)          SPDLOG_DEBUG(__VA_ARGS__)
#define LOG_INFO(...)           SPDLOG_INFO(__VA_ARGS__)
#define LOG_WARN(...)           SPDLOG_WARN(__VA_ARGS__)
#define LOG_ERROR(...)          SPDLOG_ERROR(__VA_ARGS__)
#define LOG_CRITICAL(...)       SPDLOG_CRITICAL(__VA_ARGS__)

/* ------------------------------------------ */
/*           네임스페이스 관련 매크로           */
/* ------------------------------------------ */

// static_assert(true, "") : 더미 코드
#define NS_BEGIN(NAMESPACE)     namespace NAMESPACE { static_assert(true, "");
#define NS_END                  }

#define USING(NAMESPACE)        using namespace NAMESPACE;

// DLL import/export 제어 매크로
#ifdef  ENGINE_EXPORTS
#define ENGINE_DLL              __declspec(dllexport)
#else
#define ENGINE_DLL              __declspec(dllimport)
#endif

/* ------------------------------------------ */
/*              NULL 검사 매크로               */
/* ------------------------------------------ */

// _ptr이 nullptr이면 __VA_ARGS__로 지정한 값을 반환 (비어있으면 void 반환)
#define CHECK_NULL(_ptr, ...)                                                   \
{                                                                               \
    if ((_ptr) == 0) [[unlikely]] {                                             \
        LOG_ERROR("NULL Pointer Detected!");                                    \
        return __VA_ARGS__;                                                     \
    }                                                                           \
}

/* ------------------------------------------ */
/*              경로 검사 매크로               */
/* ------------------------------------------ */

// const char* 경로가 nullptr 이거나 빈 문자열이면 로그를 남기고 반환
#define CHECK_PATH(_path, ...)                                                  \
{                                                                               \
    if ((_path) == nullptr || (_path)[0] == '\0') [[unlikely]] {                \
        LOG_ERROR(                                                              \
            "Invalid path. path: {}",                                           \
            ((_path) != nullptr && (_path)[0] != '\0')                          \
                ? (_path) : "<null>"                                            \
        );                                                                      \
        return __VA_ARGS__;                                                     \
    }                                                                           \
}

// wide 문자열에도 안전하게 사용 가능한 검사 매크로
#define CHECK_WIDEPATH(_path, ...)                                              \
{                                                                               \
    if ((_path) == nullptr || (_path)[0] == L'\0') [[unlikely]] {               \
        LOG_ERROR(                                                              \
            "Invalid path. path: {}",                                           \
            ((_path) != nullptr && (_path)[0] != L'\0')                         \
                ? String::ToString(_path) : "<null>"                            \
        );                                                                      \
        return __VA_ARGS__;                                                     \
    }                                                                           \
}

/* ------------------------------------------ */
/*            HRESULT 검사 매크로              */
/* ------------------------------------------ */

// HRESULT가 실패면 __VA_ARGS__로 지정한 값을 반환 (비어있으면 void 반환)
#define CHECK_FAILED(_hr, ...)                                                  \
{                                                                               \
    if (((HRESULT)(_hr)) < 0) [[unlikely]] {                                    \
        LOG_ERROR("Check Failed!");                                             \
        return __VA_ARGS__;                                                     \
    }                                                                           \
}

// ==================================================
//              싱글톤 매크로
// ==================================================

#define GET_SINGLE(classname) classname::GetInstance()

#define GAME    GET_SINGLE(GameInstance)
#define INPUT	GET_SINGLE(Input_Manager)
#define EVENT	GET_SINGLE(Event_Manager) // 아직 없음

#define NO_COPY(CLASSNAME)											            \
	        private:													        \
	        CLASSNAME(const CLASSNAME&) = delete;						        \
	        CLASSNAME& operator=(const CLASSNAME&) = delete;

#define DECLARE_SINGLETON(CLASSNAME)								            \
	        NO_COPY(CLASSNAME)											        \
	        private:													        \
	        static CLASSNAME* m_pInstance;								        \
	        public:														        \
	        static CLASSNAME* GetInstance();							        \
	        static void DestroyInstance();

#define IMPLEMENT_SINGLETON(CLASSNAME)								            \
	        CLASSNAME* CLASSNAME::m_pInstance = nullptr;				        \
	        CLASSNAME* CLASSNAME::GetInstance() {						        \
	            if(m_pInstance == nullptr) {							        \
	                m_pInstance = new CLASSNAME();						        \
	            }														        \
	            return m_pInstance;										        \
	        }															        \
	        void CLASSNAME::DestroyInstance() {							        \
	            delete m_pInstance;										        \
	            m_pInstance = nullptr;									        \
	        }

// ==================================================
//              리플렉션 매크로
// ==================================================
#define STRINGIZE2(x) #x
#define STRINGIZE(x) STRINGIZE2(x)
#define WIDEN2(x) L##x
#define WIDEN(x) WIDEN2(x)
#define WSTRINGIZE(x) WIDEN(STRINGIZE(x))

#define GENERATED_TYPE_BODY(ClassName, TypeKindValue)                           \
public:                                                                         \
    static const wchar_t* StaticClassName() { return WSTRINGIZE(ClassName); }   \
    static const char* StaticClassNameA() { return STRINGIZE(ClassName); }      \
    using SelfType = ClassName;                                                 \
    static Engine::ClassReflectionInfo& GetStaticReflectionInfo() {             \
        static Engine::ClassReflectionInfo info{                                \
            STRINGIZE(ClassName),                                               \
            STRINGIZE(ClassName),                                               \
            "",                                                                 \
            TypeKindValue,                                                      \
            {}                                                                  \
        };                                                                      \
        return info;                                                            \
    }                                                                           \
    virtual Engine::ClassReflectionInfo& Get_ReflectionInfo() {                 \
        return GetStaticReflectionInfo();                                       \
    }                                                                           \
private:                                                                        \
    bool _name_setter_ = [this]() {                                             \
        this->Set_Name(StaticClassName());                                      \
        return true;                                                            \
    }();

#define GENERATED_COMPONENT(ClassName)                                          \
    GENERATED_TYPE_BODY(ClassName, Engine::RegisteredTypeKind::Component)       \
public:                                                                         \
    inline static const bool _auto_register_component_ =                        \
        Engine::Register_ComponentType_Pending(                                 \
            STRINGIZE(ClassName),                                               \
            [](const ComPtr<Device>& device, const ComPtr<Context>& context) -> Shared<Engine::Component> \
            {                                                                   \
                return Engine::Create_ComponentPrototypeInstance<ClassName>(device, context); \
            },                                                                  \
            &ClassName::GetStaticReflectionInfo());

#define GENERATED_GAMEOBJECT(ClassName, ObjectTypeValue)                        \
    GENERATED_TYPE_BODY(ClassName, Engine::RegisteredTypeKind::GameObject)     \
public:                                                                         \
    static constexpr Engine::ObjectType StaticObjectType() { return ObjectTypeValue; } \
    inline static const bool _auto_register_gameobject_ =                       \
        Engine::Register_GameObjectType_Pending(                                \
            STRINGIZE(ClassName),                                               \
            ObjectTypeValue,                                                    \
            [](const ComPtr<Device>& device, const ComPtr<Context>& context) -> Shared<Engine::GameObject> \
            {                                                                   \
                return Engine::Create_GameObjectPrototypeInstance<ClassName>(device, context); \
            },                                                                  \
            &ClassName::GetStaticReflectionInfo());                             \
private:                                                                        \
    bool _object_type_setter_ = [this]() {                                      \
        _objectType = ObjectTypeValue;                                          \
        return true;                                                            \
    }();

#define GENERATED_BTNODE(ClassName, NodeCategory)                               \
    GENERATED_TYPE_BODY(ClassName, Engine::RegisteredTypeKind::BTNode)         \
public:                                                                         \
    inline static const bool _auto_register_btnode_ =                           \
        Engine::Register_BTNodeType_Pending(                                    \
            STRINGIZE(ClassName),                                               \
            NodeCategory,                                                       \
            []() -> Shared<Engine::Base>                                        \
            {                                                                   \
                return Engine::Create_BTNodePrototypeInstance<ClassName>();     \
            },                                                                  \
            &ClassName::GetStaticReflectionInfo());

};

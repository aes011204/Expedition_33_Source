#pragma once

#include "Reflection_Macro.h"
#include "Helper_LogRateLimit.h"
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
/*                로깅 매크로 EX               */
/* ------------------------------------------ */

#define ENGINE_LOG_UNIQUE_NAME_IMPL(_prefix, _line) _prefix##_line
#define ENGINE_LOG_UNIQUE_NAME(_prefix, _line) ENGINE_LOG_UNIQUE_NAME_IMPL(_prefix, _line)

#define ENGINE_LOG_ONCE_IMPL(_logged, _logMacro, ...)                           \
{                                                                               \
    static Engine::LogRateLimit::OnceState _logged{};                           \
    if (Engine::LogRateLimit::Should_LogOnce(_logged)) [[unlikely]] {           \
        _logMacro(__VA_ARGS__);                                                 \
    }                                                                           \
}

#define ENGINE_LOG_THROTTLED_IMPL(_count, _logMacro, _interval, ...)            \
{                                                                               \
    static Engine::LogRateLimit::ThrottleState _count{};                        \
    if (Engine::LogRateLimit::Should_LogThrottled(                              \
        _count,                                                                 \
        static_cast<uint32>(_interval))) [[unlikely]] {                         \
        _logMacro(__VA_ARGS__);                                                 \
    }                                                                           \
}

// 같은 호출 위치에서 첫 1회만 TRACE 로그를 남긴다.
#define LOG_TRACE_ONCE(...)    ENGINE_LOG_ONCE_IMPL(ENGINE_LOG_UNIQUE_NAME(engineLogOnce, __LINE__), LOG_TRACE, __VA_ARGS__)
// 같은 호출 위치에서 첫 1회만 DEBUG 로그를 남긴다.
#define LOG_DEBUG_ONCE(...)    ENGINE_LOG_ONCE_IMPL(ENGINE_LOG_UNIQUE_NAME(engineLogOnce, __LINE__), LOG_DEBUG, __VA_ARGS__)
// 같은 호출 위치에서 첫 1회만 INFO 로그를 남긴다.
#define LOG_INFO_ONCE(...)     ENGINE_LOG_ONCE_IMPL(ENGINE_LOG_UNIQUE_NAME(engineLogOnce, __LINE__), LOG_INFO, __VA_ARGS__)
// 같은 호출 위치에서 첫 1회만 WARN 로그를 남긴다.
#define LOG_WARN_ONCE(...)     ENGINE_LOG_ONCE_IMPL(ENGINE_LOG_UNIQUE_NAME(engineLogOnce, __LINE__), LOG_WARN, __VA_ARGS__)
// 같은 호출 위치에서 첫 1회만 ERROR 로그를 남긴다.
#define LOG_ERROR_ONCE(...)    ENGINE_LOG_ONCE_IMPL(ENGINE_LOG_UNIQUE_NAME(engineLogOnce, __LINE__), LOG_ERROR, __VA_ARGS__)
// 같은 호출 위치에서 첫 1회만 CRITICAL 로그를 남긴다.
#define LOG_CRITICAL_ONCE(...) ENGINE_LOG_ONCE_IMPL(ENGINE_LOG_UNIQUE_NAME(engineLogOnce, __LINE__), LOG_CRITICAL, __VA_ARGS__)

// 같은 호출 위치에서 첫 1회와 지정 interval 주기마다 TRACE 로그를 남긴다.
#define LOG_TRACE_THROTTLED(_interval, ...) ENGINE_LOG_THROTTLED_IMPL(ENGINE_LOG_UNIQUE_NAME(engineLogThrottle, __LINE__), LOG_TRACE, _interval, __VA_ARGS__)
// 같은 호출 위치에서 첫 1회와 지정 interval 주기마다 DEBUG 로그를 남긴다.
#define LOG_DEBUG_THROTTLED(_interval, ...) ENGINE_LOG_THROTTLED_IMPL(ENGINE_LOG_UNIQUE_NAME(engineLogThrottle, __LINE__), LOG_DEBUG, _interval, __VA_ARGS__)
// 같은 호출 위치에서 첫 1회와 지정 interval 주기마다 INFO 로그를 남긴다.
#define LOG_INFO_THROTTLED(_interval, ...) ENGINE_LOG_THROTTLED_IMPL(ENGINE_LOG_UNIQUE_NAME(engineLogThrottle, __LINE__), LOG_INFO, _interval, __VA_ARGS__)
// 같은 호출 위치에서 첫 1회와 지정 interval 주기마다 WARN 로그를 남긴다.
#define LOG_WARN_THROTTLED(_interval, ...) ENGINE_LOG_THROTTLED_IMPL(ENGINE_LOG_UNIQUE_NAME(engineLogThrottle, __LINE__), LOG_WARN, _interval, __VA_ARGS__)
// 같은 호출 위치에서 첫 1회와 지정 interval 주기마다 ERROR 로그를 남긴다.
#define LOG_ERROR_THROTTLED(_interval, ...) ENGINE_LOG_THROTTLED_IMPL(ENGINE_LOG_UNIQUE_NAME(engineLogThrottle, __LINE__), LOG_ERROR, _interval, __VA_ARGS__)
// 같은 호출 위치에서 첫 1회와 지정 interval 주기마다 CRITICAL 로그를 남긴다.
#define LOG_CRITICAL_THROTTLED(_interval, ...) ENGINE_LOG_THROTTLED_IMPL(ENGINE_LOG_UNIQUE_NAME(engineLogThrottle, __LINE__), LOG_CRITICAL, _interval, __VA_ARGS__)

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
do                                                                              \
{                                                                               \
    if ((_ptr) == 0) [[unlikely]] {                                             \
        LOG_ERROR("NULL Pointer Detected!");                                    \
        return __VA_ARGS__;                                                     \
    }                                                                           \
} while (false)

// _ptr이 nullptr이면 항상 반환하되, generic error 로그는 첫 1회만 남긴다.
#define CHECK_NULL_ONCE(_ptr, ...)                                              \
do                                                                              \
{                                                                               \
    if ((_ptr) == 0) [[unlikely]] {                                             \
        LOG_ERROR_ONCE("NULL Pointer Detected!");                               \
        return __VA_ARGS__;                                                     \
    }                                                                           \
} while (false)

// _ptr이 nullptr이면 항상 반환하되, generic error 로그는 제한 주기로 남긴다.
#define CHECK_NULL_THROTTLED(_ptr, _interval, ...)                              \
do                                                                              \
{                                                                               \
    if ((_ptr) == 0) [[unlikely]] {                                             \
        ENGINE_LOG_THROTTLED_IMPL(                                              \
            ENGINE_LOG_UNIQUE_NAME(engineLogThrottle, __LINE__),                \
            LOG_ERROR,                                                          \
            _interval,                                                          \
            "NULL Pointer Detected!"                                            \
        );                                                                      \
        return __VA_ARGS__;                                                     \
    }                                                                           \
} while (false)

/* ------------------------------------------ */
/*              경로 검사 매크로               */
/* ------------------------------------------ */

// const char* 경로가 nullptr 이거나 빈 문자열이면 로그를 남기고 반환
#define CHECK_PATH(_path, ...)                                                  \
do                                                                              \
{                                                                               \
    if ((_path) == nullptr || (_path)[0] == '\0') [[unlikely]] {                \
        LOG_ERROR(                                                              \
            "Invalid path. path: {}",                                           \
            ((_path) != nullptr && (_path)[0] != '\0')                          \
                ? (_path) : "<null>"                                            \
        );                                                                      \
        return __VA_ARGS__;                                                     \
    }                                                                           \
} while (false)

// wide 문자열에도 안전하게 사용 가능한 검사 매크로
#define CHECK_WIDEPATH(_path, ...)                                              \
do                                                                              \
{                                                                               \
    if ((_path) == nullptr || (_path)[0] == L'\0') [[unlikely]] {               \
        LOG_ERROR(                                                              \
            "Invalid path. path: {}",                                           \
            ((_path) != nullptr && (_path)[0] != L'\0')                         \
                ? String::ToString(_path) : "<null>"                            \
        );                                                                      \
        return __VA_ARGS__;                                                     \
    }                                                                           \
} while (false)

/* ------------------------------------------ */
/*             HRESULT 검사 매크로             */
/* ------------------------------------------ */

// HRESULT가 실패면 __VA_ARGS__로 지정한 값을 반환 (비어있으면 void 반환)
#define CHECK_FAILED(_hr, ...)                                                  \
do                                                                              \
{                                                                               \
    if (((HRESULT)(_hr)) < 0) [[unlikely]] {                                    \
        LOG_ERROR("Check Failed!");                                             \
        return __VA_ARGS__;                                                     \
    }                                                                           \
} while (false)

// HRESULT가 실패면 항상 반환하되, generic error 로그는 첫 1회만 남긴다.
#define CHECK_FAILED_ONCE(_hr, ...)                                             \
do                                                                              \
{                                                                               \
    if (((HRESULT)(_hr)) < 0) [[unlikely]] {                                    \
        LOG_ERROR_ONCE("Check Failed!");                                        \
        return __VA_ARGS__;                                                     \
    }                                                                           \
} while (false)

// HRESULT가 실패면 항상 반환하되, generic error 로그는 제한 주기로 남긴다.
#define CHECK_FAILED_THROTTLED(_hr, _interval, ...)                             \
do                                                                              \
{                                                                               \
    if (((HRESULT)(_hr)) < 0) [[unlikely]] {                                    \
        ENGINE_LOG_THROTTLED_IMPL(                                              \
            ENGINE_LOG_UNIQUE_NAME(engineLogThrottle, __LINE__),                \
            LOG_ERROR,                                                          \
            _interval,                                                          \
            "Check Failed!"                                                     \
        );                                                                      \
        return __VA_ARGS__;                                                     \
    }                                                                           \
} while (false)

// _expr이 true가 아니면 __VA_ARGS__로 지정한 값을 반환 (비어있으면 void 반환)
#define CHECK_TRUE(_expr, ...)                                                  \
do                                                                              \
{                                                                               \
    if (!(_expr)) [[unlikely]] {                                                \
        LOG_ERROR("Check True Failed!");                                        \
        return __VA_ARGS__;                                                     \
    }                                                                           \
} while (false)

// _expr이 true가 아니면 항상 반환하되, generic error 로그는 첫 1회만 남긴다.
#define CHECK_TRUE_ONCE(_expr, ...)                                             \
do                                                                              \
{                                                                               \
    if (!(_expr)) [[unlikely]] {                                                \
        LOG_ERROR_ONCE("Check True Failed!");                                   \
        return __VA_ARGS__;                                                     \
    }                                                                           \
} while (false)

// _expr이 true가 아니면 항상 반환하되, generic error 로그는 제한 주기로 남긴다.
#define CHECK_TRUE_THROTTLED(_expr, _interval, ...)                             \
do                                                                              \
{                                                                               \
    if (!(_expr)) [[unlikely]] {                                                \
        ENGINE_LOG_THROTTLED_IMPL(                                              \
            ENGINE_LOG_UNIQUE_NAME(engineLogThrottle, __LINE__),                \
            LOG_ERROR,                                                          \
            _interval,                                                          \
            "Check True Failed!"                                                \
        );                                                                      \
        return __VA_ARGS__;                                                     \
    }                                                                           \
} while (false)

/* ------------------------------------------ */
/*                싱글톤 매크로                */
/* ------------------------------------------ */

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
            static std::shared_ptr<CLASSNAME> m_pInstance;				        \
            public:														        \
            static std::shared_ptr<CLASSNAME>& GetInstance();			        \
            static void DestroyInstance();

#define IMPLEMENT_SINGLETON(CLASSNAME)                                          \
            std::shared_ptr<CLASSNAME> CLASSNAME::m_pInstance = nullptr;        \
            std::shared_ptr<CLASSNAME>& CLASSNAME::GetInstance()                \
            {                                                                   \
                if (m_pInstance == nullptr)                                     \
                    m_pInstance = std::make_shared<CLASSNAME>();                \
                return m_pInstance;                                             \
            }                                                                   \
            void CLASSNAME::DestroyInstance()                                   \
            {                                                                   \
                if (m_pInstance != nullptr)                                     \
                    m_pInstance->Free();                                        \
                m_pInstance.reset();                                            \
            }

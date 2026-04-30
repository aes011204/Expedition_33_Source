#ifndef Engine_Function_h__
#define Engine_Function_h__

#include "Engine_Typedef.h"
#include <type_traits>
#include <utility>

namespace Engine
{
class Base;

/* raw 포인터가 COM처럼 AddRef를 제공하는지 컴파일 타임에 확인하기 위한 traits. */
template <typename T, typename = void>
struct HasAddRef : std::false_type {};

/* Safe_AddRef가 COM 포인터만 AddRef를 호출하고 엔진 객체 raw 포인터는 건너뛰도록 구분한다. */
template <typename T>
struct HasAddRef<T, std::void_t<decltype(std::declval<T*>()->AddRef())>> : std::true_type {};

/* raw 포인터가 COM처럼 Release를 제공하는지 컴파일 타임에 확인하기 위한 traits. */
template <typename T, typename = void>
struct HasRelease : std::false_type {};

/* Safe_Release가 COM 포인터만 Release를 호출하고 스마트포인터 소유 객체는 해제하지 않도록 구분한다. */
template <typename T>
struct HasRelease<T, std::void_t<decltype(std::declval<T*>()->Release())>> : std::true_type {};

template <typename T>
void Safe_Delete(T& Pointer)
{
    if (nullptr != Pointer)
    {
        delete Pointer;
        Pointer = nullptr;
    }
}

template <typename T>
void Safe_Delete_Array(T& Pointer)
{
    if (nullptr != Pointer)
    {
        delete [] Pointer;
        Pointer = nullptr;
    }
}

template <typename T>
unsigned int Safe_AddRef(Shared<T>& pInstance)
{
    return nullptr != pInstance ? 1u : 0u;
}

template <typename T>
unsigned int Safe_AddRef(T*& pInstance)
{
    unsigned int iRefCnt = {};
    if (nullptr != pInstance)
    {
        if constexpr (HasAddRef<T>::value)
            iRefCnt = static_cast<unsigned int>(pInstance->AddRef());
        else
            iRefCnt = 1u;
    }
    return iRefCnt;
}

template <typename T>
unsigned int Safe_Release(Shared<T>& pInstance)
{
    pInstance.reset();
    return 0;
}

template <typename T>
unsigned int Safe_Release(T*& pInstance)
{
    unsigned int iRefCnt = {};
    if (nullptr != pInstance)
    {
        if constexpr (HasRelease<T>::value)
        {
            iRefCnt = static_cast<unsigned int>(pInstance->Release());
            if (0 == iRefCnt)
                pInstance = nullptr;
        }
        else
        {
            iRefCnt = 0u;
            pInstance = nullptr;
        }
    }
    return iRefCnt;
}
}

/* ------------------------------------------ */
/*               전역 헬퍼 헤더                */
/* ------------------------------------------ */

#include "Helper_String.h"   // 문자열 처리 (string <-> wstring 변환 등)
#include "Helper_Math.h"     // 수학 상수 및 함수

#endif // Engine_Function_h__

#pragma once

#include "Engine_Define.h"


// 일단 모든 enum의 비트마스크 연산을 막는다. 
template <typename Enum>
struct Enable_Bitmask_Operators
{
    static constexpr bool value = false;
};

template <typename Enum>
constexpr bool Enable_Bitmask_Operators_v = Enable_Bitmask_Operators<Enum>::value;

// 어떤 타입의 enum이든 정수로 바꿔줄 수 있게 한다
template <typename Enum>
using Enum_Underlying_t = std::underlying_type_t<Enum>;

template <typename Enum>
constexpr Enum_Underlying_t<Enum> To_Underlying(Enum value)
{
    return static_cast<Enum_Underlying_t<Enum>>(value);
}

template <typename Enum>
constexpr std::enable_if_t<Enable_Bitmask_Operators_v<Enum>, Enum>
operator|(Enum lhs, Enum rhs)
{
    return static_cast<Enum>(To_Underlying(lhs) | To_Underlying(rhs));
}

template <typename Enum>
constexpr std::enable_if_t<Enable_Bitmask_Operators_v<Enum>, Enum>
operator&(Enum lhs, Enum rhs)
{
    return static_cast<Enum>(To_Underlying(lhs) & To_Underlying(rhs));
}

template <typename Enum>
constexpr std::enable_if_t<Enable_Bitmask_Operators_v<Enum>, Enum>
operator^(Enum lhs, Enum rhs)
{
    return static_cast<Enum>(To_Underlying(lhs) ^ To_Underlying(rhs));
}

template <typename Enum>
constexpr std::enable_if_t<Enable_Bitmask_Operators_v<Enum>, Enum>
operator~(Enum value)
{
    return static_cast<Enum>(~To_Underlying(value));
}

template <typename Enum>
constexpr std::enable_if_t<Enable_Bitmask_Operators_v<Enum>, Enum&>
operator|=(Enum& lhs, Enum rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

template <typename Enum>
constexpr std::enable_if_t<Enable_Bitmask_Operators_v<Enum>, Enum&>
operator&=(Enum& lhs, Enum rhs)
{
    lhs = lhs & rhs;
    return lhs;
}

template <typename Enum>
constexpr std::enable_if_t<Enable_Bitmask_Operators_v<Enum>, Enum&>
operator^=(Enum& lhs, Enum rhs)
{
    lhs = lhs ^ rhs;
    return lhs;
}

template <typename Enum>
constexpr std::enable_if_t<Enable_Bitmask_Operators_v<Enum>, bool>
Has_Flag(Enum value, Enum flag)
{
    return (To_Underlying(value) & To_Underlying(flag)) != 0;
}

template <typename Enum>
constexpr std::enable_if_t<Enable_Bitmask_Operators_v<Enum>, bool>
Has_All_Flags(Enum value, Enum flags)
{
    return (To_Underlying(value) & To_Underlying(flags)) == To_Underlying(flags);
}

template <typename Enum>
constexpr std::enable_if_t<Enable_Bitmask_Operators_v<Enum>, void>
Add_Flag(Enum& value, Enum flag)
{
    value |= flag;
}

template <typename Enum>
constexpr std::enable_if_t<Enable_Bitmask_Operators_v<Enum>, void>
Remove_Flag(Enum& value, Enum flag)
{
    value &= ~flag;
}

template <typename Enum>
constexpr std::enable_if_t<Enable_Bitmask_Operators_v<Enum>, void>
Toggle_Flag(Enum& value, Enum flag)
{
    value ^= flag;
}

template <typename Enum>
constexpr std::enable_if_t<Enable_Bitmask_Operators_v<Enum>, void>
Set_Flag(Enum& value, Enum flag, bool enable)
{
    if (enable)
        Add_Flag(value, flag);
    else
        Remove_Flag(value, flag);
}

template <typename Enum>
constexpr std::enable_if_t<Enable_Bitmask_Operators_v<Enum>, bool>
Is_Empty_Flag(Enum value)
{
    return To_Underlying(value) == 0;
}

template <typename Enum>
constexpr std::enable_if_t<Enable_Bitmask_Operators_v<Enum>, void>
Clear_Flag(Enum& value)
{
    value = static_cast<Enum>(0);
}

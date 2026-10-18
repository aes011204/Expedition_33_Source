#pragma once

#include "Base.h"
#include <format>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

NS_BEGIN(Engine)

struct PrintDurationOption
{
    float value = 2.f;
};

struct PrintColorOption
{
    Vec4 value = Vec4(0.f, 0.f, 0.f, 1.f);
};

inline PrintDurationOption duration(float seconds)
{
    return PrintDurationOption{ seconds };
}

inline PrintColorOption color(const Vec4& value)
{
    return PrintColorOption{ value };
}

class ENGINE_DLL Print_Manager final : public Base
{
public:
    Print_Manager() = default;
    ~Print_Manager() override = default;

public:
    void Initialize();
    void Update(float timeDelta);
    void Render();

    void Clear();
    void Clear(const wstring& slot);

    template<typename... Args>
    void Print(std::wstring_view format, Args&&... args)
    {
        PrintResolved resolved{};
        resolved.slot = wstring(format);
        resolved.message = wstring(format);

        auto argsTuple = std::forward_as_tuple(std::forward<Args>(args)...);

        constexpr size_t optionCount = Count_TrailingPrintOptions<std::remove_cvref_t<Args>...>();
        constexpr size_t formatArgCount = sizeof...(Args) - optionCount;

        if constexpr (formatArgCount > 0)
        {
            resolved.message = Format_PrintMessage(
                format,
                argsTuple,
                std::make_index_sequence<formatArgCount>{});
        }

        Apply_PrintOptions<formatArgCount>(argsTuple, resolved);
        resolved.persistent = (resolved.duration <= 0.f);

        Push_PrintRecord(
            resolved.slot,
            resolved.message,
            resolved.duration,
            resolved.color,
            resolved.persistent);
    }

private:
    struct RuntimePrint
    {
        wstring slot{};
        wstring text{};
        float remainingTime = 0.f;
        Vec4 color = Vec4(0.f, 0.f, 0.f, 1.f);
        bool persistent = false;
    };

    struct PrintResolved
    {
        wstring slot{};
        wstring message{};
        float duration = 2.f;
        Vec4 color = Vec4(0.f, 0.f, 0.f, 1.f);
        bool persistent = false;
    };

private:
    template<typename T>
    static constexpr bool Is_PrintDurationOption()
    {
        return std::is_same_v<std::remove_cvref_t<T>, PrintDurationOption>;
    }

    template<typename T>
    static constexpr bool Is_PrintColorOption()
    {
        return std::is_same_v<std::remove_cvref_t<T>, PrintColorOption>;
    }

    template<typename T>
    static constexpr bool Is_PrintOption()
    {
        return Is_PrintDurationOption<T>() || Is_PrintColorOption<T>();
    }

    template<typename... Ts>
    static consteval size_t Count_TrailingPrintOptions()
    {
        constexpr bool flags[] = { Is_PrintOption<Ts>()... };

        size_t count = 0;
        for (size_t i = sizeof...(Ts); i > 0; --i)
        {
            if (flags[i - 1])
                ++count;
            else
                break;
        }

        return count;
    }

    template<typename Tuple, size_t... Is>
    static wstring Format_PrintMessage(
        std::wstring_view format,
        Tuple&& argsTuple,
        std::index_sequence<Is...>)
    {
        try
        {
            return std::vformat(
                format,
                std::make_wformat_args(std::get<Is>(std::forward<Tuple>(argsTuple))...));
        }
        catch (...)
        {
            return wstring(format);
        }
    }

    static void Apply_OnePrintOption(const PrintDurationOption& option, PrintResolved& resolved)
    {
        resolved.duration = option.value;
    }

    static void Apply_OnePrintOption(const PrintColorOption& option, PrintResolved& resolved)
    {
        resolved.color = option.value;
    }

    template<typename Tuple, size_t FormatArgCount, size_t... Is>
    static void Apply_PrintOptions_Impl(
        Tuple&& argsTuple,
        PrintResolved& resolved,
        std::index_sequence<Is...>)
    {
        (Apply_OnePrintOption(std::get<FormatArgCount + Is>(std::forward<Tuple>(argsTuple)), resolved), ...);
    }

    template<size_t FormatArgCount, typename Tuple>
    static void Apply_PrintOptions(Tuple&& argsTuple, PrintResolved& resolved)
    {
        constexpr size_t totalCount = std::tuple_size_v<std::remove_reference_t<Tuple>>;
        constexpr size_t optionCount = totalCount - FormatArgCount;

        if constexpr (optionCount > 0)
        {
            Apply_PrintOptions_Impl<Tuple, FormatArgCount>(
                std::forward<Tuple>(argsTuple),
                resolved,
                std::make_index_sequence<optionCount>{});
        }
    }

    void Push_PrintRecord(
        const wstring& slot,
        const wstring& message,
        float duration,
        const Vec4& color,
        bool persistent);

private:
    list<RuntimePrint> _messages{};

    wstring _fontTag = L"Font_Default";
    Vec2    _startPosition = Vec2(20.f, 20.f); // 시작 위치
    float   _lineHeight = 22.f;                     // 메세지 간격
    size_t  _maxTransientMessageCount = 8;          // 메세지 개수

public:
    static Unique<Print_Manager> Create();
};

NS_END

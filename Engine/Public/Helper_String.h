#pragma once

/*
 * Helper_String.h
 * - 문자열 변환, 전처리(공백 제거 등)에 사용하는 공용 헬퍼들을 정의한다.
 */

#include "Engine_Typedef.h"

#include <algorithm>
#include <cwctype>
#include <string>
#include <type_traits>

namespace Engine::String
{
using namespace std;

// UTF-8 기반 string을 엔진/UI에서 사용하는 wide string으로 변환한다
inline wstring ToWString(const string& text)
{
    // 필요 버퍼 크기
    const int requiredSize = MultiByteToWideChar(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(text.size()),
        nullptr,
        0
    );

    if (requiredSize <= 0)
        return {};

    wstring outText;
    outText.resize(static_cast<size_t>(requiredSize));

    // 실제 반환
    const int convertedSize = MultiByteToWideChar(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(text.size()),
        outText.data(),
        requiredSize
    );

    if (convertedSize <= 0)
        return {};

    return outText;
}

// wide string을 ImGui/json/log에서 쓰기 좋은 UTF-8 string으로 변환한다.
inline string ToString(const wstring& text)
{
    const int requiredSize = WideCharToMultiByte(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(text.size()),
        nullptr,
        0,
        nullptr,
        nullptr
    );

    if (requiredSize <= 0)
        return {};

    string outText;
    outText.resize(static_cast<size_t>(requiredSize));

    const int convertedSize = WideCharToMultiByte(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(text.size()),
        outText.data(),
        requiredSize,
        nullptr,
        nullptr
    );

    if (convertedSize <= 0)
        return {};

    return outText;
}

// const char*를 std::string으로 변환한다.
inline string CharToStr(const char* text)
{
    return nullptr != text ? string{ text } : string{};
}

// const char* UTF-8 문자열을 std::wstring으로 변환한다.
inline wstring CharToWString(const char* text)
{
    return ToWString(CharToStr(text));
}

// const wchar_t*를 std::wstring으로 변환한다.
inline wstring TCharToWString(const wchar_t* text)
{
    return nullptr != text ? wstring{ text } : wstring{};
}

// const wchar_t*를 UTF-8 std::string으로 변환한다.
inline string TCharToString(const wchar_t* text)
{
    return ToString(TCharToWString(text));
}

/**
 * @brief ASCII 공백 문자 여부를 판별한다.
 *
 * @param value 검사할 문자
 * @return 공백이면 true, 아니면 false
 */
template <typename TChar>
bool Is_AsciiWhitespace(TChar value)
{
    if constexpr (std::is_same_v<TChar, char>)
        return 0 != std::isspace(static_cast<unsigned char>(value));
    else if constexpr (std::is_same_v<TChar, wchar_t>)
        return 0 != std::iswspace(static_cast<wint_t>(value));
    else
    {
        static_assert(
            std::is_same_v<TChar, char> || std::is_same_v<TChar, wchar_t>,
            "Trim_AsciiWhitespace supports only char and wchar_t."
        );
        return false;
    }
}

/**
 * @brief 문자열 양끝의 ASCII 공백을 제거한 새 문자열을 반환한다.
 *
 * @param source 원본 문자열
 * @return 앞뒤 공백이 제거된 문자열. 모두 공백이면 빈 문자열 반환
 */
template <typename TChar, typename TTraits = std::char_traits<TChar>, typename TAlloc = std::allocator<TChar>>
std::basic_string<TChar, TTraits, TAlloc> Trim_AsciiWhitespace(
    const std::basic_string<TChar, TTraits, TAlloc>& source)
{
    const auto iterBegin = std::find_if_not(
        source.begin(),
        source.end(),
        [](const TChar value)
        {
            return Is_AsciiWhitespace(value);
        }
    );

    if (iterBegin == source.end())
        return {};

    const auto iterReverseEnd = std::find_if_not(
        source.rbegin(),
        source.rend(),
        [](const TChar value)
        {
            return Is_AsciiWhitespace(value);
        }
    );
    const auto iterEnd = iterReverseEnd.base();

    return std::basic_string<TChar, TTraits, TAlloc>(iterBegin, iterEnd);
}

inline string ToLowerCopy(string value)
{
    std::ranges::transform(
        value,
        value.begin(),
        [](unsigned char value)
        {
            return static_cast<char>(std::tolower(value));
        }
    );
    return value;
}

inline string Base64_Encode(const vector<uint8_t>& data)
{
    static const char base64_chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    string ret;
    int i = 0;
    int j = 0;
    uint8_t char_array_3[3];
    uint8_t char_array_4[4];

    for (uint8_t const& val : data)
    {
        char_array_3[i++] = val;
        if (i == 3)
        {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; (i < 4); i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i)
    {
        for (j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (j = 0; (j < i + 1); j++)
            ret += base64_chars[char_array_4[j]];

        while ((i++ < 3))
            ret += '=';
    }

    return ret;
}

inline vector<uint8_t> Base64_Decode(const string& encoded_string)
{
    static const string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    auto is_base64 = [](uint8_t c) -> bool {
        return (isalnum(c) || (c == '+') || (c == '/'));
    };

    size_t in_len = encoded_string.size();
    int i = 0;
    int j = 0;
    int in_ = 0;
    uint8_t char_array_4[4], char_array_3[3];
    vector<uint8_t> ret;

    while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_]))
    {
        char_array_4[i++] = encoded_string[in_]; in_++;
        if (i == 4)
        {
            for (i = 0; i < 4; i++)
                char_array_4[i] = static_cast<uint8_t>(base64_chars.find(char_array_4[i]));

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; (i < 3); i++)
                ret.push_back(char_array_3[i]);
            i = 0;
        }
    }

    if (i)
    {
        for (j = i; j < 4; j++)
            char_array_4[j] = 0;

        for (j = 0; j < 4; j++)
            char_array_4[j] = static_cast<uint8_t>(base64_chars.find(char_array_4[j]));

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (j = 0; (j < i - 1); j++) ret.push_back(char_array_3[j]);
    }

    return ret;
}
}

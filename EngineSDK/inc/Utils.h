#pragma once

#include "Engine_Define.h"

NS_BEGIN(Engine)

class Utils
{
public:
    /* UTF-8 기반 string을 엔진/UI에서 사용하는 wide string으로 변환한다. */
    static inline wstring ToWString(const string& value)
    {
        if (value.empty())
            return {};

        int convertedSize = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
        uint32 codePage = CP_UTF8;

        if (convertedSize <= 0)
        {
            codePage = CP_ACP;
            convertedSize = MultiByteToWideChar(codePage, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
        }

        if (convertedSize <= 0)
            return {};

        wstring result(static_cast<size_t>(convertedSize), L'\0');
        MultiByteToWideChar(codePage, 0, value.c_str(), static_cast<int>(value.size()), result.data(), convertedSize);

        return result;
    }

    /* wide string을 ImGui/json/log에서 쓰기 좋은 UTF-8 string으로 변환한다. */
    static inline string ToString(const wstring& value)
    {
        if (value.empty())
            return {};

        const int convertedSize = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        if (convertedSize <= 0)
            return {};

        string result(static_cast<size_t>(convertedSize), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(), convertedSize, nullptr, nullptr);

        return result;
    }
};

NS_END

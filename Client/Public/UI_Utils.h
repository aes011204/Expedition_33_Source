#pragma once

#include "UI_Base.h"

namespace Client::UI_Utils
{
    inline Shared<UI_Base> Find_UI(
        const umap<wstring, Shared<UI_Base>>& uiByName, const wstring& name)
    {
        auto iter = uiByName.find(name);
        if (iter == uiByName.end())
            return nullptr;

        return iter->second;
    }

    template<typename T>
    Shared<T> Find_TypedUI(
        const umap<wstring, Shared<Engine::UI_Base>>& uiByName,
        const wstring& name,
        const char* ownerTag = "UI")
    {
        Shared<Engine::UI_Base> ui = Find_UI(uiByName, name);
        if (ui == nullptr)
            return nullptr;

        Shared<T> typedUI = dynamic_pointer_cast<T>(ui);
        if (typedUI == nullptr)
            LOG_WARN("{} ui type mismatch: {}", ownerTag, String::ToString(name));

        return typedUI;
    }

}


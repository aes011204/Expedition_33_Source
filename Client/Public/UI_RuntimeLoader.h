#pragma once

#include "Client_Defines.h"

NS_BEGIN(Engine)
class UI_Base;
NS_END

NS_BEGIN(Client)

// UI_Editor가 저장한 json 문서를 런타임 UI로 생성하는 전용 로더.
// Image/Text/Panel/Button을 생성하고 parentId를 런타임 부모 관계로 연결.
class UI_RuntimeLoader
{
public:
    // 외부 (Level_GamePlay 등)에서 로더에 넘기는 정보
    struct UI_LoadDesc
    {
        uint32 levelIndex = 0;
        wstring jsonPath{};
        wstring animationDirectory{};
        unordered_map<wstring, Shared<UI_Base>>* outUIByName = nullptr;
    };

public:
    UI_RuntimeLoader() = delete;

public:
    // json 파일을 읽고, 현재 런타임에서 지원하는 UI를 실제로 생성
    static HRESULT Load_Document(const UI_LoadDesc& desc);

};

NS_END

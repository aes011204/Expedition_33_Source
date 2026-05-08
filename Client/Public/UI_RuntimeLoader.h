#pragma once

NS_BEGIN(Client)

// UI_Editor가 저장한 json 문서를 런타임 UI로 생성하는 전용 로더.
// 현재는 Image/Text만 실제 생성하고,
// Panel/Button/parentId/canvas는 이후 확장을 고려해 파싱 구조를 미리 분리만. 아직 구현 안했음
class UI_RuntimeLoader
{
public:
    // 외부 (Level_GamePlay 등)에서 로더에 넘기는 정보
    struct UI_LoadDesc
    {
        uint32 levelIndex = 0;
        wstring jsonPath{};
    };

public:
    UI_RuntimeLoader() = delete;

public:
    // json 파일을 읽고, 현재 런타임에서 지원하는 UI를 실제로 생성
    static HRESULT Load_Document(const UI_LoadDesc& desc);

};

NS_END

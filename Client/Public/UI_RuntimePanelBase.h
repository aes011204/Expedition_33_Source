#pragma once

#include "Base.h"
#include "IRuntimeUI_EditorTarget.h"
#include "Reflection_Types.h"

NS_BEGIN(Engine)
class UI_Base;
NS_END

NS_BEGIN(Client)

class UI_RuntimePanelBase abstract : public Base, public IRuntimeUI_EditorTarget
{
public:
    UI_RuntimePanelBase() = default;
    ~UI_RuntimePanelBase() override = default;

public:
    virtual ClassReflectionInfo& Get_ReflectionInfo()
    {
        static ClassReflectionInfo empty{};
        return empty;
    }

public:
    virtual HRESULT Initialize(uint32 levelIndex) = 0;
    virtual void    Update(float timeDelta) = 0;
    virtual void    Apply_EditorReflectionRuntimeTuning() {}

public:
    virtual void Open();
    virtual void Close();
    virtual void Toggle();

    bool Is_Open() const { return _isOpen; }
    Shared<UI_Base> Get_RootUI() const { return _root; }

public: // IRuntimeUIEditorTarget
    Shared<UI_Base> Get_EditorRootUI() const override { return _root; }
    bool Is_EditorPositionDrivenByCode() const override { return _editorPositionDrivenByCode; }

    Vec2 Get_EditorDefaultTestOffset() const override { return _editorDefaultTestOffset; }
    void Set_EditorDefaultTestOffset(const Vec2& offset) override;

    void Apply_EditorDefaultOffsetToRoot() override;

protected:
    void Capture_EditorDesignPositionIfNeeded();
    void Reset_EditorDesignPositionCache(); // 루트 UI가 바뀌면 이전 루트 위치 캐시를 버린다.

    void Apply_EditorOffsets(Vec2& inOutPosition) const;
    void Set_EditorPositionDrivenByCode(bool enabled) { _editorPositionDrivenByCode = enabled; }

protected:
    uint32 _levelIndex = 0;
    bool   _isOpen = false;

    umap<wstring, Shared<UI_Base>> _uiByName{};
    Shared<UI_Base> _root{};

private:
    Vec2 _editorDefaultTestOffset = Vec2::Zero; // 모든 UI 공통 기본 테스트 오프셋
    Vec2 _editorDesignPosition = Vec2::Zero;    // 일반 UI의 원래 루트 위치를 세션 중 기억
    bool _hasEditorDesignPosition = false;
    bool _editorPositionDrivenByCode = false;

public:
    void Free() override;
};

NS_END

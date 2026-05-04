#pragma once
#include "Base.h"

NS_BEGIN(Engine)
class RenderTarget;
NS_END

NS_BEGIN(EffectEditor)

class Editor_Window;
class EffectEditor_Manager;
class ImGui_Manager;

class EffectEditorInstance final : public Base
{
    DECLARE_SINGLETON(EffectEditorInstance)

public:
    EffectEditorInstance();
    ~EffectEditorInstance() override;

public:
    HRESULT Initialize_Editor(
        const EffectEditorDesc& editorDesc,
        const ComPtr<Device>& device,
        const ComPtr<Context>& context);
    void Update_Editor(float timeDelta);
    void Render_Editor();

public: //## Accessors
    Shared<Editor_Window> Get_Window(const wstring& windowName) const;
    HWND Get_WindowHandle() const;

public: //## Viewport Integration
    Shared<RenderTarget> Prepare_SharedViewportRenderTarget(uint32 width, uint32 height);
    void Request_ViewportSize(EditorViewportTarget targetType, uint32 width, uint32 height);
    ID3D11ShaderResourceView* Get_SharedViewportSRV() const;
    void Capture_BackBuffer_ToViewport();

public: //## Runtime Control
    void Play();
    void Pause();
    void Stop();
    void Resume();
    bool IsPlaying() const;
    bool IsPaused() const;
    void Set_RuntimeTimeScale(float timeScale);
    void Reset_RuntimeTimeScale();
    float Get_RuntimeTimeScale() const;

private: //## Data::Core
    ComPtr<Device> _device{};
    ComPtr<Context> _context{};
    EffectEditorDesc _editorDesc{};

    Unique<EffectEditor_Manager> _editorManager{};
    Unique<ImGui_Manager> _imguiManager{};

    Shared<RenderTarget> _sharedViewportRenderTarget{};

private: //## Data::Runtime
    EditorViewportTarget _lastViewportRequester{ EditorViewportTarget::Game };
    EffectEditorRuntimeState _runtimeState{ EffectEditorRuntimeState::Edit };

    // 에디터에서 뷰포트 창 사이즈 변경 시
    uint32 _requestedViewportWidth{ 1 };
    uint32 _requestedViewportHeight{ 1 };

    float _runtimeTimeScale{ 1.f }; // 런타임 배율

private:
    static float Clamp_RuntimeTimeScale(float timeScale);

public:
    void Free() override;
};

NS_END

#define EDITOR GET_SINGLE(EffectEditorInstance)

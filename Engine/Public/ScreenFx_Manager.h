#pragma once

#include "Shader_CBuffer_Define.h"
#include "ScreenFx_Instance.h"

NS_BEGIN(Engine)

class ShaderCom;
class VIBuffer_Rect;

struct ScreenFxHandle
{
    uint64 id{ 0 };

    bool Is_Valid() const { return id != 0; }
};

class ENGINE_DLL ScreenFx_Manager final : Base
{
public:
    ScreenFx_Manager() = default;
    ~ScreenFx_Manager() override;

public:
    bool Is_ScreenFXActivated() const { return _activeFx; }
    void Set_ScreenFxActivated(bool active) { _activeFx = active; }
    void Set_RenderPreviewEffectsOnly(bool previewOnly) { _renderPreviewEffectsOnly = previewOnly; }

    bool Is_CaptureRequested() const { return _captureRequested; }
    bool Is_SaveRequested() const { return _saveRequested; }
    void Clear_CaptureRequested() { _captureRequested = false; _saveRequested = false; }

#ifdef _DEBUG
    bool Has_ActiveEffects() const { return _renderPreviewEffectsOnly ? !_previewEffects.empty() : (!_activeEffects.empty() || _previewEffect != nullptr); }
#else
    bool Has_ActiveEffects() const { return _renderPreviewEffectsOnly ? !_previewEffects.empty() : !_activeEffects.empty(); }
#endif

    // Plays a screen effect without exposing lifetime control to the caller.
    void Play_ScreenEffect(const Shared<ScreenFx_Instance>& effect);
    // Plays a screen effect and returns a handle that can stop only that active instance.
    ScreenFxHandle Play_ScreenEffect_WithHandle(const Shared<ScreenFx_Instance>& effect);
    // Stops a single active screen effect; stale or empty handles are ignored.
    void Stop_ScreenEffect(ScreenFxHandle handle);
    ScreenFxHandle Play_PreviewScreenEffect_WithHandle(const Shared<ScreenFx_Instance>& effect);
    // Evaluates a preview-only screen effect at an explicit timeline time.
    void Evaluate_PreviewScreenEffect(ScreenFxHandle handle, float elapsedTime);
    void Stop_PreviewScreenEffect(ScreenFxHandle handle);
    void Clear_ScreenEffect();
    bool Is_ScreenEffectFinished(const wstring& tag) const;

    // Screen Capture
    void Request_ScreenCapture(bool saveToFile = true);
    HRESULT Save_ScreenCapture(const wstring& filePath);

public:
    void Update(float timeDelta);
    HRESULT Render_ScreenFx(const Shared<ShaderCom>& shader, const Shared <VIBuffer_Rect>& rtBuffer);

private:
    bool _activeFx{ true };
    bool _renderPreviewEffectsOnly{ false };
    bool _captureRequested{ false };
    bool _saveRequested{ false };

private:
    struct ActiveEffectEntry
    {
        ScreenFxHandle handle{};
        Shared<ScreenFx_Instance> effect{};
    };

    vector<ActiveEffectEntry> _activeEffects;
    vector<ActiveEffectEntry> _previewEffects;
    unordered_set<wstring> _finishedEffectTags;
    uint64 _nextEffectHandleId{ 1 };

public:
#ifdef _DEBUG
    void Set_PreviewEffect(const Shared<ScreenFx_Instance>& effect) { _previewEffect = effect; }
    void Clear_PreviewEffect() { _previewEffect = nullptr; }
    Shared<ScreenFx_Instance> _previewEffect{};
#endif

public:
    static Unique<ScreenFx_Manager> Create();
};

NS_END

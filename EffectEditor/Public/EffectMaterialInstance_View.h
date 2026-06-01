#pragma once
#include "Editor_Window.h"

#include "DetailPropertyContext.h"
#include "EffectAuthoring_Types.h"
#include "Emitter_View.h"

NS_BEGIN(Engine)
class Texture;
NS_END

NS_BEGIN(EffectEditor)

class EffectMaterial_View;
class EffectMaterialPreviewRenderer;

enum class EffectMaterialTwoSidedDisplayMode : uint8
{
    EditableImplemented,
    EditableUnimplemented,
    ForcedEnabledReadOnly,
};

class EffectMaterialInstance_View final : public Editor_Window
{
public:
    EffectMaterialInstance_View();
    ~EffectMaterialInstance_View() override;

public:
    void Update(float timeDelta) override; // material instance preview UV motion 시간을 갱신한다.
    void Pre_Render() override;
    void Render() override;
    static bool Draw_InstanceProperties(
        DetailPropertyContext& detailPropertyContext,
        map<string, Shared<Texture>>& textureThumbnailCache,
        EffectMaterialInstanceData& material,
        bool* outRestartPreview,
        EffectMaterialTwoSidedDisplayMode twoSidedDisplayMode = EffectMaterialTwoSidedDisplayMode::EditableImplemented,
        bool disableMeshDistortionShapeControls = false); // MeshData에서 지원하지 않는 distortion shape 편집을 막는다.

public: //## Behavior::Selection
    void Open_Instance(uint32 emitterId, uint32 moduleId);

private: //## Data
    uint32 _targetEmitterId{ 0 };
    uint32 _targetModuleId{ 0 };
    DetailPropertyContext _detailPropertyContext{};
    Unique<EffectMaterialPreviewRenderer> _previewRenderer{};
    map<string, Shared<Texture>> _textureThumbnailCache{};

private: //## Helpers
    bool Resolve_Target(Shared<Emitter_View>& outEmitterView, AuthoringEmitter*& outEmitter, RequiredModuleData*& outRequiredData) const;
    void Apply_MaterialChanged(const Shared<Emitter_View>& emitterView, AuthoringEmitter& emitter, const RequiredModuleData& requiredData);
    void Begin_PendingAuthoringEdit(
        const Shared<Emitter_View>& emitterView,
        const EffectAuthoringSelection& selection,
        const Emitter_View::AuthoringSnapshot& beforeSnapshot,
        const string& description);
    void Commit_PendingAuthoringEditIfIdle(const Shared<Emitter_View>& emitterView);
    void Commit_PendingAuthoringEdit(const Shared<Emitter_View>& emitterView);

private: //## Data::AuthoringHistory
    bool _hasPendingAuthoringEdit{ false };
    EffectAuthoringSelection _pendingAuthoringSelection{};
    Emitter_View::AuthoringSnapshot _pendingAuthoringSnapshot{};
    string _pendingAuthoringDescription{};

public:
    static Shared<EffectMaterialInstance_View> Create();
};

NS_END

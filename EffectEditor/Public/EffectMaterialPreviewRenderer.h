#pragma once

#include "Base.h"
#include "EffectMaterialAuthoring_Types.h"
#include "EffectPreviewOrbitCamera.h"

NS_BEGIN(Engine)
class RenderTarget;
class ShaderCom;
class Texture;
NS_END

NS_BEGIN(EffectEditor)

enum class EffectMaterialPreviewMeshMode : uint8
{
    Plane,
    Sphere,
};

class EffectMaterialPreviewRenderer final : public Base
{
public:
    explicit EffectMaterialPreviewRenderer(uint32 previewTextureSize = 512);
    ~EffectMaterialPreviewRenderer() override;

public:
    // 머티리얼 instance 값을 현재 preview render target에 반영할 때 호출한다.
    void Render_Material(const EffectMaterialInstanceData& material);
    // material preview의 UV scroll용 누적 시간을 외부 update tick에서 갱신한다.
    void Update_MaterialTime(float timeDelta);
    // ImGui viewport와 입력을 함께 관리하는 원본 머티리얼 preview 패널 진입점이다.
    void Draw_Viewport(const char* id, const EffectMaterialInstanceData& material, const char* label);
    ShaderResourceView* Get_SRV() const;
    const string& Get_StatusMessage() const;

private: //## Types
    struct PreviewVertex
    {
        Vec3 position{};
        Vec3 normal{};
        Vec2 texcoord{};
    };

    struct PreviewMeshResource
    {
        ComPtr<ID3D11Buffer> vertexBuffer{};
        ComPtr<ID3D11Buffer> indexBuffer{};
        uint32 indexCount{ 0 };
    };

private: //## Static Values
    static constexpr auto kEffectMaterialPreviewShaderId{ L"Shader_EffectMaterialPreview" };
    static constexpr float kHomeDistance{ 6.f };
    static constexpr float kHomeYawDegrees{ 0.f };
    static constexpr float kHomePitchDegrees{ 0.f };
    static constexpr float kHomeTransitionSpeed{ 10.f };
    static constexpr float kPreviewOrbitSensor{ 0.105f };

private: //## Data
    uint32 _previewTextureWidth{ 512 };
    uint32 _previewTextureHeight{ 512 };
    Shared<RenderTarget> _renderTarget{};
    Shared<ShaderCom> _shader{};
    Shared<Texture> _texture{};
    Shared<Texture> _noiseTexture{};
    Shared<Texture> _maskTexture{};
    PreviewMeshResource _planeMesh{};
    PreviewMeshResource _sphereMesh{};
    string _loadedTextureKey{};
    string _loadedNoiseTextureKey{};
    string _loadedMaskTextureKey{};
    string _statusMessage{};
    bool _hasRenderedPreview{ false };
    EffectMaterialPreviewMeshMode _meshMode{ EffectMaterialPreviewMeshMode::Plane };
    EffectPreviewOrbitCameraState _cameraState{};
    float _materialPreviewElapsedTime{}; // material preview UV scroll 계산에 사용할 preview-local 누적 시간이다.
    bool _isViewportDragging{ false };
    bool _hasPendingHome{ false };

private: //## Helper::Resource
    bool Ready_RenderTarget();
    bool Ready_Shader();
    bool Ready_Texture(const string& textureGuid, const string& texturePath);
    bool Ready_NoiseTexture(const string& textureGuid, const string& texturePath);
    bool Ready_MaskTexture(const string& textureGuid, const string& texturePath);
    bool Ready_Meshes();
    bool Ready_PlaneMesh();
    bool Ready_SphereMesh();
    bool Create_Mesh(const vector<PreviewVertex>& vertices, const vector<uint32>& indices, PreviewMeshResource& mesh);

private: //## Helper::Render
    HRESULT Render_PreviewMesh(const EffectMaterialInstanceData& material) const;
    HRESULT Render_Mesh(const PreviewMeshResource& mesh) const;
    void Resize_RenderTarget(uint32 width, uint32 height);

private: //## Helper::ViewportInput
    // preview canvas hover/drag/wheel 입력을 orbit camera 상태로 변환한다.
    void Handle_ViewportInput(bool isHovered, const ImVec2& canvasMin, const ImVec2& canvasMax);
    void Begin_ViewportDrag(const ImVec2& canvasMin, const ImVec2& canvasMax);
    void End_ViewportDrag();

private: //## Helper::Camera
    void Start_HomeTransition(); // 즉시 reset하지 않고 home 시점으로 부드럽게 복귀를 예약한다.
    void Update_HomeTransition(float timeDelta);
    void Reset_View();

private: //## Helper::Overlay
    void Draw_ViewportTextOverlay(const ImVec2& canvasMin, const ImVec2& canvasMax, const char* label);
    void Draw_MeshModeControls();

private: //## Helper::Lifecycle
    void Cleanup();

public:
    static Unique<EffectMaterialPreviewRenderer> Create(uint32 previewTextureSize = 512);
    void Free() override;
};

NS_END

#pragma once

#include "EffectMaterialAuthoring_Types.h"
#include "Helper_EffectCamera.h"

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

class EffectMaterialPreviewRenderer final
{
public:
    explicit EffectMaterialPreviewRenderer(uint32 previewTextureSize = 512);
    ~EffectMaterialPreviewRenderer();

public:
    void Render_Material(const EffectMaterialInstanceData& material);
    void Draw_Viewport(const char* id, const EffectMaterialInstanceData& material, const char* label);
    ID3D11ShaderResourceView* Get_SRV() const;
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
    static constexpr const char* kFallbackTextureId{ "Texture_Effect_DefaultTexture" };
    static constexpr const wchar_t* kEffectMaterialPreviewShaderId{ L"Shader_EffectMaterialPreview" };
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
    PreviewMeshResource _planeMesh{};
    PreviewMeshResource _sphereMesh{};
    string _loadedTextureId{};
    string _loadedNoiseTextureId{};
    string _statusMessage{};
    EffectMaterialPreviewMeshMode _meshMode{ EffectMaterialPreviewMeshMode::Plane };
    EffectPreviewOrbitCameraState _cameraState{};
    bool _isViewportDragging{ false };
    bool _hasPendingHome{ false };

private: //## Helpers
    bool Ready_RenderTarget();
    bool Ready_Shader();
    bool Ready_Texture(const string& textureId);
    bool Ready_NoiseTexture(const string& noiseTextureId);
    bool Ready_Meshes();
    bool Ready_PlaneMesh();
    bool Ready_SphereMesh();
    bool Create_Mesh(const vector<PreviewVertex>& vertices, const vector<uint32>& indices, PreviewMeshResource& mesh);
    HRESULT Render_PreviewMesh(const EffectMaterialInstanceData& material) const;
    HRESULT Render_Mesh(const PreviewMeshResource& mesh) const;
    void Resize_RenderTarget(uint32 width, uint32 height);
    void Handle_ViewportInput(bool isHovered, const ImVec2& canvasMin, const ImVec2& canvasMax);
    void Begin_ViewportDrag(const ImVec2& canvasMin, const ImVec2& canvasMax);
    void End_ViewportDrag();
    void Start_HomeTransition();
    void Update_HomeTransition(float timeDelta);
    void Reset_View();
    void Draw_ViewportTextOverlay(const ImVec2& canvasMin, const ImVec2& canvasMax, const char* label);
    void Draw_MeshModeControls();
    uint32 Resolve_BlendPassIndex(const string& blendMode) const;
};

NS_END

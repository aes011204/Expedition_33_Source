#pragma once

#include "Client_Defines.h"
#include "GameObject.h"

NS_BEGIN(Engine)
class ShaderCom;
class VIBuffer_Rect;
NS_END

NS_BEGIN(Client)

enum class BattleWorldUIPlaneBillboardMode
{
    FaceCamera,
    FixedYaw,
    FixedRotation,
};

class BattleWorldUIPlane final : public GameObject
{
    GENERATED_GAMEOBJECT(BattleWorldUIPlane, ObjectType::UI)

public:
    struct BATTLEWORLDUIPLANE_DESC final : public GameObject::GAMEOBJECT_DESC
    {
        wstring texturePath{};
        Vec3 worldPosition{ Vec3::Zero };
        Vec2 worldSize{ 1.f, 1.f };
        Vec4 tintColor{ 1.f, 1.f, 1.f, 1.f };
        BattleWorldUIPlaneBillboardMode billboardMode{ BattleWorldUIPlaneBillboardMode::FaceCamera };
        bool useDepthRead{ true };
        bool visible{ true };
    };

public:
    BattleWorldUIPlane(const ComPtr<Device>& device, const ComPtr<Context>& context);
    BattleWorldUIPlane(const BattleWorldUIPlane& prototype);
    ~BattleWorldUIPlane() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    HRESULT Render() override;

public:
    void Set_WorldPosition(const Vec3& position);
    void Set_WorldSize(const Vec2& size);
    void Set_TintColor(const Vec4& color);
    void Set_BillboardMode(BattleWorldUIPlaneBillboardMode mode);
    void Set_UseDepthRead(bool useDepthRead);
    void Set_Visible(bool visible);

    bool Is_RenderReady() const { return _renderReady; }

    void Submit_Render();

private:
    HRESULT Ready_Components();
    HRESULT Load_Texture(const wstring& texturePath);
    HRESULT Bind_ShaderResources();

    void Apply_WorldTransform();
    void Apply_BillboardRotation();
    fs::path Resolve_TexturePath(const wstring& texturePath) const;

private:
    Shared<ShaderCom> _shader{};
    Shared<VIBuffer_Rect> _viBuffer{};
    ComPtr<ID3D11ShaderResourceView> _textureSRV{};

private:
    wstring _texturePath{};
    Vec3 _worldPosition{ Vec3::Zero };
    Vec2 _worldSize{ 1.f, 1.f };
    Vec4 _tintColor{ 1.f, 1.f, 1.f, 1.f };
    BattleWorldUIPlaneBillboardMode _billboardMode{ BattleWorldUIPlaneBillboardMode::FaceCamera };

    bool _useDepthRead{ true };
    bool _visible{ true };
    bool _renderReady{ false };

public:
    static Shared<BattleWorldUIPlane> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END

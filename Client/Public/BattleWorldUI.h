#pragma once

#include "Client_Defines.h"
#include "GameObject.h"

NS_BEGIN(Engine)
class ShaderCom;
class VIBuffer_Rect;
NS_END

NS_BEGIN(Client)

enum class BattleWorldUIBillboardMode
{
    FaceCamera,
    FixedYaw,
    FixedRotation,
};

class BattleWorldUI final : public GameObject
{
    GENERATED_GAMEOBJECT(BattleWorldUI, ObjectType::UI)

public:
    struct BATTLEWORLDUI_DESC final : public GameObject::GAMEOBJECT_DESC
    {
        Vec3 worldPosition{ Vec3::Zero };
        Vec2 worldSize{ 1.f, 1.f };
        Vec3 worldRotation{ Vec3::Zero };
        Vec4 tintColor{ 1.f, 1.f, 1.f, 1.f };
        Vec4 uvRect{ 0.f, 0.f, 1.f, 1.f };
        BattleWorldUIBillboardMode billboardMode{ BattleWorldUIBillboardMode::FaceCamera };
        bool useDepthRead{ true };
        bool visible{ true };
    };

public:
    BattleWorldUI(const ComPtr<Device>& device, const ComPtr<Context>& context);
    BattleWorldUI(const BattleWorldUI& prototype);
    ~BattleWorldUI() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    HRESULT Render() override;

public:
    void Set_TextureSRV(ID3D11ShaderResourceView* srv);
    void Set_UVRect(const Vec4& uvRect);
    void Set_WorldPosition(const Vec3& position);
    void Set_WorldSize(const Vec2& size);
    void Set_WorldRotation(const Vec3& rotation);
    void Set_TintColor(const Vec4& color);
    void Set_BillboardMode(BattleWorldUIBillboardMode mode);
    void Set_UseDepthRead(bool useDepthRead);
    void Set_Visible(bool visible);

    bool Is_RenderReady() const { return _renderReady; }

    void Submit_Render();

private:
    HRESULT Ready_Components();
    HRESULT Bind_ShaderResources();

    void Apply_WorldTransform();
    void Apply_BillboardRotation();

private:
    Shared<ShaderCom> _shader{};
    Shared<VIBuffer_Rect> _viBuffer{};
    ComPtr<ID3D11ShaderResourceView> _textureSRV{};

private:
    Vec3 _worldPosition{ Vec3::Zero };
    Vec2 _worldSize{ 1.f, 1.f };
    Vec3 _worldRotation{ Vec3::Zero };
    Vec4 _tintColor{ 1.f, 1.f, 1.f, 1.f };
    Vec4 _uvRect{ 0.f, 0.f, 1.f, 1.f };
    BattleWorldUIBillboardMode _billboardMode{ BattleWorldUIBillboardMode::FaceCamera };

    bool _useDepthRead{ true };
    bool _visible{ true };
    bool _renderReady{ false };

public:
    static Shared<BattleWorldUI> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END

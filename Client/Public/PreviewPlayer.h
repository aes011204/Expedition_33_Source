#pragma once

#include "Client_Defines.h"
#include "ContainerObject.h"
#include "CostumeManager.h"

NS_BEGIN(Engine)
class FacialAnimationModifier;
class FacialClip;
NS_END

NS_BEGIN(Client)

class Body_Player;
class Head_Player;
class Hair_Player;
class FaceAccessory_Player;
class Weapon;

class PreviewPlayer final : public ContainerObject
{
    GENERATED_GAMEOBJECT(PreviewPlayer, ObjectType::None)

public:
    struct PreviewPlayerDesc : public ContainerObject::ContainerObjectDesc
    {
        HeroId heroId = HeroId::END;
    };

private:
    struct CostumePartTransform
    {
        Vec3 position{ Vec3::Zero };
        Vec3 rotation{ Vec3::Zero };
        Vec3 scale{ Vec3::One };
    };

    struct PreviewWeaponTransformOffset
    {
        Vec3 position{ Vec3::Zero };
        Vec3 rotation{ Vec3::Zero };
    };

    using CostumePartTransformMap = umap<wstring, CostumePartTransform>;

    struct CostumeVariantParts
    {
        Shared<Body_Player> body{};
        Shared<Head_Player> head{};
        Shared<Hair_Player> hair{};
        Shared<FaceAccessory_Player> beard{};
        Shared<FaceAccessory_Player> mustache{};
        Shared<FaceAccessory_Player> glasses{};
        CostumePartTransformMap baseTransforms{};
        CostumePartTransformMap presetTransforms{};
    };

public:
    PreviewPlayer(const ComPtr<Device>& device, const ComPtr<Context>& context);
    PreviewPlayer(const PreviewPlayer& prototype);
    ~PreviewPlayer() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    BeginPlay() override;
    void    From_Json(const json& data) override;

    void    Priority_Update(float timeDelta) override {}
    void    Update(float timeDelta) override {}
    void    Late_Update(float timeDelta) override {}
    HRESULT Render() override { return S_OK; }

public:
    HeroId  Get_HeroId() const { return _heroId; }

    void    Update_Preview(float timeDelta, bool facialEnabled);
    HRESULT Render_PreviewParts();

    void    Play_PreviewAnimation(PreviewAnimState introAnim, PreviewAnimState idleAnim);
    void    Update_GameMenuFacialPreview(float timeDelta, bool enabled);

    HRESULT Sync_FromRuntimeData();
    HRESULT Apply_CostumePreset(const CostumePreset& preset);
    HRESULT Prewarm_CostumeVariants();
    HRESULT Apply_EquippedWeaponFromRuntimeData();

private:
    HRESULT Ready_DefaultCostumeParts();
    HRESULT Ensure_CostumeVariantBuilt(const CostumePreset& preset);
    HRESULT Activate_CostumeVariant(const CostumePreset& preset);
    HRESULT Attach_WeaponPreview(const wstring& partTag, const string& prefabName, const string& socketName);

    void    Rebind_ActiveCostumeVariant(const string& presetId);
    HRESULT Warmup_CostumeVariantParts(CostumeVariantParts& parts);
    void    Clear_AllCostumeVariants();
    void    Clear_WeaponPreview();
    void    Apply_ChangeWeaponPreviewWeaponOffset(Weapon* weapon, const string& prefabName) const;

    void    Capture_DefaultCostumePartTransformsIfNeeded();
    CostumePartTransformMap Capture_CostumeVariantPartTransforms(const CostumeVariantParts& parts) const;
    CostumePartTransformMap Build_CostumePresetTransforms(
        const CostumePreset& preset,
        const CostumePartTransformMap& baseTransforms) const;

    void    Apply_CostumePartTransforms(const CostumePartTransformMap& partTransforms);
    void    Apply_CostumePartTransform(const wstring& partTag, const CostumePartTransformMap& partTransforms);
    void    Apply_CostumePartTransform(
        const wstring& partTag,
        const CostumePartTransformMap& partTransforms,
        const CostumePartLocalTransform& fallbackTransform);

    void    Reapply_CostumePartBaseMaterial(const wstring& partTag);
    void    Reapply_CostumeBaseMaterials();
    void    Reapply_CurrentPreviewAnimation(float startFrame);

    Shared<Body_Player> Create_CostumeBodyVariant(const wstring& modelTag);
    Shared<Head_Player> Create_CostumeHeadVariant(const wstring& modelTag, const Body_Player* body);
    Shared<Hair_Player> Create_CostumeHairVariant(const wstring& modelTag, const Head_Player* head);
    Shared<FaceAccessory_Player> Create_CostumeFaceAccessoryVariant(const wstring& modelTag, const Head_Player* head);

    HeroId  Parse_HeroIdName(const string& heroIdName) const;
    string  Get_HeroIdName() const;
    string  Get_CharacterId() const;
    string  Get_MainWeaponSocketName() const;
    wstring  Get_PreviewAnimMapPath() const;
    bool    Apply_PreviewAnimStateMapping(Body_Player* body) const;

    HairChainPhysicsDesc Get_HairChainPhysicsDesc() const;
    const wchar_t* Get_DefaultBodyModelTag() const;
    const wchar_t* Get_DefaultHeadModelTag() const;
    const wchar_t* Get_DefaultHairModelTag() const;
    const wchar_t* Get_DefaultBeardModelTag() const;
    const wchar_t* Get_DefaultMustacheModelTag() const;
    const wchar_t* Get_GameMenuFacialClipPath() const;

private:
    HeroId _heroId = HeroId::END;
    string _heroIdName = "END"; // preview prefab custom_properties에서 읽는 캐릭터 식별자

    CostumePartTransformMap _defaultCostumePartTransforms{};
    umap<string, CostumeVariantParts> _costumeVariantsByPreset{};
    string _activeCostumePresetId{};

    Shared<Weapon> _weapon{};
    Shared<Weapon> _subWeapon{};
    string _activeWeaponId{};
    umap<string, PreviewWeaponTransformOffset> _changeWeaponPreviewWeaponOffsets{};

    Shared<Engine::FacialClip> _gameMenuFacialClip{};
    Shared<Engine::FacialAnimationModifier> _gameMenuFacialModifier{};
    bool _gameMenuFacialEnabled = false;

    PreviewAnimState _currentPreviewIntroAnim = PreviewAnimState::Overview_Intro;
    PreviewAnimState _currentPreviewIdleAnim = PreviewAnimState::Overview_Idle;
    PreviewAnimState _pendingPreviewIdleAnim = PreviewAnimState::Overview_Idle;
    bool _isPlayingPreviewIntro = false;

public:
    static Shared<PreviewPlayer> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END

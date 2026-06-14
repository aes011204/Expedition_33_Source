#pragma once

#include "UI_Base.h"
#include "UI_SpriteVfx.h"
#include "EffectInstancePool.h"

NS_BEGIN(Client)

class UI_Vfx final : public UI_Base
{
public:
    struct UI_VFX_DESC : public UI_Base::UI_DESC
    {
        UISpriteVfxAsset asset{};
        string assetPath{};
        bool playOnOpen{ true };
        bool loop{ false };
        bool stopOnClose{ true };
    };

public:
    UI_Vfx();
    UI_Vfx(const UI_Vfx& prototype);
    ~UI_Vfx() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void Open() override;
    void Close() override;
    void Update(float timeDelta) override;
    HRESULT Render() override;

protected:
    void On_EffectiveOpenChanged(bool isEffectivelyOpen) override;

private:
    void Play_Vfx();
    void Stop_Vfx();
    void Update_VfxTransform();
    bool Is_VfxFinished() const;
    Vec3 Resolve_EffectWorldPosition() const;
    Vec3 Resolve_EffectWorldScale() const;
    Vec3 Resolve_EffectWorldRotation() const;

private:
    UISpriteVfxAsset _asset{};
    string _assetPath{};
    bool _playOnOpen{ true };
    bool _loop{ false };
    bool _stopOnClose{ true };
    bool _playedForCurrentOpen{ false };
    Shared<EffectDefinition> _definition{};
    EffectInstancePool::AttachedHandle _effectHandle{};

public:
    static Shared<UI_Vfx> Create();
    Shared<UI_Base> Clone(void* arg) override;
    void Free() override;
};

NS_END

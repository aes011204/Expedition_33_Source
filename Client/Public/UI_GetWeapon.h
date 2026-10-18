#pragma once

#include "UI_RuntimePanelBase.h"

NS_BEGIN(Engine)
class UI_Base;
NS_END

NS_BEGIN(Client)

class UI_GetWeapon final : public UI_RuntimePanelBase
{
public:
    HRESULT Initialize(uint32 levelIndex) override;
    void Update(float timeDelta) override;

    void Open_ForWeaponIds(const vector<string>& weaponPresentationIds);
    void Close() override;
    void Close_Immediately();

    static Unique<UI_GetWeapon> Create(uint32 levelIndex);

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void Open_NextWeapon();
    void Finish_CurrentAndOpenNext();
    void Close_AllRoots();
    Shared<Engine::UI_Base> Resolve_Root(const string& presentationId) const;
    bool Is_WeaponQueuedOrOpen(const string& presentationId) const;
    void Apply_ActiveGroupRootLayout(const string& presentationId, const Shared<Engine::UI_Base>& root);
    string Normalize_WeaponPresentationId(const string& presentationId) const;

private:
    umap<string, Shared<Engine::UI_Base>> _weaponRoots{};

    vector<string> _activePresentationIds{};
    vector<vector<string>> _queuedPresentationGroups{};

    float _elapsed = 0.f;
    float _animationDuration = 1.5f;
};

NS_END

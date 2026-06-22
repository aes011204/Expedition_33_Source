#pragma once

#include "UI_RuntimePanelBase.h"

NS_BEGIN(Engine)
class UI_Base;
NS_END

NS_BEGIN(Client)

class UI_GetPictos final : public UI_RuntimePanelBase
{
public:
    HRESULT Initialize(uint32 levelIndex) override;
    void Update(float timeDelta) override;

    void Open_ForPictosIds(const vector<string>& pictosIds);
    void Close() override;
    void Close_Immediately();

    static Unique<UI_GetPictos> Create(uint32 levelIndex);

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void Open_NextPictos();
    void Finish_CurrentAndOpenNext();
    void Close_AllRoots();
    Shared<Engine::UI_Base> Resolve_Root(const string& pictosId) const;
    bool Is_PictosQueuedOrOpen(const string& pictosId) const;
    string Normalize_PictosId(const string& pictosId) const;

private:
    umap<string, Shared<Engine::UI_Base>> _pictosRoots{};

    string _activePictosId{};
    vector<string> _queuedPictosIds{};

    float _elapsed = 0.f;
    float _animationDuration = 1.5f;
};

NS_END

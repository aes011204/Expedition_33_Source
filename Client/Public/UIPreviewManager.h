#pragma once
#include "Base.h"
#include "PreviewShotTable.h"

NS_BEGIN(Engine)
class TransformCom;
class Camera;
NS_END

NS_BEGIN(Client)
class PreviewCamera;
class PreviewPlayer;

class UIPreviewManager : public Base
{
public:
    struct PreviewHeroEntry
    {
        HeroId heroId = HeroId::END;        // 캐릭터마다 다른 머테리얼 force pass index를 따로 관리하기 위해
        Shared<PreviewPlayer> player{};
    };

public:

    struct PreviewSlotDesc
    {
        HeroId heroId;
        Vec3   position;
        Vec3   rotation;
        bool   isTarget = false;
    };

public:
    UIPreviewManager();
    virtual ~UIPreviewManager();

public:
    HRESULT Initialize();
    void    Update(float timeDelta);

public:
    void    Begin_Preview(PreviewShotPanel panel);
    void    Show_Preview(HeroId heroId, PreviewShotPanel panel);
    void    Hide_Preview();
    void    Apply_Layout(const vector<PreviewSlotDesc>& layout);
    const vector<PreviewHeroEntry>& Get_PreviewHeroes() const { return _previewHeroes; }
    Shared<PreviewCamera> Get_PreviewCamera() const { return _previewCamera; }

    // 미리 준비
    HRESULT Prepare_PreviewResources();
    // 데이터 연동
    void    Sync_PreviewHeroRuntimeData(HeroId heroId);

    void    Play_PreviewAnimation(HeroId heroId, PreviewAnimState introAnim, PreviewAnimState idleAnim, float blendDuration = 0.25f);

private:
    HRESULT Ensure_PreviewCamera();
    HRESULT Ensure_PreviewCharacters();
    HRESULT Spawn_PreviewCamera();
    HRESULT Spawn_PreviewCharacter();
  //  void    Restore_PreviousCamera();
    void    Clear_StatusPreviewHeroes();

private:
    bool                      _isActive = false;
    bool                      _pendingPreviewReveal = false;
    PreviewShotPanel          _activePanel = PreviewShotPanel::CharacterStatus;
    HeroId                    _shownHeroId = HeroId::END;
    vector<PreviewHeroEntry>  _previewHeroes{};
  //  Shared<Camera>            _previousCamera{};
    Shared<PreviewCamera>     _previewCamera{};
    wstring                   _previewLayerTag = L"Layer_UIPreview";

public:
    static Shared<UIPreviewManager> Create();
    void Free() override;
};
NS_END

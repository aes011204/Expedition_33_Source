#pragma once

#include "Editor_Window.h"

NS_BEGIN(Engine)
class ModelCom;
NS_END

NS_BEGIN(Client)
class CombatStatCom;
class SL_Base;
NS_END

NS_BEGIN(MainEditor)

class SoulLike_Viewer final : public Editor_Window
{
public:
    SoulLike_Viewer();
    ~SoulLike_Viewer() override = default;

public:
    HRESULT Initialize() override;
    void    Update(float timeDelta) override;
    void    Pre_Render() override;
    void    Render() override;
    bool    CanSave() const override { return false; }

private:
    struct RuntimeEntry
    {
        Shared<Client::SL_Base> actor{};
        wstring layerTag{};
        string objectName{};
        string className{};
        string sourcePrefab{};

        string stateName{};
        string animationStateName{};
        string animationClipName{};
        uint32 animationStateIndex = UINT32_MAX;
        float animationTimeSec = 0.f;
        float animationDurationSec = 0.f;

        float curHp = 0.f;
        float maxHp = 0.f;
        uint32 curAp = 0;
        uint32 maxAp = 0;
        uint32 shield = 0;
        float speed = 0.f;
        string aliveState{};

        Vec3 position{};
        bool visible = false;
        bool markedDestroy = false;
        bool hasStat = false;
    };

private:
    void Refresh_Entries();
    void Draw_Header();
    void Draw_Table();
    void Draw_Detail(const RuntimeEntry& entry);

    bool Passes_Filter(const RuntimeEntry& entry) const;

    static Shared<Engine::ModelCom> Find_PrimaryModel(const Shared<Client::SL_Base>& actor);
    static string Resolve_ClassName(const Shared<Client::SL_Base>& actor);
    static string Resolve_StateName(const Shared<Client::SL_Base>& actor);
    static string Resolve_ModelStateName(const Shared<Engine::ModelCom>& model);

private:
    vector<RuntimeEntry> _entries{};
    int32 _selectedIndex = -1;
    bool _autoRefresh = true;
    bool _showDead = true;
    char _searchBuffer[64]{};

public:
    static Shared<SoulLike_Viewer> Create();
};

NS_END

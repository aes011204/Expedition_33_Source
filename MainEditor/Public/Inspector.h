// MainEditor/Public/Inspector.h
#pragma once

#include "Editor_Window.h"
#include "Reflection_Types.h"

NS_BEGIN(Engine)


    class GameObject;
class Component;
struct ClassReflectionInfo;
struct PropertyInfo;
NS_END

namespace Client
{
    class IRuntimeWorldUI_EditorTarget;
}

NS_BEGIN(MainEditor)

class Inspector final : public Editor_Window
{
public:
    Inspector();
    ~Inspector() override = default;

public:
    void Render() override;

public:
    void Set_PrimaryShadowLightTarget(bool enabled);
    bool Is_PrimaryShadowLightTarget() const { return _isPrimaryShadowLightTarget; }

public:
    static bool Draw_ComponentOverride(const Shared<Component>& component);
    static bool Draw_ReflectionSection(const string& headerId, void* instance, const ClassReflectionInfo& info);

    static bool Draw_ReflectionProperties(void* instance, const ClassReflectionInfo& info);
    static bool Draw_ReflectionProperty(const string& idPrefix, void* instance, const PropertyInfo& property);
    static bool Draw_MonsterPatrolPointEditor(const Shared<GameObject>& target, bool showMapSaveButton = false);
    static bool Should_DrawMonsterPatrolPoints();


    static void Set_BTBlackboardKeys(const vector<Engine::BTBlackboardKeyView>* keys);


private:
    struct FGameObjectClassPickerEntry
    {
        string className{};
        string displayName{};
        string category{};
    };

private:
    void Draw_PrimaryShadowLightInspector();

    static void Draw_UIInspector(
        const Shared<Engine::UI_Base>& ui,
        const string& groupName,
        const string& displayName,
        bool positionDrivenByCode);
    static void Draw_RuntimeWorldUIPlacementInspector(
        Client::IRuntimeWorldUI_EditorTarget* target,
        const string& groupName,
        const string& displayName);
    static void Draw_RuntimeUITestOffsetSection(const Shared<Engine::UI_Base>& ui); // Battle UI에만 강제 테스트 오프셋 편집 섹션을 노출한다.
    static void Draw_RuntimeUIOwnerReflectionSection(const Shared<Engine::UI_Base>& ui);
    static bool Draw_UIVec3Row(const char* label, const string& id, Vec3& value, float speed = 0.01f);
    static bool Draw_UIVec2Row(const char* label, const string& id, Vec2& value, float speed = 1.f);
    static bool Draw_UIFloatRow(const char* label, const string& id, float& value, float speed = 0.01f);

    static void Draw_Components(const Shared<GameObject>& target);
    static void Draw_ObjectReflection(const Shared<GameObject>& target);
    static bool Draw_MonsterBattlePrefabList(const Shared<GameObject>& target);

    static bool Draw_Vec2PropertyRow(const string& idPrefix, const PropertyInfo& property, Vec2& value);
    static bool Draw_Vec3PropertyRow(const string& idPrefix, const PropertyInfo& property, Vec3& value);
    static bool Draw_Vec4PropertyRow(const string& idPrefix, const PropertyInfo& property, Vec4& value);

    static bool Draw_ColorPropertyRow(const string& idPrefix, Vec4& value);
    static bool Draw_EnumPropertyRow(const string& idPrefix, const PropertyInfo& property, int& value);
    static bool Draw_SpawnActorClassPropertyRow(
        const string& hiddenId,
        std::string& value);
    static void Rebuild_GameObjectClassPickerEntries();
    static bool Matches_GameObjectClassPickerSearch(const FGameObjectClassPickerEntry& entry, const string& searchText);

    static bool Draw_VecAxis(const char* axisLabel, const ImVec4& color, float& value, float speed, const string& uniqueId, float width);
#ifdef _DEBUG
    void Draw_ShadowDebugRTToggleRow(const wchar_t* targetTag, const char* label);
#endif
private:
    static constexpr float InspectorLabelWidth = 110.f;
    static const vector<Engine::BTBlackboardKeyView>* _btBlackboardKeys;
    static vector<FGameObjectClassPickerEntry> _gameObjectClassPickerEntries;
    static char _spawnActorClassSearchBuffer[256];
    bool _isPrimaryShadowLightTarget = false;
public:
    static Shared<Inspector> Create();
};

NS_END

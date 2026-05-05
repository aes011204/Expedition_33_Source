#pragma once

#include "Editor_Window.h"

NS_BEGIN(Engine)
class GameObject;
class Component;
struct ClassReflectionInfo;
struct PropertyInfo;
NS_END

NS_BEGIN(MainEditor)

class Inspector final : public Editor_Window
{
public:
    Inspector();
    ~Inspector() override = default;

public:
    void Render() override;

private:
    static bool Draw_ComponentOverride(const Shared<Component>& component);

    static void Draw_Components(const Shared<GameObject>& target);
    static void Draw_ObjectReflection(const Shared<GameObject>& target);

    static void Draw_ReflectionSection(const string& headerId, void* instance, const Engine::ClassReflectionInfo& info);
    static void Draw_ReflectionProperties(void* instance, const Engine::ClassReflectionInfo& info);
    static void Draw_ReflectionProperty(const string& idPrefix, void* instance, const Engine::PropertyInfo& property);

    static bool Draw_Vec3PropertyRow(const string& idPrefix, const Engine::PropertyInfo& property, Vec3& value);
    static bool Draw_VecAxis(const char* axisLabel, const ImVec4& color, float& value, float speed, const string& uniqueId, float width);

public:
    static Shared<Inspector> Create();
};

NS_END

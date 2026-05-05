#pragma once

#include "Editor_Window.h"

NS_BEGIN(Engine)
class GameObject;
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
    static void Draw_Component(const string& key, const Shared<Component>& component);
    static void Draw_Components(const Shared<GameObject>& target);

public:
    static Shared<Inspector> Create();
};

NS_END

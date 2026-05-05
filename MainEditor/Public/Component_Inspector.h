#pragma once

NS_BEGIN(Engine)
class Component;
NS_END

NS_BEGIN(MainEditor)

class Component_Inspector
{
public:
    Component_Inspector() = default;
    virtual ~Component_Inspector() = default;

public:
    virtual void Draw_Inspector(const Shared<Component>& component) = 0;

protected:
    bool Draw_Header(const char* title)
    {
        return ImGui::CollapsingHeader(title, ImGuiTreeNodeFlags_DefaultOpen);
    }
};

NS_END

#pragma once
#include "Editor_Window.h"

NS_BEGIN(MainEditor)

class Camera_Tool : public Editor_Window
{
public:
    Camera_Tool();
    virtual ~Camera_Tool() = default;

public:
    virtual void Render() override;

private:
    void Render_CameraList();
    void Render_CommonSettings();
    void Render_SpecificSettings();

private:
    int _selectedCameraIndex = { 0 };

public:
    static Shared<Camera_Tool> Create();
};

NS_END

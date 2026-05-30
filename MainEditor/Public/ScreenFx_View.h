#pragma once
#include "Editor_Window.h"
#include <map>
#include <string>
#include <functional>

NS_BEGIN(Engine)
class ScreenFx_Instance;
NS_END

NS_BEGIN(MainEditor)

class ScreenFx_View final : public Editor_Window
{
public:
    ScreenFx_View();
    ~ScreenFx_View() override = default;

public:
    HRESULT Initialize() override;
    void Update(float timeDelta) override;
    void Pre_Render() override;
    void Render() override;

public:
    static Shared<ScreenFx_View> Create();

private:
    Shared<Engine::ScreenFx_Instance> _previewInstance{ nullptr };
    std::string _selectedEffectName{ "" };
    
    bool  _isPlaying{ false };
    float _playbackSpeed{ 1.0f };
    bool  _autoLoop{ true };
};

NS_END

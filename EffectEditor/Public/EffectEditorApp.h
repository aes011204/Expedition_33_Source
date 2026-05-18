#pragma once

#include "Base.h"

NS_BEGIN(Engine)
class GameInstance;
NS_END

NS_BEGIN(EffectEditor)

class EffectEditorApp final : public Base
{
public:
    EffectEditorApp();
    ~EffectEditorApp() override;

public:
    HRESULT Initialize();
    void Priority_Update(float timeDelta);
    void Update(float timeDelta);
    void Late_Update(float timeDelta);
    HRESULT Render();

private:
    inline static const Vec4 kDefaultClearColor{ 0.18f, 0.18f, 0.18f, 1.f };

private:
    ComPtr<Device> _device{};
    ComPtr<Context> _context{};

private:
    HRESULT Ready_Fonts();
    HRESULT Ready_Prototype_For_Static_Level();
    HRESULT Ready_PreviewLevel(); // EffectEditor 전용 preview level을 GamePlay resource index 위에 올린다.

    wstring Get_ClientResourceRoot() const;
    wstring Get_ClientResourcePath(const wchar_t* relativePath) const;

public:
    static Unique<EffectEditorApp> Create();
    void Free() override;
};

NS_END

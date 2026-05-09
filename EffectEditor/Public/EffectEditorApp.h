#pragma once

#include "Base.h"
#include "Client_Defines.h"

NS_BEGIN(Engine)
class GameInstance;
NS_END

NS_BEGIN(EffectEditor)

class EffectEditorApp final : public Base
{
public:
    EffectEditorApp();
    ~EffectEditorApp() override = default;

public:
    HRESULT Initialize();
    void Priority_Update(float timeDelta);
    void Update(float timeDelta);
    void Late_Update(float timeDelta);
    HRESULT Render();

private:
    inline static const Vec4 defaultClearColor{ 0.13f, 0.13f, 0.13f, 1.0f };
    static constexpr uint32 computeSmokeElementCount{ 4 }; // compute smoke test에서 검증할 float4 개수.
    static constexpr uint32 computeSmokeThreadCount{ 4 };  // smoke shader의 numthreads.x 값.

private:
    ComPtr<Device> _device;
    ComPtr<Context> _context;

private:
    HRESULT Ready_Fonts();
    HRESULT Ready_Prototype_For_Static_Level();
    HRESULT Ready_PreviewLevel();         // EffectEditor 전용 preview level을 GamePlay resource index 위에 올린다.
    HRESULT Run_ComputeShaderSmokeTest(); // Debug 전용 compute foundation 검증을 1회 수행한다.

    wstring Get_ClientResourceRoot() const;
    wstring Get_ClientResourcePath(const wchar_t* relativePath) const;
    wstring Get_ClientShaderPath(const wchar_t* fileName) const; // Client/Bin/ShaderFiles 아래 shader 경로를 만든다.

public:
    static Unique<EffectEditorApp> Create();
    void Free() override;
};

NS_END

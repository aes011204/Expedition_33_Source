#pragma once

#include "Base.h"
#include "Debug_Types.h"

NS_BEGIN(Engine)

class ENGINE_DLL Debug_Manager : public Base
{
public:
    Debug_Manager(ComPtr<Device> device, ComPtr<Context> context);
    virtual ~Debug_Manager() = default;

public:
    HRESULT Initialize();
    void    Update(float timeDelta);

    HRESULT Render_Depth();   // depth test가 켜진 디버그 도형 처리
    HRESULT Render_Overlay(); // depth test가 꺼진 디버그 도형 처리

public:
    void    Draw_Box(const DebugBoxDesc& desc);
    void    Draw_Sphere(const DebugSphereDesc& desc);
    void    Draw_Capsule(const DebugCapsuleDesc& desc);
    void    Draw_Line(const DebugLineDesc& desc);
    void    Draw_TraceLine(const DebugTraceLineDesc& desc);

public:
    void    Clear();

    // 전체 디버그 렌더 출력을 켜고 끌 때
    void    Set_Enabled(bool enabled) { _isEnabled = enabled; }

    // 현재 디버그 렌더 출력 가능 여부를 조회할 때
    bool    Is_Enabled() const { return _isEnabled; }

private:
    HRESULT Prepare_RenderState();

    void Render_BoxEntries(const vector<DebugBoxEntry>& entries);
    void Render_SphereEntries(const vector<DebugSphereEntry>& entries);
    void Render_LineEntries(const vector<DebugLineEntry>& entries);

private:
    ComPtr<Device>  _device;                   
    ComPtr<Context> _context;           

    Shared<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> _batch; 
    Shared<DirectX::BasicEffect>    _effect;     
    ComPtr<ID3D11InputLayout>       _inputLayout;   

    vector<DebugBoxEntry>           _depthBoxes;       
    vector<DebugSphereEntry>        _depthSpheres;  
    vector<DebugLineEntry>          _depthLines;      

    vector<DebugBoxEntry>           _overlayBoxes;     
    vector<DebugSphereEntry>        _overlaySpheres;
    vector<DebugLineEntry>          _overlayLines;    

    vector<DebugMeshEntry>          _depthMeshes;
    vector<DebugMeshEntry>          _overlayMeshes;

    bool _isEnabled = true;                    // 전체 디버그 렌더 출력을 허용할지 여부

public:
    static Unique<Debug_Manager> Create(ComPtr<Device> device, ComPtr<Context> context);
    void Free() override;

};

NS_END

#include "Debug_Manager.h"
#include "GameInstance.h"

#include "DebugDraw.h"
#include "Mesh.h"
#include "ModelCom.h"

Debug_Manager::Debug_Manager(ComPtr<Device> device, ComPtr<Context> context)
    : _device(device)
    , _context(context)
{
}

HRESULT Debug_Manager::Initialize()
{
    _batch = make_shared<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>>(_context.Get());
    _effect = make_shared<DirectX::BasicEffect>(_device.Get());

    CHECK_NULL(_batch, E_FAIL);
    CHECK_NULL(_effect, E_FAIL);

    _effect->SetVertexColorEnabled(true);

    const void* shaderByteCode = nullptr;
    size_t shaderByteCodeLength = 0;
    _effect->GetVertexShaderBytecode(&shaderByteCode, &shaderByteCodeLength);

    if (FAILED(_device->CreateInputLayout(
        DirectX::VertexPositionColor::InputElements,
        DirectX::VertexPositionColor::InputElementCount,
        shaderByteCode,
        shaderByteCodeLength,
        _inputLayout.GetAddressOf())))
    {
        return E_FAIL;
    }

    return S_OK;
}

void Debug_Manager::Update(float timeDelta)
{
    // 프레임 시작 시 이전 프레임의 1프레임 요청을 제거하고, 유지 시간이 있는 요청은 남은시간 줄이기
    auto updateEntries = [timeDelta]<typename TEntry>(vector<TEntry>& entries)
    {
        std::erase_if(entries, [timeDelta](TEntry& entry)
        {
            if (entry.lifetime.isOneFrame)
                return true;

            entry.lifetime.remainingTime -= timeDelta;
                return entry.lifetime.remainingTime <= 0.f;
            });
    };

    updateEntries(_depthBoxes);
    updateEntries(_depthSpheres);
    updateEntries(_depthLines);

    updateEntries(_overlayBoxes);
    updateEntries(_overlaySpheres);
    updateEntries(_overlayLines);

    updateEntries(_depthMeshes);
    updateEntries(_overlayMeshes);
}

HRESULT Debug_Manager::Render_Depth()
{
}

HRESULT Debug_Manager::Render_Overlay()
{
}

void Debug_Manager::Draw_Box(const DebugBoxDesc& desc)
{
}

void Debug_Manager::Draw_Sphere(const DebugSphereDesc& desc)
{
}

void Debug_Manager::Draw_Line(const DebugLineDesc& desc)
{
}

void Debug_Manager::Draw_TraceLine(const DebugTraceLineDesc& desc)
{
}

void Debug_Manager::Draw_Mesh(const DebugMeshDesc& desc)
{
}

void Debug_Manager::Clear()
{
}

HRESULT Debug_Manager::Prepare_RenderState()
{
    CHECK_NULL(_effect, E_FAIL);

    _effect->SetWorld(Matrix::Identity);
    _effect->SetView(*GAME->Get_Transform(D3DTS::View));
    _effect->SetProjection(*GAME->Get_Transform(D3DTS::Proj));

    _context->IASetInputLayout(_inputLayout.Get());
    _effect->Apply(_context.Get());

    return S_OK;
}

void Debug_Manager::Render_BoxEntries(const vector<DebugBoxEntry>& entries)
{
}

void Debug_Manager::Render_SphereEntries(const vector<DebugSphereEntry>& entries)
{
}

void Debug_Manager::Render_LineEntries(const vector<DebugLineEntry>& entries)
{
}

void Debug_Manager::Render_MeshEntries(const vector<DebugMeshEntry>& entries)
{
}

Unique<Debug_Manager> Debug_Manager::Create(ComPtr<Device> device, ComPtr<Context> context)
{
    return make_unique<Debug_Manager>(device, context);
}

void Debug_Manager::Free()
{
    Base::Free();
}

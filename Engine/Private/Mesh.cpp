#include "Mesh.h"
#include "Bone.h"


NS_BEGIN(Engine)

Mesh::Mesh(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : VIBuffer{ device, context } {}

Mesh::Mesh(const Mesh& prototype)
    : VIBuffer{ prototype }
{
    _name = prototype._name;
    _materialIndex = prototype._materialIndex;
}

HRESULT Mesh::RenderInstanced(Buffer* instanceVB, uint32 instanceStride, uint32 instanceCount)
{
    if (!instanceVB || instanceCount == 0)
    {
        return E_FAIL;
    }

    Bind_Resources();

    const uint32 offset = 0;
    _context->IASetVertexBuffers(1, 1, &instanceVB, &instanceStride, &offset);
    _context->DrawIndexedInstanced(_numIndices, instanceCount, 0, 0, 0);

    return S_OK;
}

HRESULT Mesh::RenderInstancedIndirect(Buffer* instanceVB, uint32 instanceStride, Buffer* indirectArgsBuffer)
{
    if (!instanceVB || !indirectArgsBuffer)
    {
        return E_FAIL;

    }

    Bind_Resources();

    _context->UpdateSubresource(indirectArgsBuffer, 0, nullptr, &_numIndices, 0, 0);

    const uint32 offset = 0;
    _context->IASetVertexBuffers(1, 1, &instanceVB, &instanceStride, &offset);
    _context->DrawIndexedInstancedIndirect(indirectArgsBuffer, 0);

    return S_OK;
}

HRESULT Mesh::Render_ComputeSkinned(Buffer* vertexBuffer, uint32 vertexStride)
{
    if (vertexBuffer == nullptr || vertexStride == 0)
        return E_FAIL;

    ID3D11Buffer* vertexBuffers[] = { vertexBuffer };
    uint32 vertexStrides[] = { vertexStride };
    uint32 offsets[] = { 0 };

    _context->IASetVertexBuffers(0, 1, vertexBuffers, vertexStrides, offsets);
    _context->IASetIndexBuffer(_ib.Get(), 2 == _indexStride ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);
    _context->IASetPrimitiveTopology(_primitiveType);
    _context->DrawIndexed(_numIndices, 0, 0);

    return S_OK;
}

const vector<uint32>& Mesh::Get_BoneIndices() const
{
    static const vector<uint32> empty;
    return empty;
}

void Mesh::Free()
{
    VIBuffer::Free();
}

NS_END

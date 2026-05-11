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

void Mesh::Free()
{
    VIBuffer::Free();
}

NS_END

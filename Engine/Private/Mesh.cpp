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

void Mesh::Free()
{
    VIBuffer::Free();
}

NS_END

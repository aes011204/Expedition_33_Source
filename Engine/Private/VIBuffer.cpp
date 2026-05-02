#include "VIBuffer.h"

VIBuffer::VIBuffer(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: Component { device, context }
{
}

VIBuffer::VIBuffer(const VIBuffer& prototype)
	: Component{ prototype }
	, _vb { prototype._vb }
	, _ib { prototype._ib }
	, _numVertexBuffers { prototype._numVertexBuffers }
	, _vertexStride{ prototype._vertexStride }
	, _numVertices{ prototype._numVertices }	
	, _indexStride { prototype._indexStride }
	, _numIndices { prototype._numIndices }
	, _primitiveType { prototype._primitiveType }
	
{
	
}

HRESULT VIBuffer::Initialize_Prototype()
{
	return S_OK;
}

HRESULT VIBuffer::Initialize(void* arg)
{
	return S_OK;
}

HRESULT VIBuffer::Bind_Resources()
{
	ID3D11Buffer* vertexBuffers[] = {
		_vb.Get(),
		// _vb, 
	};

	uint32 vertexStrides[] = {
		_vertexStride, 
	};

	uint32			offsets[] = {
		0
	};


	_context->IASetVertexBuffers(0, _numVertexBuffers, vertexBuffers, vertexStrides, offsets);
	_context->IASetIndexBuffer(_ib.Get(), 2 == _indexStride ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);
	_context->IASetPrimitiveTopology(_primitiveType);

	

	return S_OK;
}

HRESULT VIBuffer::Render()
{
	_context->DrawIndexed(_numIndices, 0, 0);

	return S_OK;
}

void VIBuffer::Free()
{
	__super::Free();
}

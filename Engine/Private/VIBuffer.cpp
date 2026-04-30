#include "VIBuffer.h"

VIBuffer::VIBuffer(ComPtr<Device> device, ComPtr<Context> context)
	: Component { device, context }
{
}

VIBuffer::VIBuffer(const VIBuffer& Prototype)
	: Component{ Prototype }
	, _vb { Prototype._vb }
	, _ib { Prototype._ib }
	, _numVertexBuffers { Prototype._numVertexBuffers }
	, _vertexStride{ Prototype._vertexStride }
	, _numVertices{ Prototype._numVertices }	
	, _indexStride { Prototype._indexStride }
	, _numIndices { Prototype._numIndices }
	, _primitiveType { Prototype._primitiveType }
	
{
	Safe_AddRef(_vb);
	Safe_AddRef(_ib);
}

HRESULT VIBuffer::Initialize_Prototype()
{
	return S_OK;
}

HRESULT VIBuffer::Initialize(void* pArg)
{
	return S_OK;
}

HRESULT VIBuffer::Bind_Resources()
{
	ID3D11Buffer* pVertexBuffers[] = {
		_vb, 
		// _vb, 
	};

	uint32 iVertexStrides[] = {
		_vertexStride, 
	};

	uint32			iOffsets[] = {
		0
	};


	_context->IASetVertexBuffers(0, _numVertexBuffers, pVertexBuffers, iVertexStrides, iOffsets);
	_context->IASetIndexBuffer(_ib, 2 == _indexStride ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);
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

	Safe_Release(_vb);
	Safe_Release(_ib);
}

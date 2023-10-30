#pragma once
#include "D3D12VertexBuffer.h"
#include "D3D12IndexBuffer.h"

using namespace DirectX;

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT2 texCoord;
    XMFLOAT4 color;
};

class D3D12Mesh
{
private:
	XMVECTOR position;
    Vertex* pVertices;
    UINT16* pIndices;
    UINT verticesSize;
    UINT indicesSize;
    UINT indicesNum;

public:
    D3D12Mesh();
    ~D3D12Mesh();

    void SetVertices(Vertex* triangleVertices, UINT size);
    void SetIndices(UINT16* triangleIndices, UINT size);
    UINT GetVerticesSize();
    UINT GetIndicesSize();
    UINT GetIndicesNum();
    void const* GetVerticesData();
    void const* GetIndicesData();
    void CopyVertices(void* destination);
    void CopyIndices(void* destination);
    void CreateView();

    std::unique_ptr<D3D12VertexBuffer> VertexBuffer;
    std::unique_ptr<D3D12IndexBuffer> IndexBuffer;
};
#include "pch.h"
#include "Mesh.h"

#include "Dx12/CommandList.h"
#include "Dx12/GraphicResourceTypes.h"

#include <DirectXMath.h>

using namespace Core;
using namespace DirectX;

const D3D12_INPUT_ELEMENT_DESC VertexPositionNormalTexture::InputElements[] =
{
    { "POSITION",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "NORMAL",     0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD",   0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

Core::Mesh::Mesh()
    : m_indexCount(0)
    , m_numVertices(0)
{
}

void Mesh::Draw(CommandList& commandList, uint32_t instanceCount, uint32_t firstInstance)
{
	commandList.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList.SetVertexBuffer(this->m_vertexBuffer);
	if (this->m_indexCount > 0)
	{
		commandList.SetIndexBuffer(this->m_indexBuffer);
		commandList.DrawIndexed(this->m_indexCount, instanceCount, 0, 0, firstInstance);
	}
	else
	{
		commandList.Draw(this->m_numVertices, instanceCount, 0, firstInstance);
	}
}


// Helper for flipping winding of geometric primitives for LH vs. RH coords
static void ReverseWinding(IndexList& indices, VertexList& vertices)
{
    assert((indices.size() % 3) == 0);
    for (auto it = indices.begin(); it != indices.end(); it += 3)
    {
        std::swap(*it, *(it + 2));
    }

    for (auto it = vertices.begin(); it != vertices.end(); ++it)
    {
        it->textureCoordinate.x = (1.f - it->textureCoordinate.x);
    }
}


void Core::Mesh::Initialize(
    std::shared_ptr<Dx12RenderDevice> renderDevice,
    CommandList& commandList,
    VertexList vertices,
    IndexList indices,
    bool rhcoords)
{
    if (vertices.size() >= USHRT_MAX)
    {
        LOG_CORE_ERROR("Too many vertices for 16-bit index buffers");
        throw std::exception();
    }

    if (!rhcoords)
    {
        ReverseWinding(indices, vertices);
    }

    this->m_indexCount = indices.size();
    this->m_numVertices = vertices.size();
    {
        BufferDesc desc;
        desc.BindFlags = BIND_VERTEX_BUFFER;
        desc.ElementByteStride = sizeof(VertexPositionNormalTexture);
        desc.NumElements = vertices.size();
        this->m_vertexBuffer = Dx12Buffer(renderDevice, desc);
    }

    {
        BufferDesc desc;
        desc.BindFlags = BIND_INDEX_BUFFER;
        desc.ElementByteStride = sizeof(uint16_t);
        desc.NumElements = indices.size();
        this->m_indexBuffer = Dx12Buffer(renderDevice, desc);
    }

    commandList.CopyBuffer<VertexPositionNormalTexture>(this->m_vertexBuffer, vertices);
    commandList.CopyBuffer(this->m_indexBuffer, indices);
}

std::unique_ptr<Mesh> Core::MeshPrefabs::CreateCube(
	std::shared_ptr<Dx12RenderDevice> renderDevice,
	CommandList& commandList,
    float size,
    bool rhcoords)
{

    // A cube has six faces, each one pointing in a different direction.
    const int FaceCount = 6;

    static const XMVECTORF32 faceNormals[FaceCount] =
    {
        { 0,  0,  1 },
        { 0,  0, -1 },
        { 1,  0,  0 },
        { -1,  0,  0 },
        { 0,  1,  0 },
        { 0, -1,  0 },
    };

    static const XMVECTORF32 textureCoordinates[4] =
    {
        { 1, 0 },
        { 1, 1 },
        { 0, 1 },
        { 0, 0 },
    };

    VertexList vertices;
    IndexList indices;

    size /= 2;

    for (int i = 0; i < FaceCount; i++)
    {
        XMVECTOR normal = faceNormals[i];

        // Get two vectors perpendicular both to the face normal and to each other.
        XMVECTOR basis = (i >= 4) ? g_XMIdentityR2 : g_XMIdentityR1;

        XMVECTOR side1 = XMVector3Cross(normal, basis);
        XMVECTOR side2 = XMVector3Cross(normal, side1);

        // Six indices (two triangles) per face.
        size_t vbase = vertices.size();
        indices.push_back(static_cast<uint16_t>(vbase + 0));
        indices.push_back(static_cast<uint16_t>(vbase + 1));
        indices.push_back(static_cast<uint16_t>(vbase + 2));

        indices.push_back(static_cast<uint16_t>(vbase + 0));
        indices.push_back(static_cast<uint16_t>(vbase + 2));
        indices.push_back(static_cast<uint16_t>(vbase + 3));

        // Four vertices per face.
        vertices.push_back(VertexPositionNormalTexture((normal - side1 - side2) * size, normal, textureCoordinates[0]));
        vertices.push_back(VertexPositionNormalTexture((normal - side1 + side2) * size, normal, textureCoordinates[1]));
        vertices.push_back(VertexPositionNormalTexture((normal + side1 + side2) * size, normal, textureCoordinates[2]));
        vertices.push_back(VertexPositionNormalTexture((normal + side1 - side2) * size, normal, textureCoordinates[3]));
    }

    std::unique_ptr<Mesh> mesh = std::make_unique<Mesh>();
    mesh->Initialize(renderDevice, commandList, vertices, indices, rhcoords);

    return mesh;
}

std::unique_ptr<Mesh> Core::MeshPrefabs::CreateSphere(
    std::shared_ptr<Dx12RenderDevice> renderDevice,
    CommandList& commandList,
    float diameter,
    size_t tessellation,
    bool rhcoords)
{
    if (tessellation < 3)
    {
        LOG_CORE_ERROR("tessellation parameter out of range");
        throw std::out_of_range("tessellation parameter out of range");
    }
        
    VertexList vertices;
    IndexList indices;

    float radius = diameter / 2.0f;
    size_t verticalSegments = tessellation;
    size_t horizontalSegments = tessellation * 2;


    // Create rings of vertices at progressively higher latitudes.
    for (size_t i = 0; i <= verticalSegments; i++)
    {
        float v = 1 - (float)i / verticalSegments;

        float latitude = (i * XM_PI / verticalSegments) - XM_PIDIV2;
        float dy, dxz;

        XMScalarSinCos(&dy, &dxz, latitude);

        // Create a single ring of vertices at this latitude.
        for (size_t j = 0; j <= horizontalSegments; j++)
        {
            float u = (float)j / horizontalSegments;

            float longitude = j * XM_2PI / horizontalSegments;
            float dx, dz;

            XMScalarSinCos(&dx, &dz, longitude);

            dx *= dxz;
            dz *= dxz;

            XMVECTOR normal = XMVectorSet(dx, dy, dz, 0);
            XMVECTOR textureCoordinate = XMVectorSet(u, v, 0, 0);

            vertices.push_back(VertexPositionNormalTexture(normal * radius, normal, textureCoordinate));
        }
    }

    // Fill the index buffer with triangles joining each pair of latitude rings.
    size_t stride = horizontalSegments + 1;
    for (size_t i = 0; i < verticalSegments; i++)
    {
        for (size_t j = 0; j <= horizontalSegments; j++)
        {
            size_t nextI = i + 1;
            size_t nextJ = (j + 1) % stride;

            indices.push_back(static_cast<uint16_t>(i * stride + j));
            indices.push_back(static_cast<uint16_t>(nextI * stride + j));
            indices.push_back(static_cast<uint16_t>(i * stride + nextJ));

            indices.push_back(static_cast<uint16_t>(i * stride + nextJ));
            indices.push_back(static_cast<uint16_t>(nextI * stride + j));
            indices.push_back(static_cast<uint16_t>(nextI * stride + nextJ));
        }
    }

    std::unique_ptr<Mesh> mesh = std::make_unique<Mesh>();
    mesh->Initialize(renderDevice, commandList, vertices, indices, rhcoords);

    return mesh;
}

std::unique_ptr<Mesh> Core::MeshPrefabs::CreatePlane(std::shared_ptr<Dx12RenderDevice> renderDevice, CommandList& commandList, float width, float height, bool rhcoords)
{
    VertexList vertices =
    {
        { XMFLOAT3(-0.5f * width, 0.0f,  0.5f * height), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) }, // 0
        { XMFLOAT3(0.5f * width, 0.0f,  0.5f * height), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) }, // 1
        { XMFLOAT3(0.5f * width, 0.0f, -0.5f * height), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) }, // 2
        { XMFLOAT3(-0.5f * width, 0.0f, -0.5f * height), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) }  // 3
    };

    IndexList indices =
    {
        0, 3, 1, 1, 3, 2
    };

    std::unique_ptr<Mesh> mesh = std::make_unique<Mesh>();
    mesh->Initialize(renderDevice, commandList, vertices, indices, rhcoords);

    return mesh;
}

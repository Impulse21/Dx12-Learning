#pragma once

#include "Dx12/GraphicResourceTypes.h"

#include "Drawable.h"
#include <DirectXMath.h>
namespace Core
{
	// Forward Declares
	class Dx12RenderDevice;

	// Vertex struct holding position, normal vector, and texture mapping information.
	struct VertexPositionNormalTexture
	{
		VertexPositionNormalTexture()
		{ }

		VertexPositionNormalTexture(
			DirectX::XMFLOAT3 const& position,
			DirectX::XMFLOAT3 const& normal,
			DirectX::XMFLOAT2 const& textureCoordinate)
			: position(position),
			normal(normal),
			textureCoordinate(textureCoordinate)
		{ }

		VertexPositionNormalTexture(
			DirectX::FXMVECTOR position,
			DirectX::FXMVECTOR normal,
			DirectX::FXMVECTOR textureCoordinate)
		{
			XMStoreFloat3(&this->position, position);
			XMStoreFloat3(&this->normal, normal);
			XMStoreFloat2(&this->textureCoordinate, textureCoordinate);
		}

		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 normal;
		DirectX::XMFLOAT2 textureCoordinate;

		static const int InputElementCount = 3;
		static const D3D12_INPUT_ELEMENT_DESC InputElements[InputElementCount];
	};

	using VertexList = std::vector<VertexPositionNormalTexture>;
	using IndexList = std::vector<uint16_t>;

	class Mesh : public IDrawable
	{
		friend class MeshPrefabs;

	public:
		Mesh();

		void Draw(
			CommandList& commandList, 
			uint32_t instanceCount = 1,
			uint32_t firstInstance = 0) override;

	private:
		void Initialize(
			std::shared_ptr<Dx12RenderDevice> renderDevice,
			CommandList& commandList,
			VertexList vertices,
			IndexList indices,
			bool rhcoords);

	private:
		uint32_t m_indexCount;
		uint32_t m_numVertices;

		Dx12Buffer m_vertexBuffer;
		Dx12Buffer m_indexBuffer;
	};

	class MeshPrefabs
	{
	public:
		static std::unique_ptr<Mesh> CreateCube(
			std::shared_ptr<Dx12RenderDevice> renderDevice,
			CommandList& commandList,
			float size = 1,
			bool rhcoords = false);

		static std::unique_ptr<Mesh> CreateSphere(
			std::shared_ptr<Dx12RenderDevice> renderDevice,
			CommandList& commandList,
			float diameter = 1,
			size_t tesselation = 16,
			bool rhcoords = false);

		static std::unique_ptr<Mesh> CreatePlane(
			std::shared_ptr<Dx12RenderDevice> renderDevice,
			CommandList& commandList,
			float width = 1,
			float height = 1,
			bool rhcoords = false);
	};
}


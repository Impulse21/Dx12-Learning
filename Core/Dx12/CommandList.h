#pragma once

#include "pch.h"

#include <wrl.h>
#include <memory>
#include <vector>

#include "d3dx12.h"

#include "Dx12/DynamicDescriptorHeap.h"
#include "Dx12/UploadBuffer.h"
#include "Dx12/GraphicResourceTypes.h"
#include "Dx12/RootSignature.h"
#include "RenderTarget.h"

namespace Core
{
	class ResourceStateTracker;
	class Dx12RenderDevice;

	class CommandList
	{
	public:
		CommandList(
			D3D12_COMMAND_LIST_TYPE type,
			std::shared_ptr<Dx12RenderDevice> renderDevice,
			Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
			ID3D12CommandAllocator* allocator);

		void Reset();

		bool Close(CommandList& pendingCommandList);
		void Close();

		ID3D12GraphicsCommandList2* GetD3D12Impl() { return this->m_commandList.Get(); }

		void FlushResourceBarriers();
		void TransitionBarrier(
			Microsoft::WRL::ComPtr<ID3D12Resource> resource, 
			D3D12_RESOURCE_STATES stateAfter, 
			uint32_t subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
			bool flushBarriers = false);

	public:
		void LoadTextureFromFile(Dx12Texture& texture, std::wstring const& filename);

		void GenerateMips(Dx12Texture& texture);

		void CopyTextureSubresource(
			Dx12Texture& texture,
			uint32_t firstSubResource,
			uint32_t numSubresources,
			D3D12_SUBRESOURCE_DATA* subresourceData);

		void CopyBuffer(
			Microsoft::WRL::ComPtr<ID3D12Resource>& buffer,
			size_t numOfElements,
			size_t elementStride,
			const void* data,
			D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

		template<typename T>
		void CopyBuffer(Dx12Buffer& buffer, const std::vector<T>& bufferData)
		{
			LOG_CORE_ASSERT(buffer.GetNumElements() == bufferData.size(), "Invalid Buffer Data size");
			LOG_CORE_ASSERT(buffer.GetElementByteStride() == sizeof(T), "Invalid Buffer Element Stride");

			Microsoft::WRL::ComPtr<ID3D12Resource> resource;
			this->CopyBuffer(
				resource,
				buffer.GetSizeInBytes(),
				buffer.GetElementByteStride(),
				bufferData.data());

			buffer.SetDx12Resource(resource);
		}

		void CommandList::CopyResource(
			Microsoft::WRL::ComPtr<ID3D12Resource> dstRes,
			Microsoft::WRL::ComPtr<ID3D12Resource> srcRes);

		void CommandList::CopyResource(Dx12Resrouce const& dstRes, Dx12Resrouce const& srcRes)
		{
			this->CopyResource(dstRes.GetDx12Resource(), srcRes.GetDx12Resource());
		}

		void CommandList::ResolveSubresource(
			Dx12Resrouce const& dstRes,
			Dx12Resrouce const& srcRes,
			uint32_t dstSubresource = 0,
			uint32_t srcSubresource = 0);

		/**
		* Clear depth/stencil texture.
		*/
		void ClearDepthStencilTexture(
			Dx12Texture const& texture,
			D3D12_CLEAR_FLAGS clearFlags,
			float depth = 1.0f,
			uint8_t stencil = 0);

		void ClearRenderTarget(
			Dx12Texture const& texture,
			std::array<FLOAT, 4> clearColour);

		void ClearRenderTarget(
			Microsoft::WRL::ComPtr<ID3D12Resource> resource,
			D3D12_CPU_DESCRIPTOR_HANDLE rtv,
			std::array<FLOAT, 4> clearColour);

		void SetViewport(CD3DX12_VIEWPORT const& viewport);
		void SetViewports(std::vector<CD3DX12_VIEWPORT> const& viewports);

		void SetScissorRect(CD3DX12_RECT const& rect);
		void SetScissorRects(std::vector<CD3DX12_RECT> const& rects);

		void SetRenderTarget(RenderTarget const& renderTarget);

		void SetRenderTarget(
			Microsoft::WRL::ComPtr<ID3D12Resource> resource,
			D3D12_CPU_DESCRIPTOR_HANDLE& rtv);

		/**
		 * Set a set of 32-bit constants on the graphics pipeline.
		 */
		void SetGraphics32BitConstants(uint32_t rootParameterIndex, uint32_t numConstants, const void* constants);
		template<typename T>
		void SetGraphics32BitConstants(uint32_t rootParameterIndex, const T& constants)
		{
			static_assert(sizeof(T) % sizeof(uint32_t) == 0, "Size of type must be a multiple of 4 bytes");
			this->SetGraphics32BitConstants(rootParameterIndex, sizeof(T) / sizeof(uint32_t), &constants);
		}

		void SetGraphicsRootShaderResourceView(uint32_t rootParameterIndex, Microsoft::WRL::ComPtr<ID3D12Resource> resource);
		void SetGraphicsRootSignature(Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature);

		void SetShaderResourceView(
			uint32_t rootParameterIndex,
			uint32_t descritporOffset,
			Dx12Resrouce const& resource,
			D3D12_RESOURCE_STATES stateAfter = 
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			UINT firstSubresource = 0,
			UINT numSubresources = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
			const D3D12_SHADER_RESOURCE_VIEW_DESC* srv = nullptr);

		void SetGraphicsRootSignature(RootSignature const& rootSignature);

		void SetPipelineState(Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState);

		void SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY topology);

		void SetVertexBuffer(Dx12Buffer& vertexBuffer);
		void SetVertexBuffer(Microsoft::WRL::ComPtr<ID3D12Resource> resource, D3D12_VERTEX_BUFFER_VIEW& vertexView);
		/**
		 * Set dynamic vertex buffer data to the rendering pipeline.
		 */
		void SetDynamicVertexBuffer(uint32_t slot, size_t numVertices, size_t vertexSize, const void* vertexBufferData);
		template<typename T>
		void SetDynamicVertexBuffer(uint32_t slot, const std::vector<T>& vertexBufferData)
		{
			this->SetDynamicVertexBuffer(slot, vertexBufferData.size(), sizeof(T), vertexBufferData.data());
		}

		void SetIndexBuffer(Dx12Buffer& indexBuffer);
		void SetIndexBuffer(Microsoft::WRL::ComPtr<ID3D12Resource> resource, D3D12_INDEX_BUFFER_VIEW& indexView);

		/**
		 * Bind dynamic index buffer data to the rendering pipeline.
		 */
		void SetDynamicIndexBuffer(size_t numIndicies, DXGI_FORMAT indexFormat, const void* indexBufferData);
		template<typename T>
		void SetDynamicIndexBuffer(const std::vector<T>& indexBufferData)
		{
			static_assert(sizeof(T) == 2 || sizeof(T) == 4);

			DXGI_FORMAT indexFormat = (sizeof(T) == 2) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
			SetDynamicIndexBuffer(indexBufferData.size(), indexFormat, indexBufferData.data());
		}

		void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t startVertex = 0, uint32_t startInstance = 0);
		void DrawIndexed(
			uint32_t indexCount,
			uint32_t instanceCount = 1,
			uint32_t startIndex = 0,
			int32_t baseVertex = 0,
			uint32_t startInstance = 0);

		void SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, ID3D12DescriptorHeap* heap);

	private:
		void TrackResource(Microsoft::WRL::ComPtr<ID3D12Object> object);
		void BindDescriptorHeaps();

	private:
		D3D12_COMMAND_LIST_TYPE m_type;
		std::shared_ptr<Dx12RenderDevice> m_renderDevice;

		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> m_commandList;
		std::shared_ptr<CommandList> m_computeCommandList;
		ID3D12CommandAllocator* m_allocator;

		using TrackedObjects = std::vector<Microsoft::WRL::ComPtr<ID3D12Object>>;
		TrackedObjects m_trackedObjects;

		std::unique_ptr<ResourceStateTracker> m_resourceStateTracker;
		std::unique_ptr<UploadBuffer> m_uploadBuffer;

		std::unique_ptr<DynamicDescriptorHeap> m_dynamicDescriptorHeap[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

		// Keep track of the currently bound descriptor heaps. Only change descriptor 
		// heaps if they are different than the currently bound descriptor heaps.
		ID3D12DescriptorHeap* m_descriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

		// Keep track of the currently bound root signatures to minimize root
		// signature changes.
		ID3D12RootSignature* m_rootSignature;

	private:
		static std::map<std::wstring, ID3D12Resource*> ms_textureCache;
		static std::mutex ms_textureCacheMutex;
	};
}
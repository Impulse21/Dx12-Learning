#pragma once

#include <wrl.h>
#include <memory>
#include <vector>

#include "d3dx12.h"

namespace Core
{
	class ResourceStateTracker;

	class CommandList
	{
	public:
		CommandList(
			Microsoft::WRL::ComPtr<ID3D12Device2> device,
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
		void CopyBuffer(
			Microsoft::WRL::ComPtr<ID3D12Resource>& buffer,
			size_t numOfElements,
			size_t elementStride,
			const void* data,
			D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

		void ClearRenderTarget(
			Microsoft::WRL::ComPtr<ID3D12Resource> resource,
			D3D12_CPU_DESCRIPTOR_HANDLE rtv,
			std::array<FLOAT, 4> clearColour);

		void SetViewport(CD3DX12_VIEWPORT const& viewport);
		void SetViewports(std::vector<CD3DX12_VIEWPORT> const& viewports);

		void SetScissorRect(CD3DX12_RECT const& rect);
		void SetScissorRects(std::vector<CD3DX12_RECT> const& rects);

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
		void SetPipelineState(Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState);

		void SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY topology);
		void SetVertexBuffer(Microsoft::WRL::ComPtr<ID3D12Resource> resource, D3D12_VERTEX_BUFFER_VIEW& vertexView);
		void SetIndexBuffer(Microsoft::WRL::ComPtr<ID3D12Resource> resource, D3D12_INDEX_BUFFER_VIEW& indexView);

		void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t startVertex = 0, uint32_t startInstance = 0);
		void DrawIndexed(
			uint32_t indexCount,
			uint32_t instanceCount = 1,
			uint32_t startIndex = 0,
			int32_t baseVertex = 0,
			uint32_t startInstance = 0);

	private:
		void TrackResource(Microsoft::WRL::ComPtr<ID3D12Object> object);

	private:
		using TrackedObjects = std::vector<Microsoft::WRL::ComPtr<ID3D12Object>>;
		TrackedObjects m_trackedObjects;

		std::unique_ptr<ResourceStateTracker> m_resourceStateTracker;

		Microsoft::WRL::ComPtr<ID3D12Device2> m_d3d12Device;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> m_commandList;
		ID3D12CommandAllocator* m_allocator;

		// Store Command Allocator
	};
}
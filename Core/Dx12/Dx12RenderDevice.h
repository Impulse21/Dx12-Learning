#pragma once

// DirectX 12 specific headers.
#include <d3d12.h>
#include <dxgi1_6.h>

// D3D12 extension library.
#include "Dx12/d3dx12.h"

#ifdef _DEBUG
#include <dxgidebug.h>
#endif

// -- STL ---
#include <memory>

#include "Dx12/CommandQueue.h"

#include "DescriptorAllocation.h"
#include "DescriptorAllocator.h"

namespace Core
{
	class DescriptorHeap;

	class Dx12RenderDevice : public std::enable_shared_from_this<Dx12RenderDevice>
	{
	public:
		Dx12RenderDevice();

		void Initialize();
		void Initialize(Microsoft::WRL::ComPtr<IDXGIFactory6> dxgiFactory);

		Microsoft::WRL::ComPtr<ID3D12Device2> GetD3DDevice() { return this->m_d3d12Device;  }

		std::shared_ptr<CommandQueue> GetQueue(
			D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT);

		void Flush();

		DescriptorAllocation AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescritpors = 1);
		void ReleaseStaleDescriptors(uint64_t finishedFrame);

		std::unique_ptr<DescriptorHeap> CreateDescriptorHeap(UINT numDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE type);

		UINT GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE type) const;

		void UploadBufferResource(
			ID3D12GraphicsCommandList2* commandList,
			ID3D12Resource** pDestinationResource,
			ID3D12Resource** pIntermediateResource,
			size_t numOfElements,
			size_t elementStride,
			const void* data,
			D3D12_RESOURCE_FLAGS flags);

	private:

		void CreateDevice(Microsoft::WRL::ComPtr<IDXGIFactory6> dxgiFactory);
		void EnableDebugLayer();
		Microsoft::WRL::ComPtr<IDXGIAdapter1> FindCompatibleAdapter(
			Microsoft::WRL::ComPtr<IDXGIFactory6> dxgiFactory);

		void CreateCommandQueues();

	private:
		// -- Dx12 resources ---
		Microsoft::WRL::ComPtr<IDXGIAdapter1> m_dxgiAdapter;
		Microsoft::WRL::ComPtr<ID3D12Device2> m_d3d12Device;

		// -- Queues ---
		std::shared_ptr<CommandQueue> m_directQueue;
		std::shared_ptr<CommandQueue> m_computeQueue;
		std::shared_ptr<CommandQueue> m_copyQueue;

		// -- Heaps ---
		std::unique_ptr<DescriptorAllocator> m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
	};
}


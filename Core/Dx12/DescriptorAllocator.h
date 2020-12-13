#pragma once

#include "d3dx12.h"
#include <vector>
#include <memory>

#include <set>
#include <mutex>

namespace Core
{
	class DescriptorAllocatorPage;
	class DescriptorAllocation;

	class DescriptorAllocator
	{
	public:
		DescriptorAllocator(
			Microsoft::WRL::ComPtr<ID3D12Device2> device,
			D3D12_DESCRIPTOR_HEAP_TYPE heapType,
			uint32_t numDescriptorsPerHeap = 256);

		DescriptorAllocation Allocate(uint32_t numDescriptors);
		DescriptorAllocation Allocate();

		void ReleaseStaleDescriptors(uint64_t frameNumber);

	private:
		std::shared_ptr<DescriptorAllocatorPage> CreateAllocatorPage();

	private:
		Microsoft::WRL::ComPtr<ID3D12Device2> m_device;
		const D3D12_DESCRIPTOR_HEAP_TYPE m_heapType;
		uint32_t m_numDescriptorsPerHeap;

		using DescriptorHeapPool = std::vector<std::shared_ptr<DescriptorAllocatorPage>>;
		DescriptorHeapPool m_heapPool;

		std::set<size_t> m_availableHeaps;

		std::mutex m_allocationMutex;
	};
}

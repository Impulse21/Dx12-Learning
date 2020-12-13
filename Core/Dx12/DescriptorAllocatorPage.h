#pragma once

#include "d3dx12.h"

#include <wrl.h>

#include <map>
#include <memory>
#include <mutex>
#include <queue>

#include "DescriptorAllocation.h"

namespace Core
{
	class DescriptorAllocatorPage : public std::enable_shared_from_this<DescriptorAllocatorPage>
	{
	public:
		DescriptorAllocatorPage(
			D3D12_DESCRIPTOR_HEAP_TYPE type,
			uint32_t numDescriptors,
			Microsoft::WRL::ComPtr<ID3D12Device2> device);

		D3D12_DESCRIPTOR_HEAP_TYPE GetHeapType() const { return this->m_heapType; }

		bool HasSpace(uint32_t numDescriptors) const 
		{ 
			return this->m_freeListBySize.lower_bound(numDescriptors) != this->m_freeListBySize.end();
		}

		uint32_t NumFreeHandles() const { return this->m_numFreeHandles; }

		DescriptorAllocation Allocate(uint32_t numDescriptors);

		void Free(DescriptorAllocation&& descriptor, uint64_t frameNumber);

		void ReleaseStaleDescriptors(uint64_t frameNumber);

	protected:
		uint32_t ComputeOffset(D3D12_CPU_DESCRIPTOR_HANDLE handle);

		void AddNewBlock(uint32_t offset, uint32_t numDescritpors);

		void FreeBlock(uint32_t offset, uint32_t numDescriptors);

	private:
		// The offset in descritpors within the descriptor heap
		using OffsetType = uint32_t;
		using SizeType = uint32_t;
		
		struct FreeBlockInfo;
		using FreeListByOffset = std::map<OffsetType, FreeBlockInfo>;
		using FreeListBySize = std::multimap<SizeType, FreeListByOffset::iterator>;

		struct FreeBlockInfo
		{
			FreeBlockInfo(SizeType size)
				: Size(size)
			{}

			SizeType Size;
			FreeListBySize::iterator FreeListBySizeIter;
		};

		struct StaleDescriptorInfo
		{
			StaleDescriptorInfo(OffsetType offset, SizeType size, uint64_t frameNumber)
				: Offset(offset)
				, Size(size)
				, FrameNumber(frameNumber)
			{}

			OffsetType Offset;
			SizeType Size;
			uint64_t FrameNumber;
		};

		// Stale descriptors are queued for release until the frame that they were freed
		// has completed.
		using StaleDescriptorQueue = std::queue<StaleDescriptorInfo>;


	private:
		FreeListByOffset m_freeListByOffset;
		FreeListBySize m_freeListBySize;
		StaleDescriptorQueue m_staleDescriptors;

		const D3D12_DESCRIPTOR_HEAP_TYPE m_heapType;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_d3d12DescriptorHeap;
		CD3DX12_CPU_DESCRIPTOR_HANDLE m_baseDescriptor;
		uint32_t m_descriptorHandleIncrementSize;
		uint32_t m_numDescriptorsInHeap;
		uint32_t m_numFreeHandles;

		std::mutex m_allocationMutex;
	};
}


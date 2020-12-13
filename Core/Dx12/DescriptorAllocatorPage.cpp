#include "pch.h"
#include "DescriptorAllocatorPage.h"

using namespace Core;

Core::DescriptorAllocatorPage::DescriptorAllocatorPage(
	D3D12_DESCRIPTOR_HEAP_TYPE type,
	uint32_t numDescriptors,
	Microsoft::WRL::ComPtr<ID3D12Device2> device)
	: m_heapType(type)
	, m_numDescriptorsInHeap(numDescriptors)
{

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = m_heapType;
	heapDesc.NumDescriptors = m_numDescriptorsInHeap;

	ThrowIfFailed(
		device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_d3d12DescriptorHeap)));

	this->m_baseDescriptor = m_d3d12DescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	this->m_descriptorHandleIncrementSize = device->GetDescriptorHandleIncrementSize(m_heapType);
	this->m_numFreeHandles = m_numDescriptorsInHeap;

	// Initialize the free lists
	this->AddNewBlock(0, m_numFreeHandles);
}

DescriptorAllocation Core::DescriptorAllocatorPage::Allocate(uint32_t numDescriptors)
{
	std::lock_guard<std::mutex> lock(this->m_allocationMutex);

	// There are less than the requested number of descriptors left in the heap.
	// Return a NULL descriptor and try another heap.
	if (numDescriptors > m_numFreeHandles)
	{
		return DescriptorAllocation();
	}

	// Get the first block that is large enough to satisfy the request.
	auto smallestBlockIt = this->m_freeListBySize.lower_bound(numDescriptors);
	if (smallestBlockIt == this->m_freeListBySize.end())
	{
		// There was no free block that could satisfy the request.
		return DescriptorAllocation();
	}

	// The size of the smallest block that satisfies the request.
	auto blockSize = smallestBlockIt->first;

	// The pointer to the same entry in the FreeListByOffset map.
	auto offsetIt = smallestBlockIt->second;

	// The offset in the descriptor heap.
	auto offset = offsetIt->first;

	// Remove the existing free block from the free list.
	m_freeListBySize.erase(smallestBlockIt);
	m_freeListByOffset.erase(offsetIt);

	// Compute the new free block that results from splitting this block.
	auto newOffset = offset + numDescriptors;
	auto newSize = blockSize - numDescriptors;

	if (newSize > 0)
	{
		// If the allocation didn't exactly match the requested size,
		// return the left-over to the free list.
		this->AddNewBlock(newOffset, newSize);
	}
	// Decrement free handles.
	this->m_numFreeHandles -= numDescriptors;

	return DescriptorAllocation(
		CD3DX12_CPU_DESCRIPTOR_HANDLE(
			this->m_baseDescriptor,
			offset,
			this->m_descriptorHandleIncrementSize),
		numDescriptors, 
		this->m_descriptorHandleIncrementSize,
		shared_from_this());
}

void Core::DescriptorAllocatorPage::Free(DescriptorAllocation&& descriptor, uint64_t frameNumber)
{
	// Compute the offset of the descriptor within the descriptor heap.
	auto offset = ComputeOffset(descriptor.GetDescriptorHandle());

	std::lock_guard<std::mutex> lock(this->m_allocationMutex);

	// Don't add the block directly to the free list until the frame has completed.
	this->m_staleDescriptors.emplace(offset, descriptor.GetNumHandles(), frameNumber);
}

void Core::DescriptorAllocatorPage::ReleaseStaleDescriptors(uint64_t frameNumber)
{
	std::lock_guard<std::mutex> lock(this->m_allocationMutex);

	while (!this->m_staleDescriptors.empty() && this->m_staleDescriptors.front().FrameNumber <= frameNumber)
	{
		auto& staleDescriptor = m_staleDescriptors.front();

		// The offset of the descriptor in the heap.
		auto offset = staleDescriptor.Offset;
		// The number of descriptors that were allocated.
		auto numDescriptors = staleDescriptor.Size;

		FreeBlock(offset, numDescriptors);

		this->m_staleDescriptors.pop();
	}
}

uint32_t Core::DescriptorAllocatorPage::ComputeOffset(D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
	return static_cast<uint32_t>(handle.ptr - this->m_baseDescriptor.ptr) / this->m_descriptorHandleIncrementSize;
}

void Core::DescriptorAllocatorPage::AddNewBlock(uint32_t offset, uint32_t numDescritpors)
{
	auto offsetItr = this->m_freeListByOffset.emplace(offset, numDescritpors);
	auto sizeItr = this->m_freeListBySize.emplace(numDescritpors, offsetItr.first);
	offsetItr.first->second.FreeListBySizeIter = sizeItr;
}

void Core::DescriptorAllocatorPage::FreeBlock(uint32_t offset, uint32_t numDescriptors)
{
	// Find the first element whose offset is greater than the specified offset.
	// This is the block that should appear after the block that is being freed.
	auto nextBlockIt = this->m_freeListByOffset.upper_bound(offset);

	// Find the block that appears before the block being freed.
	auto prevBlockIt = nextBlockIt;

	// If it's not the first block in the list.
	if (prevBlockIt != this->m_freeListByOffset.begin())
	{
		// Go to the previous block in the list.
		--prevBlockIt;
	}
	else
	{
		// Otherwise, just set it to the end of the list to indicate that no
		// block comes before the one being freed.
		prevBlockIt = this->m_freeListByOffset.end();
	}

	// Add the number of free handles back to the heap.
	// This needs to be done before merging any blocks since merging
	// blocks modifies the numDescriptors variable.
	this->m_numFreeHandles += numDescriptors;

	if (prevBlockIt != this->m_freeListByOffset.end() &&
		offset == prevBlockIt->first + prevBlockIt->second.Size)
	{
		// The previous block is exactly behind the block that is to be freed.
		//
		// PrevBlock.Offset           Offset
		// |                          |
		// |<-----PrevBlock.Size----->|<------Size-------->|
		//

		// Increase the block size by the size of merging with the previous block.
		offset = prevBlockIt->first;
		numDescriptors += prevBlockIt->second.Size;

		// Remove the previous block from the free list.
		this->m_freeListBySize.erase(prevBlockIt->second.FreeListBySizeIter);
		this->m_freeListByOffset.erase(prevBlockIt);
	}

	if (nextBlockIt != this->m_freeListByOffset.end() &&
		offset + numDescriptors == nextBlockIt->first)
	{
		// The next block is exactly in front of the block that is to be freed.
		//
		// Offset               NextBlock.Offset 
		// |                    |
		// |<------Size-------->|<-----NextBlock.Size----->|

		// Increase the block size by the size of merging with the next block.
		numDescriptors += nextBlockIt->second.Size;

		// Remove the next block from the free list.
		this->m_freeListBySize.erase(nextBlockIt->second.FreeListBySizeIter);
		this->m_freeListByOffset.erase(nextBlockIt);
	}

	this->AddNewBlock(offset, numDescriptors);
}

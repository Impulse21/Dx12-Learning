#include "pch.h"
#include "Dx12\DescriptorAllocator.h"

#include "Dx12\DescriptorAllocatorPage.h"
using namespace Core;

DescriptorAllocator::DescriptorAllocator(
	Microsoft::WRL::ComPtr<ID3D12Device2> device,
	D3D12_DESCRIPTOR_HEAP_TYPE heapType,
	uint32_t numDescriptorsPerHeap)
	: m_device(device)
	, m_heapType(heapType)
	, m_numDescriptorsPerHeap(numDescriptorsPerHeap)
{
}

DescriptorAllocation Core::DescriptorAllocator::Allocate(uint32_t numDescriptors)
{
	std::lock_guard<std::mutex> lock(this->m_allocationMutex);

	DescriptorAllocation allocation;

	for (auto iter = m_availableHeaps.begin(); iter != this->m_availableHeaps.end(); ++iter)
	{
		auto allocatorPage = this->m_heapPool[*iter];
		
		allocation = allocatorPage->Allocate(numDescriptors);
		
		// Allocator page is full
		if (allocatorPage->NumFreeHandles() == 0)
		{
			iter = this->m_availableHeaps.erase(iter);
		}

		// A valid allocation has been found.
		if (!allocation.IsNull())
		{
			break;
		}
	}

	if (allocation.IsNull())
	{
		this->m_numDescriptorsPerHeap = std::max(m_numDescriptorsPerHeap, numDescriptors);
		auto newPage = this->CreateAllocatorPage();

		allocation = newPage->Allocate(numDescriptors);
	}

	return allocation;
}

DescriptorAllocation Core::DescriptorAllocator::Allocate()
{
	return this->Allocate(1);
}

void Core::DescriptorAllocator::ReleaseStaleDescriptors(uint64_t frameNumber)
{
	std::lock_guard<std::mutex> lock(this->m_allocationMutex); 
	
	for (size_t i = 0; i < m_heapPool.size(); ++i)
	{
		auto page = m_heapPool[i];

		page->ReleaseStaleDescriptors(frameNumber);

		if (page->NumFreeHandles() > 0)
		{
			m_availableHeaps.insert(i);
		}
	}
}

std::shared_ptr<DescriptorAllocatorPage> Core::DescriptorAllocator::CreateAllocatorPage()
{
	auto newPage =
		std::make_shared<DescriptorAllocatorPage>(
			this->m_heapType,
			this->m_numDescriptorsPerHeap,
			this->m_device);

	this->m_heapPool.emplace_back(newPage);
	this->m_availableHeaps.insert(this->m_heapPool.size() - 1);

	return newPage;
}

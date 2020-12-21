#include "pch.h"
#include "DescriptorAllocation.h"

#include "DescriptorAllocatorPage.h"

using namespace Core;

DescriptorAllocation::DescriptorAllocation()
    : m_descriptor{ 0 }
    , m_numHandles(0)
    , m_descriptorSize(0)
    , m_page(nullptr)
{}

Core::DescriptorAllocation::DescriptorAllocation(
    D3D12_CPU_DESCRIPTOR_HANDLE descriptor, 
    uint32_t numHandles,
    uint32_t descriptorSize,
    std::shared_ptr<DescriptorAllocatorPage> page)
    : m_descriptor(descriptor)
    , m_numHandles(numHandles)
    , m_descriptorSize(descriptorSize)
    , m_page(page)
{}

Core::DescriptorAllocation::~DescriptorAllocation()
{
    this->Free();
}

Core::DescriptorAllocation::DescriptorAllocation(DescriptorAllocation&& allocation)
    : m_descriptor(allocation.m_descriptor)
    , m_numHandles(allocation.m_numHandles)
    , m_descriptorSize(allocation.m_descriptorSize)
    , m_page(std::move(allocation.m_page))
{
    allocation.m_descriptor.ptr = 0;
    allocation.m_numHandles = 0;
    allocation.m_descriptorSize = 0;
}

DescriptorAllocation& Core::DescriptorAllocation::operator=(DescriptorAllocation&& other)
{
    // Free this descriptor if it points to anything.
    Free();

    this->m_descriptor = other.m_descriptor;
    this->m_numHandles = other.m_numHandles;
    this->m_descriptorSize = other.m_descriptorSize;
    this->m_page = std::move(other.m_page);

    other.m_descriptor.ptr = 0;
    other.m_numHandles = 0;
    other.m_descriptorSize = 0;

    return *this;
}

D3D12_CPU_DESCRIPTOR_HANDLE Core::DescriptorAllocation::GetDescriptorHandle(uint32_t offset) const
{
    LOG_CORE_ASSERT(offset < this->m_numHandles, "Offset out of range");
    return { this->m_descriptor.ptr + (this->m_descriptorSize * offset) };
}

void Core::DescriptorAllocation::Free()
{
    if (!this->IsNull() && this->m_page)
    {
        // TODO: GetFrameCount some how?
        this->m_page->Free(std::move(*this), 1);

        this->m_descriptor.ptr = 0;
        this->m_numHandles = 0;
        this->m_descriptorSize = 0;
        this->m_page.reset();
    }
}

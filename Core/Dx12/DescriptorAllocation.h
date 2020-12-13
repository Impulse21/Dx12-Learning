#pragma once

#include <d3d12.h>

#include <cstdint>
#include <memory>

namespace Core
{
    class DescriptorAllocatorPage;

	class DescriptorAllocation
	{
    public:
        // Creates a NULL descriptor.
        DescriptorAllocation();

        DescriptorAllocation(
            D3D12_CPU_DESCRIPTOR_HANDLE descriptor,
            uint32_t numHandles,
            uint32_t descriptorSize,
            std::shared_ptr<DescriptorAllocatorPage> page);

        // The destructor will automatically free the allocation.
        ~DescriptorAllocation();

        // Copies are not allowed.
        DescriptorAllocation(const DescriptorAllocation&) = delete;
        DescriptorAllocation& operator=(const DescriptorAllocation&) = delete;

        // Move is allowed.
        DescriptorAllocation(DescriptorAllocation&& allocation);
        DescriptorAllocation& operator=(DescriptorAllocation&& other);

        // Check if this a valid descriptor.
        bool IsNull() const { return this->m_descriptor.ptr == 0; }

        // Get a descriptor at a particular offset in the allocation.
        D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHandle(uint32_t offset = 0) const
        {
            LOG_CORE_ASSERT(offset < this->m_numHandles, "Offset out of range");
            return { this->m_descriptor.ptr + (this->m_descriptorSize * offset) };
        }

        // Get the number of (consecutive) handles for this allocation.
        uint32_t GetNumHandles() const { return this->m_numHandles; }

        // Get the heap that this allocation came from.
        // (For internal use only).
        std::shared_ptr<DescriptorAllocatorPage> GetDescriptorAllocatorPage() const { this->m_page; }

    private:
        // Free the descriptor back to the heap it came from.
        void Free();

    private:
        // The base descriptor.
        D3D12_CPU_DESCRIPTOR_HANDLE m_descriptor;

        // The number of descriptors in this allocation.
        uint32_t m_numHandles;

        // The offset to the next descriptor.
        uint32_t m_descriptorSize;

        // A pointer back to the original page where this allocation came from.
        std::shared_ptr<DescriptorAllocatorPage> m_page;
	};
}


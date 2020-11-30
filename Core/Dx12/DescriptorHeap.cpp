#include "pch.h"

#include "DescriptorHeap.h"
#include <stdexcept>      // std::out_of_range


using namespace Core;

Core::DescriptorHeap::DescriptorHeap(
    Microsoft::WRL::ComPtr<ID3D12Device2> device,
    D3D12_DESCRIPTOR_HEAP_TYPE type,
    size_t numDescriptors,
    D3D12_DESCRIPTOR_HEAP_FLAGS flags)
    : m_heapType(type)
    , m_heapFlags(flags)
    , m_descriptorSize(device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV))
    , m_numDescriptors(numDescriptors)
{
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.NumDescriptors = numDescriptors;
    desc.Type = type;
    desc.Flags = flags;

    ThrowIfFailed(
        device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&this->m_descriptorHeap)));
}

D3D12_CPU_DESCRIPTOR_HANDLE Core::DescriptorHeap::GetCpuHandle(size_t heapIndex) const
{
    assert(this->m_descriptorHeap != nullptr);
    if (heapIndex >= this->m_numDescriptors)
    {
        throw std::out_of_range("D3DX12_GPU_DESCRIPTOR_HANDLE");
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(this->m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
    handle.Offset(heapIndex, this->m_descriptorSize);

    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE Core::DescriptorHeap::GetGpuHandle(size_t heapIndex) const
{
    assert(this->m_descriptorHeap != nullptr);
    if (heapIndex >= this->m_numDescriptors)
    {
        throw std::out_of_range("D3DX12_GPU_DESCRIPTOR_HANDLE");
    }

    CD3DX12_GPU_DESCRIPTOR_HANDLE handle(this->m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());
    handle.Offset(heapIndex, this->m_descriptorSize);

    return handle;
}

Core::DescriptorPile::DescriptorPile(
    Microsoft::WRL::ComPtr<ID3D12Device2> device,
    D3D12_DESCRIPTOR_HEAP_TYPE type,
    size_t capacity,
    size_t reserve,
    D3D12_DESCRIPTOR_HEAP_FLAGS flags) noexcept(false)
    : DescriptorHeap(device, type, capacity, flags)
    , m_top(reserve)
{
    if (reserve > 0 && this->m_top >= this->Count())
    {
        throw std::out_of_range("Reserve descriptor range is too large");
    }
}

void Core::DescriptorPile::AllocateRange(size_t numDescriptors, IndexType& outStart, IndexType& outEnd)
{
    if (numDescriptors == 0)
    {
        throw std::out_of_range("Can't allocate a zero descriptor");
    }

    outStart = this->m_top;

    this->m_top += numDescriptors;
    outEnd = this->m_top;


    // Ensure there is enough room
    if (this->m_top > this->Count())
    {
        throw std::out_of_range("Can't allocate more descriptors");
    }
}

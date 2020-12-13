#include "pch.h"
#include "DynamicDescriptorHeap.h"

#include "CommandList.h"

Core::DynamicDescriptorHeap::DynamicDescriptorHeap(
	D3D12_DESCRIPTOR_HEAP_TYPE heapType,
    Microsoft::WRL::ComPtr<ID3D12Device> device,
	uint32_t numDescriptorsPerHeap) 
    : m_descriptorHeapType(heapType)
    , m_device(device)
    , m_numDescriptorsPerHeap(numDescriptorsPerHeap)
    , m_descriptorTableBitMask(0)
    , m_staleDescriptorTableBitMask(0)
    , m_currentCPUDescriptorHandle(D3D12_DEFAULT)
    , m_currentGPUDescriptorHandle(D3D12_DEFAULT)
    , m_numFreeHandles(0)
{
    // TODO:
    m_descriptorHandleIncrementSize = this->m_device->GetDescriptorHandleIncrementSize(this->m_descriptorHeapType);

    // Allocate space for staging CPU visible descriptors.
    this->m_descriptorHandleCache = std::make_unique<D3D12_CPU_DESCRIPTOR_HANDLE[]>(this->m_numDescriptorsPerHeap);
}

Core::DynamicDescriptorHeap::~DynamicDescriptorHeap()
{
    this->Reset();
}

void Core::DynamicDescriptorHeap::Reset()
{
    this->m_availableDescriptorHeaps = this->m_descriptorHeapPool;
    this->m_currentDescriptorHeap.Reset();
    this->m_currentCPUDescriptorHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
    this->m_currentGPUDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
    this->m_numFreeHandles = 0;
    this->m_descriptorTableBitMask = 0;
    this->m_staleDescriptorTableBitMask = 0;

    // Reset the table cache
    for (int i = 0; i < MaxDescriptorTables; ++i)
    {
        this->m_descriptorTableCache[i].Reset();
    }
}

void Core::DynamicDescriptorHeap::StageDescriptors(
    uint32_t rootParameterIndex,
    uint32_t offset,
    uint32_t numDescriptors,
    const D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptors)
{
    // Cannot stage more than the maximum number of descriptors per heap.
    // Cannot stage more than MaxDescriptorTables root parameters.
    if (numDescriptors > this->m_numDescriptorsPerHeap || rootParameterIndex >= MaxDescriptorTables)
    {
        throw std::bad_alloc();
    }

    DescriptorTableCache& descriptorTableCache = this->m_descriptorTableCache[rootParameterIndex];

    // Check that the number of descriptors to copy does not exceed the number
    // of descriptors expected in the descriptor table.
    if ((offset + numDescriptors) > descriptorTableCache.NumDescriptors)
    {
        throw std::length_error("Number of descriptors exceeds the number of descriptors in the descriptor table.");
    }

    D3D12_CPU_DESCRIPTOR_HANDLE* dstDescriptor = (descriptorTableCache.BaseDescriptor + offset);
    for (uint32_t i = 0; i < numDescriptors; ++i)
    {
        dstDescriptor[i] = CD3DX12_CPU_DESCRIPTOR_HANDLE(srcDescriptors, i, m_descriptorHandleIncrementSize);
    }
    
    // Set the root parameter index bit to make sure the descriptor table 
    // at that index is bound to the command list.
    this->m_staleDescriptorTableBitMask |= (1 << rootParameterIndex);
}

void Core::DynamicDescriptorHeap::CommitStagedDescriptors(CommandList& commandList, std::function<void(ID3D12GraphicsCommandList*, UINT, D3D12_GPU_DESCRIPTOR_HANDLE)> setFunc)
{ 
    // Compute the number of descriptors that need to be copied 
    uint32_t numDescriptorsToCommit = ComputeStaleDescriptorCount();

    if (numDescriptorsToCommit <= 0)
    {
        return;
    }

	auto d3d12GraphicsCommandList = commandList.GetD3D12Impl();
	LOG_CORE_ASSERT(d3d12GraphicsCommandList != nullptr, "Invalid Command List");

	if (!this->m_currentDescriptorHeap || this->m_numFreeHandles < numDescriptorsToCommit)
	{
		this->m_currentDescriptorHeap = RequestDescriptorHeap();
		this->m_currentCPUDescriptorHandle = this->m_currentDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		this->m_currentGPUDescriptorHandle = this->m_currentDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
		this->m_numFreeHandles = this->m_numDescriptorsPerHeap;

		commandList.SetDescriptorHeap(this->m_descriptorHeapType, this->m_currentDescriptorHeap.Get());

		// When updating the descriptor heap on the command list, all descriptor
		// tables must be (re)recopied to the new descriptor heap (not just
		// the stale descriptor tables).
		this->m_staleDescriptorTableBitMask = this->m_descriptorTableBitMask;
	}

	DWORD rootIndex;
	// Scan from LSB to MSB for a bit set in staleDescriptorsBitMask
	while (_BitScanForward(&rootIndex, this->m_staleDescriptorTableBitMask))
	{
		UINT numSrcDescriptors = this->m_descriptorTableCache[rootIndex].NumDescriptors;
		D3D12_CPU_DESCRIPTOR_HANDLE* pSrcDescriptorHandles = this->m_descriptorTableCache[rootIndex].BaseDescriptor;

		D3D12_CPU_DESCRIPTOR_HANDLE pDestDescriptorRangeStarts[] =
		{
			this->m_currentCPUDescriptorHandle
		};
		UINT pDestDescriptorRangeSizes[] =
		{
			numSrcDescriptors
		};

		// Copy the staged CPU visible descriptors to the GPU visible descriptor heap.
		this->m_device->CopyDescriptors(1, pDestDescriptorRangeStarts, pDestDescriptorRangeSizes,
			numSrcDescriptors, pSrcDescriptorHandles, nullptr, this->m_descriptorHeapType);

		// Set the descriptors on the command list using the passed-in setter function.
		setFunc(d3d12GraphicsCommandList, rootIndex, this->m_currentGPUDescriptorHandle);

		// Offset current CPU and GPU descriptor handles.
		this->m_currentCPUDescriptorHandle.Offset(numSrcDescriptors, this->m_descriptorHandleIncrementSize);
		this->m_currentGPUDescriptorHandle.Offset(numSrcDescriptors, this->m_descriptorHandleIncrementSize);
		this->m_numFreeHandles -= numSrcDescriptors;

		// Flip the stale bit so the descriptor table is not recopied again unless it is updated with a new descriptor.
		this->m_staleDescriptorTableBitMask ^= (1 << rootIndex);
	}
}

void Core::DynamicDescriptorHeap::CommitStagedDescriptorsForDraw(CommandList& commandList)
{
    this->CommitStagedDescriptors(
        commandList,
        &ID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable);
}

void Core::DynamicDescriptorHeap::CommitStagedDescriptorsForDispatch(CommandList& commandList)
{
    this->CommitStagedDescriptors(
        commandList,
        &ID3D12GraphicsCommandList::SetComputeRootDescriptorTable);
}

D3D12_GPU_DESCRIPTOR_HANDLE Core::DynamicDescriptorHeap::CopyDescriptor(CommandList& comandList, D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor)
{
    if (!this->m_currentDescriptorHeap || this->m_numFreeHandles < 1)
    {
        this->m_currentDescriptorHeap = RequestDescriptorHeap();
        this->m_currentCPUDescriptorHandle = this->m_currentDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        this->m_currentGPUDescriptorHandle = this->m_currentDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
        this->m_numFreeHandles = this->m_numDescriptorsPerHeap;

        comandList.SetDescriptorHeap(this->m_descriptorHeapType, this->m_currentDescriptorHeap.Get());

        // When updating the descriptor heap on the command list, all descriptor
        // tables must be (re)recopied to the new descriptor heap (not just
        // the stale descriptor tables).
        this->m_staleDescriptorTableBitMask = this->m_descriptorTableBitMask;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE hGPU = this->m_currentGPUDescriptorHandle;
    this->m_device->CopyDescriptorsSimple(
        1, this->m_currentCPUDescriptorHandle, cpuDescriptor, this->m_descriptorHeapType);

    this->m_currentCPUDescriptorHandle.Offset(1, this->m_descriptorHandleIncrementSize);
    this->m_currentGPUDescriptorHandle.Offset(1, this->m_descriptorHandleIncrementSize);
    this->m_numFreeHandles -= 1;

    return hGPU;
}

/*
void Core::DynamicDescriptorHeap::ParseRootSignature(const RootSignature& rootSignature)
{
    // If the root signature changes, all descriptors must be (re)bound to the
    // command list.
    this->m_staleDescriptorTableBitMask = 0;

    const auto& rootSignatureDesc = rootSignature.GetRootSignatureDesc();

    // Get a bit mask that represents the root parameter indices that match the 
    // descriptor heap type for this dynamic descriptor heap.
    this->m_descriptorTableBitMask = rootSignature.GetDescriptorTableBitMask(m_descriptorHeapType);
    uint32_t descriptorTableBitMask = this->m_descriptorTableBitMask;

    uint32_t currentOffset = 0;
    DWORD rootIndex;
    while (_BitScanForward(&rootIndex, descriptorTableBitMask) && rootIndex < rootSignatureDesc.NumParameters)
    {
        uint32_t numDescriptors = rootSignature.GetNumDescriptors(rootIndex);

        DescriptorTableCache& descriptorTableCache = this->m_descriptorTableCache[rootIndex];
        descriptorTableCache.NumDescriptors = numDescriptors;
        descriptorTableCache.BaseDescriptor = this->m_descriptorHandleCache.get() + currentOffset;

        currentOffset += numDescriptors;

        // Flip the descriptor table bit so it's not scanned again for the current index.
        descriptorTableBitMask ^= (1 << rootIndex);
    }
    // Make sure the maximum number of descriptors per descriptor heap has not been exceeded.
    LOG_CORE_ASSERT(
        currentOffset <= this->m_numDescriptorsPerHeap, 
        "The root signature requires more than the maximum number of descriptors per descriptor heap. Consider increasing the maximum number of descriptors per descriptor heap.");
}
*/
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> Core::DynamicDescriptorHeap::RequestDescriptorHeap()
{
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
    if (!this->m_availableDescriptorHeaps.empty())
    {
        descriptorHeap = this->m_availableDescriptorHeaps.front();
        this->m_availableDescriptorHeaps.pop();
    }
    else
    {
        descriptorHeap = CreateDescriptorHeap();
        this->m_descriptorHeapPool.push(descriptorHeap);
    }

    return descriptorHeap;
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> Core::DynamicDescriptorHeap::CreateDescriptorHeap()
{
    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
    descriptorHeapDesc.Type = m_descriptorHeapType;
    descriptorHeapDesc.NumDescriptors = m_numDescriptorsPerHeap;
    descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
    ThrowIfFailed(
        this->m_device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap)));

    return descriptorHeap;
}

uint32_t Core::DynamicDescriptorHeap::ComputeStaleDescriptorCount() const
{
    uint32_t numStaleDescriptors = 0;
    DWORD i;
    DWORD staleDescriptorsBitMask = this->m_staleDescriptorTableBitMask;

    while (_BitScanForward(&i, staleDescriptorsBitMask))
    {
        numStaleDescriptors += this->m_descriptorTableCache[i].NumDescriptors;
        staleDescriptorsBitMask ^= (1 << i);
    }

    return numStaleDescriptors;
}

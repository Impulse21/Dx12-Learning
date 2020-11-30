#include "pch.h"

#include "CommandQueue.h"

using namespace Core;
using namespace Microsoft::WRL;

CommandQueue::CommandQueue(
	Microsoft::WRL::ComPtr<ID3D12Device2> device,
	D3D12_COMMAND_LIST_TYPE type)
	: m_type(type)
	, m_device(device)
	, m_fenceValue(0)
	, m_commandAllocatorPool(device, type)
{
	// Create Command Queue
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = this->m_type;
	queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.NodeMask = 0;
	ThrowIfFailed(
		this->m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&this->m_commandQueue)));

	this->m_commandQueue->SetName(L"Dx12CommandQueue::Dx12CommandQueue::CommandQueue");

	// Create Fence
	ThrowIfFailed(
		this->m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&this->m_fence)));
	this->m_fence->SetName(L"Dx12CommandQueue::Dx12CommandQueue::Fence");

	// Create Handle
	this->m_fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	LOG_CORE_ASSERT(this->m_fenceEvent, "Failed to create fence event.");
}

CommandQueue::~CommandQueue()
{
	if (this->m_fenceEvent)
	{
		::CloseHandle(this->m_fenceEvent);
	}
}

Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> Core::CommandQueue::GetCommandList()
{
	ID3D12CommandAllocator* commandAllocator =  this->RequestAllocator();
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList;

	if (!this->m_commandListQueue.empty())
	{
		commandList = this->m_commandListQueue.front();
		this->m_commandListQueue.pop();

		ThrowIfFailed(
			commandList->Reset(commandAllocator, nullptr));
	}
	else
	{
		commandList = this->CreateCommandList(commandAllocator);
	}


	// Associate the command allocator with the command list so that it can be
	// retrieved when the command list is executed.
	ThrowIfFailed(
		commandList->SetPrivateDataInterface(__uuidof(ID3D12CommandAllocator), commandAllocator));

	return commandList;
}

uint64_t CommandQueue::ExecuteCommandList(
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList)
{
	commandList->Close();


	ID3D12CommandAllocator* commandAllocator;
	UINT dataSize = sizeof(commandAllocator);
	ThrowIfFailed(
		commandList->GetPrivateData(__uuidof(ID3D12CommandAllocator), &dataSize, &commandAllocator));

	ID3D12CommandList* const ppCommandLists[] = 
	{
		commandList.Get()
	};

	this->m_commandQueue->ExecuteCommandLists(1, ppCommandLists);
	uint64_t fenceValue = this->Signal();

	this->m_commandAllocatorPool.DiscardAllocator(fenceValue, commandAllocator);
	this->m_commandListQueue.push(commandList);

	return fenceValue;
}


ID3D12CommandAllocator* CommandQueue::RequestAllocator()
{
	uint64_t completedFence = this->m_fence->GetCompletedValue();
	return this->m_commandAllocatorPool.RequestAllocator(completedFence);
}

void CommandQueue::DiscardAllocator(uint64_t fence, ID3D12CommandAllocator* allocator)
{
	this->m_commandAllocatorPool.DiscardAllocator(fence, allocator);
}

uint64_t CommandQueue::Signal()
{
	// This is the value that should be signaled when the GPU is finished the command queue.
	uint64_t fenceValue = ++this->m_fenceValue;
	ThrowIfFailed(
		this->m_commandQueue->Signal(this->m_fence.Get(), fenceValue));

	return fenceValue;
}

void CommandQueue::WaitForFenceValue(uint64_t fenceValue)
{
	if (!this->IsFenceComplete(fenceValue))
	{
		this->m_fence->SetEventOnCompletion(fenceValue, this->m_fenceEvent);
		::WaitForSingleObject(this->m_fenceEvent, DWORD_MAX);
	}
}

void CommandQueue::Flush()
{
	this->WaitForFenceValue(this->Signal());
}

Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CommandQueue::CreateCommandAllocator()
{
	ComPtr<ID3D12CommandAllocator> commandAllocator;
	ThrowIfFailed(
		this->m_device->CreateCommandAllocator(this->m_type, IID_PPV_ARGS(&commandAllocator)));

	return commandAllocator;
}

Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> CommandQueue::CreateCommandList(
	ID3D12CommandAllocator* commandAllocator)
{

	ComPtr<ID3D12GraphicsCommandList2> commandList;
	ThrowIfFailed(
		this->m_device->CreateCommandList(
			0,
			this->m_type,
			commandAllocator,
			nullptr,
			IID_PPV_ARGS(&commandList)));

	return commandList;
}
#include "pch.h"

#include "CommandQueue.h"
#include "CommandList.h"
#include "ResourceStateTracker.h"

using namespace Core;
using namespace Microsoft::WRL;

CommandQueue::CommandQueue(
	Microsoft::WRL::ComPtr<ID3D12Device2> device,
	D3D12_COMMAND_LIST_TYPE type)
	: m_type(type)
	, m_device(device)
	, m_fenceValue(0)
	, m_commandAllocatorPool(device, type)
	, m_bProcessInflightCommandLists(true)
{
	// Create Command Queue
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = this->m_type;
	queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.NodeMask = 0;

	ThrowIfFailed(
		this->m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&this->m_commandQueue)));

	// Create Fence
	ThrowIfFailed(
		this->m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&this->m_fence)));
	this->m_fence->SetName(L"Dx12CommandQueue::Dx12CommandQueue::Fence");

	switch (type)
	{
	case D3D12_COMMAND_LIST_SUPPORT_FLAG_DIRECT:
		this->m_commandQueue->SetName(L"Direct Command Queue");
		break;
	case D3D12_COMMAND_LIST_SUPPORT_FLAG_COPY:
		this->m_commandQueue->SetName(L"Copy Command Queue");
		break;
	case D3D12_COMMAND_LIST_SUPPORT_FLAG_COMPUTE:
		this->m_commandQueue->SetName(L"Compute Command Queue");
		break;

	}

	this->m_processInflightCommnadListsThread = 
		std::thread(&CommandQueue::ProccessInFlightCommandLists, this);
}

CommandQueue::~CommandQueue()
{
	this->m_bProcessInflightCommandLists = false;
	this->m_processInflightCommnadListsThread.join();
}

std::shared_ptr<CommandList> Core::CommandQueue::GetCommandList()
{
	std::shared_ptr<CommandList> commandList;

	if (!this->m_availableCommandList.Empty())
	{
		this->m_availableCommandList.TryPop(commandList);
	}
	else
	{
		ID3D12CommandAllocator* commandAllocator = this->RequestAllocator();
		auto d3d12CommandList = this->CreateCommandList(commandAllocator);
		commandList = std::make_shared<CommandList>(
			this->m_device,
			d3d12CommandList,
			commandAllocator);
	}

	return commandList;
}

uint64_t Core::CommandQueue::ExecuteCommandList(std::shared_ptr<CommandList> commandList)
{
	ResourceStateTracker::Lock();

	// Command lists that need to put back on the command list queue.
	std::vector<std::shared_ptr<CommandList> > toBeQueued;
	toBeQueued.reserve(1 * 2);        // 2x since each command list will have a pending command list.

	// Command lists that need to be executed.
	std::vector<ID3D12CommandList*> d3d12CommandLists;
	d3d12CommandLists.reserve(1 * 2); // 2x since each command list will have a pending command list.

	auto pendingCommandList = this->GetCommandList();

	bool hasPendingBarrieris = commandList->Close(*pendingCommandList);

	pendingCommandList->Close();

	if (hasPendingBarrieris)
	{
		d3d12CommandLists.push_back(pendingCommandList->GetD3D12Impl());
	}
	d3d12CommandLists.push_back(commandList->GetD3D12Impl());

	toBeQueued.push_back(pendingCommandList);
	toBeQueued.push_back(commandList);

	this->m_commandQueue->ExecuteCommandLists(
		static_cast<UINT>(d3d12CommandLists.size()),
		d3d12CommandLists.data());

	uint64_t fenceValue = this->Signal();

	ResourceStateTracker::Unlock();

	// Queue command lists for reuse.
	for (auto commandList : toBeQueued)
	{
		this->m_inflightCommandLists.Push({ fenceValue, commandList });
	}

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
		auto fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
		LOG_CORE_ASSERT(fenceEvent, "Failed to create fence event.");

		this->m_fence->SetEventOnCompletion(fenceValue, fenceEvent);
		::WaitForSingleObject(fenceEvent, DWORD_MAX);

		::CloseHandle(fenceEvent);
	}
}

void CommandQueue::Flush()
{
	std::unique_lock<std::mutex> lock(this->m_processInFlightCommandListsThreadMutex);
	this->m_processInflightCommandListThreadCv.wait(
		lock, 
		[this] { return this->m_inflightCommandLists.Empty(); });

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

void Core::CommandQueue::ProccessInFlightCommandLists()
{
	std::unique_lock<std::mutex> lock(this->m_processInFlightCommandListsThreadMutex, std::defer_lock);

	while (this->m_bProcessInflightCommandLists)
	{
		CommandListEntry commandListEntry;

		lock.lock();
		while (this->m_inflightCommandLists.TryPop(commandListEntry))
		{
			auto fenceValue = std::get<0>(commandListEntry);
			auto commandList = std::get<1>(commandListEntry);

			this->WaitForFenceValue(fenceValue);

			commandList->Reset();

			this->m_availableCommandList.Push(commandList);
		}
		lock.unlock();
		this->m_processInflightCommandListThreadCv.notify_one();

		std::this_thread::yield();
	}
}

#pragma once

#include <d3d12.h>  // For ID3D12CommandQueue, ID3D12Device2, and ID3D12Fence
#include <wrl.h>    // For Microsoft::WRL::ComPtr

#include <queue>    // For std::queue

#include <thread>
#include <condition_variable>

#include "CommandAllocatorPool.h"
#include "ThreadSafePool.h"

namespace Core
{
	class CommandList;
	class Dx12RenderDevice;

	// TODO: Should be non copyable
	class CommandQueue
	{
	public:
		CommandQueue(
			std::shared_ptr<Dx12RenderDevice> renderDevice,
			D3D12_COMMAND_LIST_TYPE type);

		~CommandQueue();
		
		std::shared_ptr<CommandList> GetCommandList();

		uint64_t ExecuteCommandList(std::shared_ptr<CommandList> commandList);
		uint64_t Signal();


		bool IsFenceComplete(uint64_t fenceValue) { return this->m_fence->GetCompletedValue() >= fenceValue; };
		void WaitForFenceValue(uint64_t fenceValue);
		void Flush();

		ID3D12CommandQueue* GetImpl() const { return this->m_commandQueue.Get(); }

	protected:

		ID3D12CommandAllocator* RequestAllocator();
		void DiscardAllocator(uint64_t fence, ID3D12CommandAllocator* allocator);

		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CreateCommandAllocator();

		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> CreateCommandList(
			ID3D12CommandAllocator* commandAllocator);

	private:
		void ProccessInFlightCommandLists();

	private:
		const D3D12_COMMAND_LIST_TYPE m_type;

		std::shared_ptr<Dx12RenderDevice> m_renderDevice;

		Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
		Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;

		CommandAllocatorPool m_commandAllocatorPool;

		// new stuff

		std::atomic_uint64_t m_fenceValue;

		using CommandListEntry = std::tuple<uint64_t, std::shared_ptr<CommandList>>;
		ThreadSafePool<CommandListEntry> m_inflightCommandLists;
		ThreadSafePool<std::shared_ptr<CommandList>> m_availableCommandList;

		std::thread m_processInflightCommnadListsThread;
		std::atomic_bool m_bProcessInflightCommandLists;
		std::mutex m_processInFlightCommandListsThreadMutex;
		std::condition_variable m_processInflightCommandListThreadCv;
	};
}


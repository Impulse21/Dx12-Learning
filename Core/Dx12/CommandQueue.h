#pragma once

#include <d3d12.h>  // For ID3D12CommandQueue, ID3D12Device2, and ID3D12Fence
#include <wrl.h>    // For Microsoft::WRL::ComPtr

#include <cstdint>  // For uint64_t
#include <queue>    // For std::queue

#include "CommandAllocatorPool.h"

namespace Core
{
	// TODO: Should be non copyable
	class CommandQueue
	{
	public:
		CommandQueue(
			Microsoft::WRL::ComPtr<ID3D12Device2> device,
			D3D12_COMMAND_LIST_TYPE type);

		~CommandQueue();

		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> GetCommandList();
	
		/// <summary>
		/// Executes a command list
		/// </summary>
		/// <param name="commnadList"></param>
		/// <returns>Fence value for CPU to wait on.</returns>
		uint64_t ExecuteCommandList(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commnadList);

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
		const D3D12_COMMAND_LIST_TYPE m_type;

		Microsoft::WRL::ComPtr<ID3D12Device2> m_device;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;

		Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
		HANDLE m_fenceEvent;
		uint64_t m_fenceValue;

		std::queue<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>> m_commandListQueue;
		CommandAllocatorPool m_commandAllocatorPool;
	};
}


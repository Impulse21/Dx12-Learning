#pragma once

#include <d3d12.h>
#include <wrl.h>

#include <deque>
#include <memory>

#include "Defines.h"
namespace Core
{
	class UploadBuffer
	{
	public:
		struct Allocation
		{
			void* Cpu;
			D3D12_GPU_VIRTUAL_ADDRESS Gpu;
		};

	public:
		explicit UploadBuffer(

			Microsoft::WRL::ComPtr<ID3D12Device> device,
			size_t pageSize = _2MB);

		Allocation Allocate(size_t sizeInBytes, size_t alignment);

		void Reset();
							
	public:
		size_t GetPageSize() const { return this->m_pageSize; }

	private:
		struct Page
		{
			Page(
				Microsoft::WRL::ComPtr<ID3D12Resource> resource,
				size_t sizeInBytes);
			~Page();

			bool HasSpace(size_t sizeInBytes, size_t alignment);
			Allocation Allocate(size_t sizeInBytes, size_t alignment);
			void Reset();

		private:
			Microsoft::WRL::ComPtr<ID3D12Resource> m_d3d12Resource;
			Microsoft::WRL::ComPtr<ID3D12Device> m_device;

			// Base Pointers
			void* m_cpuPtr;
			D3D12_GPU_VIRTUAL_ADDRESS m_gpuPtr;

			size_t m_pageSize;
			size_t m_offset;
		};

		std::shared_ptr<UploadBuffer::Page> RequestPage();

	private:
		Microsoft::WRL::ComPtr<ID3D12Device> m_device;
		using PagePool = std::deque<std::shared_ptr<Page>>;

		PagePool m_pagePool;
		PagePool m_availablePages;

		std::shared_ptr<Page> m_currentPage;

		size_t m_pageSize;
	};
}


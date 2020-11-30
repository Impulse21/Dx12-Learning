#pragma once

#include "d3dx12.h"

#include <wrl.h>

namespace Core
{
	class DescriptorHeap
	{
	public:
		DescriptorHeap(
			Microsoft::WRL::ComPtr<ID3D12Device2> device,
			D3D12_DESCRIPTOR_HEAP_TYPE type,
			size_t numDescriptors,
			D3D12_DESCRIPTOR_HEAP_FLAGS flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE);


		D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(size_t heapIndex) const;
		D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(size_t hepIndex) const;

		size_t Count() const noexcept { return this->m_numDescriptors; }

	private:
		const D3D12_DESCRIPTOR_HEAP_TYPE m_heapType;
		const D3D12_DESCRIPTOR_HEAP_FLAGS m_heapFlags;
		const UINT m_descriptorSize;
		const size_t m_numDescriptors;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;
	};

	class DescriptorPile : public DescriptorHeap
	{
	public:
		using IndexType = size_t;
		static const IndexType INVALID_INDEX = size_t(-1);

		DescriptorPile(
			Microsoft::WRL::ComPtr<ID3D12Device2> device,
			D3D12_DESCRIPTOR_HEAP_TYPE type,
			size_t capacity,
			size_t reserve = 0,
			D3D12_DESCRIPTOR_HEAP_FLAGS flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE) noexcept(false);


		IndexType Allocate()
		{
			IndexType start = INVALID_INDEX;
			IndexType end = INVALID_INDEX;

			this->AllocateRange(1, start, end);

			return start;
		};

		void AllocateRange(size_t numDescriptors, IndexType& outStart, IndexType& outEnd);

	private:
			IndexType m_top;
	};
}


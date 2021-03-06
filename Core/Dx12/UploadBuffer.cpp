#include "pch.h"
#include "UploadBuffer.h"

#include "MathHelpers.h"

#include "d3dx12.h"

#include <new> // for std::bad_alloc

using namespace Core;

Core::UploadBuffer::UploadBuffer(
	Microsoft::WRL::ComPtr<ID3D12Device> device, 
	size_t pageSize)
	: m_device(device)
	, m_pageSize(pageSize)
{
}

UploadBuffer::Allocation UploadBuffer::Allocate(size_t sizeInBytes, size_t alignment)
{
	if (sizeInBytes > this->m_pageSize)
	{
		throw std::bad_alloc();
	}

	if (!this->m_currentPage || this->m_currentPage->HasSpace(sizeInBytes, alignment))
	{
		this->m_currentPage = this->RequestPage();
	}

	return this->m_currentPage->Allocate(sizeInBytes, alignment);
}

void Core::UploadBuffer::Reset()
{
	this->m_currentPage = nullptr;
	this->m_availablePages = this->m_pagePool;

	for (auto& page : this->m_availablePages)
	{
		page->Reset();
	}
}

std::shared_ptr<UploadBuffer::Page> UploadBuffer::RequestPage()
{
	std::shared_ptr<Page> page;

	if (this->m_availablePages.empty())
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;
		ThrowIfFailed(
			this->m_device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(m_pageSize),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&uploadBuffer)
			));

		uploadBuffer->SetName(L"Upload Buffer (Page)");

		page = std::make_shared<UploadBuffer::Page>(uploadBuffer, this->m_pageSize);
		this->m_pagePool.push_back(page);
	}
	else
	{
		page = this->m_availablePages.front();
		this->m_availablePages.pop_front();
	}

	return page;
}

Core::UploadBuffer::Page::Page(
	Microsoft::WRL::ComPtr<ID3D12Resource> resource,
	size_t sizeInBytes)
	: m_pageSize(sizeInBytes)
	, m_d3d12Resource(resource)
	, m_offset(0)
	, m_cpuPtr(nullptr)
	, m_gpuPtr(resource->GetGPUVirtualAddress())
{
	this->m_d3d12Resource->Map(0, nullptr, &this->m_cpuPtr);
}

Core::UploadBuffer::Page::~Page()
{
	this->m_d3d12Resource->Unmap(0, nullptr);
	this->m_cpuPtr = nullptr;
	this->m_gpuPtr = D3D12_GPU_VIRTUAL_ADDRESS(0);
}

bool Core::UploadBuffer::Page::HasSpace(size_t sizeInBytes, size_t alignment)
{
	size_t alignedSize = Math::AlignUp(sizeInBytes, alignment);
	size_t alignmentOffset = Math::AlignUp(this->m_offset, alignment);

	return alignmentOffset + alignedSize <= this->m_pageSize;
}

UploadBuffer::Allocation UploadBuffer::Page::Allocate(size_t sizeInBytes, size_t alignment)
{
	if (!this->HasSpace(sizeInBytes, alignment))
	{
		throw std::bad_alloc();
	}

	size_t alignedSize = Math::AlignUp(sizeInBytes, alignment);
	this->m_offset = Math::AlignUp(sizeInBytes, alignment);

	UploadBuffer::Allocation allocation = {};
	allocation.Cpu = static_cast<uint8_t*>(this->m_cpuPtr) + this->m_offset;
	allocation.Gpu = this->m_gpuPtr + this->m_offset;

	this->m_offset += alignedSize;
	return allocation;
}

void Core::UploadBuffer::Page::Reset()
{
	this->m_offset = 0;
}



#include "pch.h"
#include "Dx12RenderDevice.h"

#include "DescriptorAllocator.h"

#include "DescriptorHeap.h"

using namespace Core;

Core::Dx12RenderDevice::Dx12RenderDevice()
{
}

std::shared_ptr<CommandQueue> Core::Dx12RenderDevice::GetQueue(D3D12_COMMAND_LIST_TYPE type)
{
	switch (type)
	{
	case D3D12_COMMAND_LIST_TYPE_DIRECT:
		return this->m_directQueue;

	case D3D12_COMMAND_LIST_TYPE_COPY:
		return this->m_directQueue;

	case D3D12_COMMAND_LIST_TYPE_COMPUTE:
		return this->m_directQueue;
	default:
		LOG_CORE_ASSERT(false, "Unexpected commandlist type");
	}

	return nullptr;
}

void Core::Dx12RenderDevice::Flush()
{
	this->m_computeQueue->Flush();
	this->m_copyQueue->Flush();
	this->m_directQueue->Flush();
}

DescriptorAllocation Dx12RenderDevice::AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescritpors)
{
	return this->m_descriptorAllocators[type]->Allocate(numDescritpors);
}

void Core::Dx12RenderDevice::ReleaseStaleDescriptors(uint64_t finishedFrame)
{
	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		this->m_descriptorAllocators[i]->ReleaseStaleDescriptors(finishedFrame);
	}
}

std::unique_ptr<DescriptorHeap> Core::Dx12RenderDevice::CreateDescriptorHeap(UINT numDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE type)
{
	return std::make_unique<DescriptorHeap>(this->m_d3d12Device, type, numDescriptors);
}

UINT Core::Dx12RenderDevice::GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE type) const
{
	return this->m_d3d12Device->GetDescriptorHandleIncrementSize(type);
}

void Core::Dx12RenderDevice::UploadBufferResource(
	ID3D12GraphicsCommandList2* commandList,
	ID3D12Resource** pDestinationResource,
	ID3D12Resource** pIntermediateResource,
	size_t numOfElements,
	size_t elementStride,
	const void* data, D3D12_RESOURCE_FLAGS flags)
{
	size_t bufferSize = numOfElements * elementStride;

	// Create a committed resource for the GPU resource in a default heap.
	ThrowIfFailed(
		this->m_d3d12Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(bufferSize, flags),
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(pDestinationResource)));

	// Create an committed resource for the upload.
	if (data)
	{
		ThrowIfFailed(
			this->m_d3d12Device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(pIntermediateResource)));

		D3D12_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pData = data;
		subresourceData.RowPitch = bufferSize;
		subresourceData.SlicePitch = subresourceData.RowPitch;

		UpdateSubresources(
			commandList,
			*pDestinationResource, *pIntermediateResource,
			0, 0, 1, &subresourceData);
	}
}

void Core::Dx12RenderDevice::Initialize()
{
	Microsoft::WRL::ComPtr<IDXGIFactory6> dxgiFactory;
	UINT createFactoryFlags = 0;
#ifdef BCE_DEBUG
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif // BCE_DEBUG

	ThrowIfFailed(
		CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

	this->Initialize(dxgiFactory);
}

void Core::Dx12RenderDevice::Initialize(Microsoft::WRL::ComPtr<IDXGIFactory6> dxgiFactory)
{
	this->CreateDevice(dxgiFactory);
	this->CreateCommandQueues();

	// Intialize Descriptors
	for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++)
	{
		this->m_descriptorAllocators[i] =
			std::make_unique<DescriptorAllocator>(
				this->m_d3d12Device,
				static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(i));
	}
}

void Core::Dx12RenderDevice::CreateDevice(Microsoft::WRL::ComPtr<IDXGIFactory6> dxgiFactory)
{
	LOG_CORE_INFO("Creating DirectX 12 Rendering Device");
	this->EnableDebugLayer();

	Microsoft::WRL::ComPtr<IDXGIAdapter1> selectedAdapter = this->FindCompatibleAdapter(dxgiFactory);

	LOG_CORE_ASSERT(selectedAdapter, "No compatable adapter was found");
	DXGI_ADAPTER_DESC1 desc;
	selectedAdapter->GetDesc1(&desc);
	LOG_CORE_INFO("DX12 capable adatper found '{0}' ({1} MB)", NarrowString(desc.Description), desc.DedicatedVideoMemory >> 20);

	this->m_dxgiAdapter = selectedAdapter;

	// Create Device.
	Microsoft::WRL::ComPtr<ID3D12Device2> dx12Device;

	ThrowIfFailed(
		D3D12CreateDevice(selectedAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(dx12Device.ReleaseAndGetAddressOf())));

#ifndef LOG_RELEASE
	bool DeveloperModeEnabled = false;

	// Look in the Windows Registry to determine if Developer Mode is enabled
	HKEY hKey;
	LSTATUS result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\AppModelUnlock", 0, KEY_READ, &hKey);
	if (result == ERROR_SUCCESS)
	{
		DWORD keyValue, keySize = sizeof(DWORD);
		result = RegQueryValueEx(hKey, L"AllowDevelopmentWithoutDevLicense", 0, NULL, (byte*)&keyValue, &keySize);
		if (result == ERROR_SUCCESS && keyValue == 1)
		{
			DeveloperModeEnabled = true;
		}
		RegCloseKey(hKey);
	}

	if (!DeveloperModeEnabled)
	{
		LOG_CORE_WARN("Enable Developer Mode on Windows 10 to get consistent profiling results");
	}

	// Prevent the GPU from overclocking or underclocking to get consistent timings
	if (DeveloperModeEnabled)
	{
		dx12Device->SetStablePowerState(TRUE);
	}
#endif

#if LOG_DEBUG
	// Set message filters
	ID3D12InfoQueue* pInfoQueue = nullptr;
	if (SUCCEEDED(dx12Device->QueryInterface(IID_PPV_ARGS(&pInfoQueue))))
	{
		// Suppress whole categories of messages
		//D3D12_MESSAGE_CATEGORY Categories[] = {};

		// Suppress messages based on their severity level
		D3D12_MESSAGE_SEVERITY Severities[] =
		{
			D3D12_MESSAGE_SEVERITY_INFO
		};

		// Suppress individual messages by their ID
		D3D12_MESSAGE_ID DenyIds[] =
		{
			// This occurs when there are uninitialized descriptors in a descriptor table, even when a
			// shader does not access the missing descriptors.  I find this is common when switching
			// shader permutations and not wanting to change much code to reorder resources.
			D3D12_MESSAGE_ID_INVALID_DESCRIPTOR_HANDLE,

			// Triggered when a shader does not export all color components of a render target, such as
			// when only writing RGB to an R10G10B10A2 buffer, ignoring alpha.
			D3D12_MESSAGE_ID_CREATEGRAPHICSPIPELINESTATE_PS_OUTPUT_RT_OUTPUT_MISMATCH,

			// This occurs when a descriptor table is unbound even when a shader does not access the missing
			// descriptors.  This is common with a root signature shared between disparate shaders that
			// don't all need the same types of resources.
			D3D12_MESSAGE_ID_COMMAND_LIST_DESCRIPTOR_TABLE_NOT_SET,

			// RESOURCE_BARRIER_DUPLICATE_SUBRESOURCE_TRANSITIONS
			(D3D12_MESSAGE_ID)1008,
		};

		D3D12_INFO_QUEUE_FILTER NewFilter = {};
		//NewFilter.DenyList.NumCategories = _countof(Categories);
		//NewFilter.DenyList.pCategoryList = Categories;
		NewFilter.DenyList.NumSeverities = _countof(Severities);
		NewFilter.DenyList.pSeverityList = Severities;
		NewFilter.DenyList.NumIDs = _countof(DenyIds);
		NewFilter.DenyList.pIDList = DenyIds;

		pInfoQueue->PushStorageFilter(&NewFilter);
		pInfoQueue->Release();
	}

	Microsoft::WRL::ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(dxgiInfoQueue.GetAddressOf()))))
	{
		dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
		dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
	}
#endif

	// Check feature level - this should be checked as part of compatible device check.
	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

	ThrowIfFailed(
		dx12Device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData)));

	this->m_d3d12Device = dx12Device;
	SetD3DDebugName(this->m_d3d12Device, L"Main Device Object");
}

void Core::Dx12RenderDevice::EnableDebugLayer()
{
#ifdef _DEBUG
	// Always enable the debug layer before doing anything DX12 related
	 // so all possible errors generated while creating DX12 objects
	 // are caught by the debug layer.
	Microsoft::WRL::ComPtr<ID3D12Debug> debugInterface;
	auto hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface));
	LOG_CORE_FATAL_ASSERT(hr == S_OK, "Failed to get Debug interface");
	debugInterface->EnableDebugLayer();
#endif
}

Microsoft::WRL::ComPtr<IDXGIAdapter1> Core::Dx12RenderDevice::FindCompatibleAdapter(Microsoft::WRL::ComPtr<IDXGIFactory6> dxgiFactory)
{
	Microsoft::WRL::ComPtr<IDXGIAdapter1> pAdapter;
	Microsoft::WRL::ComPtr<IDXGIAdapter1> pSelectedAdapter;
	SIZE_T MaxDedicatedVideoMemory = 0;
	for (uint32_t i = 0; DXGI_ERROR_NOT_FOUND != dxgiFactory->EnumAdapters1(i, &pAdapter); i++)
	{
		DXGI_ADAPTER_DESC1 desc;
		pAdapter->GetDesc1(&desc);

		// Skip any software devices
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			continue;
		}

		if (desc.DedicatedVideoMemory > MaxDedicatedVideoMemory)
		{
			pSelectedAdapter = pAdapter;
			pAdapter->GetDesc1(&desc);
			MaxDedicatedVideoMemory = desc.DedicatedVideoMemory;
		}
	}

	return pSelectedAdapter;
}

void Core::Dx12RenderDevice::CreateCommandQueues()
{
	this->m_directQueue =
		std::make_shared<CommandQueue>(
			this->shared_from_this(),
			D3D12_COMMAND_LIST_TYPE_DIRECT);

	this->m_computeQueue =
		std::make_shared<CommandQueue>(
			this->shared_from_this(),
			D3D12_COMMAND_LIST_TYPE_COMPUTE);

	this->m_copyQueue =
		std::make_shared<CommandQueue>(
			this->shared_from_this(),
			D3D12_COMMAND_LIST_TYPE_COPY);
}

#include "pch.h"
#include "Application.h"

Core::Dx12Application::~Dx12Application()
{
	this->m_dsvDescriptorHeap.reset();
	this->m_rtvDescriptorHeap.reset();
	this->m_computeQueue.reset();
	this->m_copyQueue.reset();
	this->m_directQueue.reset();
}

void Core::Dx12Application::RunApplication()
{
	this->Ininitialize();
	this->LoadContent();

	static uint64_t frameCounter = 0;
	static double elapsedSeconds = 0.0;
	static std::chrono::high_resolution_clock clock;
	static auto t0 = clock.now();

	while (!this->m_window->IsClosing())
	{
		frameCounter++;
		auto t1 = clock.now();
		auto deltaTime = t1 - t0;
		t0 = t1;

		elapsedSeconds += deltaTime.count() * 1e-9;

		if (elapsedSeconds > 1.0)
		{
			frameCounter = 0;
			elapsedSeconds = 0.0;
		}

		// TODO: Sort out the game loop update functionality.
		this->m_window->PullEvents();
		this->Update(elapsedSeconds);
		this->Render();

		this->m_swapChain->Present();

		// Wait on commandQueues before starting next frame
		this->m_directQueue->WaitForFenceValue(this->m_frameFences[this->m_swapChain->GetCurrentBufferIndex()]);
	}

	this->Shutdown();
}

void Core::Dx12Application::UploadBufferResource(
	ID3D12GraphicsCommandList2* commandList,
	ID3D12Resource** pDestinationResource,
	ID3D12Resource** pIntermediateResource,
	size_t numOfElements,
	size_t elementStride,
	const void* data,
	D3D12_RESOURCE_FLAGS flags)
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

void Core::Dx12Application::Ininitialize()
{
	this->m_window = IWindow::Create();
	this->InitializeDx12();

	this->m_frameFences.resize(NumOfBuffers);
	
	for (int i = 0; i < NumOfBuffers; i++)
	{
		this->m_frameFences[i] = 0;
	}
}

void Core::Dx12Application::Shutdown()
{
	this->m_computeQueue->Flush();
	this->m_copyQueue->Flush();
	this->m_directQueue->Flush();
}

void Core::Dx12Application::InitializeDx12()
{
	Microsoft::WRL::ComPtr<IDXGIFactory6> dxgiFactory;
	UINT createFactoryFlags = 0;
#ifdef BCE_DEBUG
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif // BCE_DEBUG

	ThrowIfFailed(
		CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

	this->CreateDevice(dxgiFactory);

	this->CreateCommandQueues();

	this->CreateDescriptorHeaps();

	this->CreateSwapChain(dxgiFactory, this->m_window.get());

}

void Core::Dx12Application::CreateDevice(Microsoft::WRL::ComPtr<IDXGIFactory6> dxgiFactory)
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
}

void Core::Dx12Application::CreateSwapChain(Microsoft::WRL::ComPtr<IDXGIFactory6> dxgiFactory, IWindow* window)
{
	DXGI_SWAP_CHAIN_DESC1 dxgiSwapChainDesc = {};
	dxgiSwapChainDesc.Width = window->GetWidth();
	dxgiSwapChainDesc.Height = window->GetHeight();
	dxgiSwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	dxgiSwapChainDesc.Scaling = DXGI_SCALING_NONE;
	dxgiSwapChainDesc.SampleDesc.Quality = 0;
	dxgiSwapChainDesc.SampleDesc.Count = 1;
	dxgiSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // Pipeline will render to this target
	dxgiSwapChainDesc.BufferCount = 3;
	dxgiSwapChainDesc.Flags = 0;
	dxgiSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // Discard after we present.


	Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain = nullptr;
	ThrowIfFailed(
		dxgiFactory->CreateSwapChainForHwnd(
			this->m_directQueue->GetImpl(),
			static_cast<HWND>(window->GetNativeHandle()),
			&dxgiSwapChainDesc,
			nullptr,
			nullptr,
			&swapChain));

	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain4 = nullptr;
	ThrowIfFailed(
		swapChain.As(&swapChain4));

	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> backBuffers(NumOfBuffers);

	// Initialize Render Target Views
	for (int i = 0; i < backBuffers.size(); i++)
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> backBuffer;
		ThrowIfFailed(
			swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));

		auto rtvHandle = this->m_rtvDescriptorHeap->GetCpuHandle(i);
		this->m_d3d12Device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtvHandle);

		backBuffers[i] = backBuffer;
	}

	this->m_swapChain = std::make_unique<SwapChain>(
		swapChain4,
		std::move(backBuffers),
		dxgiSwapChainDesc.Format);
}

void Core::Dx12Application::EnableDebugLayer()
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

Microsoft::WRL::ComPtr<IDXGIAdapter1> Core::Dx12Application::FindCompatibleAdapter(Microsoft::WRL::ComPtr<IDXGIFactory6> dxgiFactory)
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

void Core::Dx12Application::CreateCommandQueues()
{
	this->m_directQueue =
		std::make_unique<CommandQueue>(
			this->m_d3d12Device,
			D3D12_COMMAND_LIST_TYPE_DIRECT);

	this->m_computeQueue =
		std::make_unique<CommandQueue>(
			this->m_d3d12Device,
			D3D12_COMMAND_LIST_TYPE_COMPUTE);

	this->m_copyQueue =
		std::make_unique<CommandQueue>(
			this->m_d3d12Device,
			D3D12_COMMAND_LIST_TYPE_COPY);

}

void Core::Dx12Application::CreateDescriptorHeaps()
{
	this->m_rtvDescriptorHeap =
		std::make_unique<DescriptorHeap>(
			this->m_d3d12Device.Get(),
			D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
			NumOfBuffers);

	this->m_dsvDescriptorHeap =
		std::make_unique<DescriptorHeap>(
			this->m_d3d12Device.Get(),
			D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
			1);
}

void Core::Dx12Application::InitializeSwapChainRenderTargets()
{
}

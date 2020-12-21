#include "pch.h"
#include "Application.h"

#include "Dx12/ResourceStateTracker.h"

Core::Dx12Application::~Dx12Application()
{
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
		this->m_frameCount++;
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

		this->m_frameCounts[this->m_swapChain->GetCurrentBufferIndex()] = this->m_frameCount;
		this->m_renderDevice->ReleaseStaleDescriptors(this->m_frameCount);

		// Wait on commandQueues before starting next frame
		this->m_renderDevice->GetQueue()->WaitForFenceValue(this->m_frameFences[this->m_swapChain->GetCurrentBufferIndex()]);
	}

	this->Shutdown();
}


void Core::Dx12Application::Ininitialize()
{
	this->m_window = IWindow::Create();
	this->InitializeDx12();

	this->m_frameFences.resize(NumOfBuffers);
	this->m_frameCounts.resize(NumOfBuffers);

	for (int i = 0; i < NumOfBuffers; i++)
	{
		this->m_frameFences[i] = 0;
		this->m_frameCounts[i] = 0;
	}
}

void Core::Dx12Application::Shutdown()
{
	this->m_renderDevice->Flush();
	for (int i = 0; i < this->m_frameCounts.size(); i++)
	{
		this->m_renderDevice->ReleaseStaleDescriptors(this->m_frameCounts[i]);
	}
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

	this->m_renderDevice = std::make_shared<Dx12RenderDevice>();
	this->m_renderDevice->Initialize(dxgiFactory);
	this->CreateSwapChain(dxgiFactory, this->m_window.get());

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
			this->m_renderDevice->GetQueue()->GetImpl(),
			static_cast<HWND>(window->GetNativeHandle()),
			&dxgiSwapChainDesc,
			nullptr,
			nullptr,
			&swapChain));

	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain4 = nullptr;
	ThrowIfFailed(
		swapChain.As(&swapChain4));

	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> backBuffers(NumOfBuffers);

	this->m_swapChain = std::make_unique<SwapChain>(
		swapChain4,
		this->m_renderDevice,
		NumOfBuffers,
		dxgiSwapChainDesc.Format);
}
#include "pch.h"

#include "SwapChain.h"

#include "Dx12RenderDevice.h"
#include "ResourceStateTracker.h"

using namespace Core;

Core::SwapChain::SwapChain(
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain,
	std::shared_ptr<Dx12RenderDevice> renderDevice,
	int numOfBuffer,
	DXGI_FORMAT format)
	: NumOfBuffers(numOfBuffer)
	, m_backBuffers(NumOfBuffers)
	, Format(format)
	, m_swapChain(swapChain)
	, m_currBufferIndex(swapChain->GetCurrentBackBufferIndex())
{

	// Create Descriptor HEAP
	this->m_rtvHeap = renderDevice->CreateDescriptorHeap(NumOfBuffers, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// Initialize Render Target Views
	for (int i = 0; i < this->m_backBuffers.size(); i++)
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> backBuffer;
		ThrowIfFailed(
			swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));

		// Alloca
		auto rtvHandle = this->m_rtvHeap->GetCpuHandle(i);
		renderDevice->GetD3DDevice()->CreateRenderTargetView(backBuffer.Get(), nullptr, rtvHandle);

		ResourceStateTracker::AddGlobalResourceState(backBuffer.Get(), D3D12_RESOURCE_STATE_COMMON);

		std::wstring name = std::wstring(L"Back Buffer ") + std::to_wstring(i);
		backBuffer->SetName(name.c_str());
		this->m_backBuffers[i] = backBuffer;
	}
}

void Core::SwapChain::Present()
{
	this->m_swapChain->Present(
		0, // VSync,
		0); // Allow Tearing

	this->m_currBufferIndex = this->m_swapChain->GetCurrentBackBufferIndex();
}

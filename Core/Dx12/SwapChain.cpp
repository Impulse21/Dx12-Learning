#include "pch.h"

#include "SwapChain.h"

using namespace Core;


Core::SwapChain::SwapChain(
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain,
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>&& backBuffers,
	DXGI_FORMAT format)
	: NumOfBuffers(backBuffers.size())
	, Format(format)
	, m_backBuffers(std::move(backBuffers))
	, m_swapChain(swapChain)
	, m_currBufferIndex(swapChain->GetCurrentBackBufferIndex())
{
}

void Core::SwapChain::Present()
{
	this->m_swapChain->Present(
		0, // VSync,
		0); // Allow Tearing

	this->m_currBufferIndex = this->m_swapChain->GetCurrentBackBufferIndex();
}
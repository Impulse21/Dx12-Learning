#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <memory>
#include <vector>

#include "DescriptorHeap.h"

namespace Core
{
	class Dx12RenderDevice;
	class SwapChain
	{
	public:
		SwapChain(
			Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain,
			std::shared_ptr<Dx12RenderDevice> renderDevice,
			int numOfBuffer,
			DXGI_FORMAT format);
		~SwapChain() {};

		void Present();

		int GetCurrentBufferIndex() const { return this->m_currBufferIndex; }
		Microsoft::WRL::ComPtr<ID3D12Resource> GetCurrentBackBuffer()
		{ 
			return this->m_backBuffers[this->GetCurrentBufferIndex()];
		}

		DXGI_FORMAT GetFormat() const { return this->Format; }

		/**
		 * Get the render target view for the current back buffer.
		 */
		D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRenderTargetView() const
		{
			return this->m_rtvHeap->GetCpuHandle(this->m_currBufferIndex);
		}

	private:
		const int NumOfBuffers;
		const DXGI_FORMAT Format;
		int m_currBufferIndex;

		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_backBuffers;

		Microsoft::WRL::ComPtr<IDXGISwapChain4> m_swapChain;
		std::shared_ptr<DescriptorHeap> m_rtvHeap;
	};
}


#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <memory>
#include <vector>

namespace Core
{
	class DescriptorHeap;
	class SwapChain
	{
	public:
		SwapChain(
			Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain,
			std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>&& m_backBuffers,
			DXGI_FORMAT format);
		~SwapChain() {};

		void Present();

		int GetCurrentBufferIndex() const { return this->m_currBufferIndex; }
		Microsoft::WRL::ComPtr<ID3D12Resource> GetCurrentBackBuffer()
		{ 
			return this->m_backBuffers[this->GetCurrentBufferIndex()];
		}

		DXGI_FORMAT GetFormat() const { return this->Format; }

	private:
		const int NumOfBuffers;
		const DXGI_FORMAT Format;
		int m_currBufferIndex;

		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_backBuffers;

		Microsoft::WRL::ComPtr<IDXGISwapChain4> m_swapChain;
		std::shared_ptr<DescriptorHeap> m_rtvHeap;
	};
}


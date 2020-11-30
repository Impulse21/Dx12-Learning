#pragma once

#include "Log.h"
#include "Window.h"

// DirectX 12 specific headers.
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

// D3D12 extension library.
#include "Dx12/d3dx12.h"

#ifdef _DEBUG
#include <dxgidebug.h>
#endif

#include "Dx12/DescriptorHeap.h"
#include "Dx12/CommandQueue.h"
#include "Dx12/SwapChain.h"
#include "Dx12/PipelineStateBuilder.h"

// -- STL ---
#include <memory>

namespace Core
{
	class Dx12Application
	{
	public:
		Dx12Application() = default;
		virtual ~Dx12Application();

		void RunApplication();

	protected:
		virtual void LoadContent() {};
		virtual void Update(double deltaTime) {};
		virtual void Render() {};


		void SetCurrentFrameFence(uint64_t fenceValue)
		{
			this->m_frameFences[this->m_swapChain->GetCurrentBufferIndex()] = fenceValue;
		}

		// -- Potential Render Device functions ---
	protected:
		void UploadBufferResource(
			ID3D12GraphicsCommandList2* commandList,
			ID3D12Resource** pDestinationResource,
			ID3D12Resource** pIntermediateResource,
			size_t numOfElements,
			size_t elementStride,
			const void* data,
			D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

	private:
		void Ininitialize();
		void Shutdown();

	private:
		void InitializeDx12();

		void CreateDevice(Microsoft::WRL::ComPtr<IDXGIFactory6> dxgiFactory);
		void CreateSwapChain(Microsoft::WRL::ComPtr<IDXGIFactory6> dxgiFactory, IWindow* window);
		void EnableDebugLayer();
		Microsoft::WRL::ComPtr<IDXGIAdapter1> FindCompatibleAdapter(Microsoft::WRL::ComPtr<IDXGIFactory6> dxgiFactory);

		void CreateCommandQueues();
		void CreateDescriptorHeaps();
		void InitializeSwapChainRenderTargets();

	protected:
		const uint8_t NumOfBuffers = 3;
		std::shared_ptr<IWindow> m_window;

		// -- Dx12 resources ---
		Microsoft::WRL::ComPtr<IDXGIAdapter1> m_dxgiAdapter;
		Microsoft::WRL::ComPtr<ID3D12Device2> m_d3d12Device;

		std::unique_ptr<SwapChain> m_swapChain;

		// -- Queues ---
		std::unique_ptr<CommandQueue> m_directQueue;
		std::unique_ptr<CommandQueue> m_computeQueue;
		std::unique_ptr<CommandQueue> m_copyQueue;

		// -- Descriptors ---
		std::unique_ptr<DescriptorHeap> m_rtvDescriptorHeap;
		std::unique_ptr<DescriptorHeap> m_dsvDescriptorHeap;

		// -- Frame resources ---
		std::vector<uint64_t> m_frameFences;
	};
}

#define MAIN_FUNCTION() int main()

#define CREATE_APPLICATION( app_Class ) \
	MAIN_FUNCTION() \
	{ \
	   Core::Log::Initialize(); \
       std::unique_ptr<Core::Dx12Application> app = std::make_unique<app_Class>(); \
	   app->RunApplication();\
	   return 0; \
	}

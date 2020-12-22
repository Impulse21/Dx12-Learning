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

#include "Dx12/Dx12RenderDevice.h"
#include "Dx12/SwapChain.h"
#include "Dx12/PipelineStateBuilder.h"
#include "Dx12/CommandList.h"
#include "Dx12/GraphicResourceTypes.h"

#include "UserInterface.h"

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
		virtual void RenderScene(Dx12Texture& sceneTexture) {};
		virtual void RenderUI() {};

		void SetCurrentFrameFence(uint64_t fenceValue)
		{
			this->m_frameFences[this->m_swapChain->GetCurrentBufferIndex()] = fenceValue;
		}

	private:
		void Ininitialize();
		void Shutdown();

		void EndFrame();

	private:
		void InitializeDx12();

		void CreateSwapChain(Microsoft::WRL::ComPtr<IDXGIFactory6> dxgiFactory, IWindow* window);

	protected:
		const uint8_t NumOfBuffers = 3;
		std::shared_ptr<IWindow> m_window;

		std::shared_ptr<Dx12RenderDevice> m_renderDevice;
		std::unique_ptr<SwapChain> m_swapChain;

		std::unique_ptr<IUserInterface> m_gui = nullptr;

		// -- Frame resources ---
		std::vector<uint64_t> m_frameFences;
		std::vector<uint64_t> m_frameCounts;
		uint64_t m_frameCount = 0;
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

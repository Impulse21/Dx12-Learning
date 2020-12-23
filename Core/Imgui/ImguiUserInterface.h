#pragma once

#include "UserInterface.h"
#include "imgui.h"
#include "Dx12/d3dx12.h"
#include <wrl.h>

#include <memory>

#include "Dx12/GraphicResourceTypes.h"
#include "Dx12/RootSignature.h"

namespace Core
{
	class IWindow;
	class ImguiUserInterface : public IUserInterface
	{
	public:
		ImguiUserInterface() = default;
		~ImguiUserInterface() { this->Destory(); }

		bool Initialize(
			std::shared_ptr<Dx12RenderDevice> renderDevice,
			std::shared_ptr<IWindow> window) override;

		void NewFrame() override;
		void Render(std::shared_ptr<CommandList> commandList, RenderTarget const& renderTarget) override;

		void Destory() override;

	private:
		void CreatePipelineStateObject();
	private:
		std::shared_ptr<Dx12RenderDevice> m_renderDevice;
		std::shared_ptr<IWindow> m_window;

		ImGuiContext* m_imguiContext;
		std::unique_ptr<Dx12Texture> m_fontTexture;
		std::unique_ptr<RootSignature> m_rootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
	};
}


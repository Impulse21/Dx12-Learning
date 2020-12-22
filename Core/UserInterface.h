#pragma once

#include <memory>

namespace Core
{
	class IWindow;
	class Dx12RenderDevice;
	class CommandList;
	class RenderTarget;

	class IUserInterface
	{
	public:
		static std::unique_ptr<IUserInterface> Create();

		virtual bool Initialize(
			std::shared_ptr<Dx12RenderDevice> renderDevice,
			std::shared_ptr<IWindow> window) = 0;
		virtual void NewFrame() = 0;
		virtual void Render(std::shared_ptr<CommandList> commandList, RenderTarget const& renderTarget) = 0;

		virtual void Destory() = 0;


		virtual ~IUserInterface() = default;
	};
}
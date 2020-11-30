#pragma once

#include <stdint.h>
#include <memory>
#include <string>

namespace Core
{
	using WindowFlags = uint32_t;

	enum WindowOptions
	{
		None = 0,
		DisableFullscreen = 0x01,
		DisableResize = 0x02,
	};

	struct WindowProperties
	{
		uint32_t Width;
		uint32_t Height;
		std::string Title;

		WindowFlags Flags;

		WindowProperties()
			: Title("Beyond Crisis Engine")
			, Width(1280)
			, Height(720)
			, Flags(WindowOptions::None)
		{}

		WindowProperties(std::string const& title)
			: Title(title)
			, Width(1280)
			, Height(720)
			, Flags(WindowOptions::None)
		{}

		WindowProperties(std::string const& title, uint32_t width, uint32_t height)
			: Title(title)
			, Width(width)
			, Height(height)
			, Flags(WindowOptions::None)
		{}
	};

	class IWindow
	{
	public:
		static std::shared_ptr<IWindow> Create();

		virtual void PullEvents() = 0;
		virtual bool IsClosing() const = 0;
		virtual bool IsMinimized() const = 0;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;

		virtual std::string GetTitle() const = 0;

		virtual void* GetNativeHandle() = 0;

		virtual ~IWindow() {};

	};
}
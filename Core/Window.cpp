#include "pch.h"
#include "Window.h"
#include "GLFW/glfw3.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3native.h"

using namespace Core;

static bool sGlwfIsInitialzed = false;

class GltfWindow : public IWindow
{
public:
	GltfWindow(WindowProperties const& windowProperties)
		: m_properties(windowProperties)
	{
		LOG_CORE_INFO("Creating window {0} ({1}, {2})", this->m_properties.Title, this->m_properties.Width, this->m_properties.Height);

		if (!sGlwfIsInitialzed)
		{
			int success = glfwInit();
			LOG_CORE_ASSERT(success, "Could not initialize GLFW");
		}

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		// TODO Check Flags
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		this->m_window = 
			glfwCreateWindow(
				this->m_properties.Width,
				this->m_properties.Height,
				this->m_properties.Title.c_str(),
				nullptr,
				nullptr);

		glfwSetWindowUserPointer(this->m_window, &this->m_windowState);

		// Handle Close Event
		glfwSetWindowCloseCallback(this->m_window, [](GLFWwindow* window) {
			WindowState& state = *(WindowState*)glfwGetWindowUserPointer(window);
			state.IsClosing = true;
			});
	}

	~GltfWindow()
	{
		if (this->m_window)
		{
			LOG_CORE_INFO("Destroying Window");
			glfwDestroyWindow(this->m_window);
		}

		if (sGlwfIsInitialzed)
		{
			glfwTerminate();
			sGlwfIsInitialzed = false;
		}
	}

	void PullEvents() override final
	{
		glfwPollEvents();
	}

public:
	bool IsClosing() const override final { return this->m_windowState.IsClosing; }
	bool IsMinimized() const override final { return false; }

	uint32_t GetWidth() const override final { return this->m_properties.Width; }
	uint32_t GetHeight() const override final { return this->m_properties.Height; }

	std::string GetTitle() const override final { return this->m_properties.Title; }

	void* GetNativeHandle() override final
	{
		return glfwGetWin32Window(this->m_window);
	}

private:
	struct WindowState
	{
		bool IsClosing{ false };
	};

	WindowState m_windowState;
	WindowProperties m_properties;
	GLFWwindow* m_window;
};


std::shared_ptr<IWindow> IWindow::Create()
{
	return std::make_shared<GltfWindow>(WindowProperties());
}

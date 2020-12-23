#include "pch.h"

#include "UserInterface.h"

#include "Imgui/ImguiUserInterface.h"

using namespace Core;

std::unique_ptr<IUserInterface> Core::IUserInterface::Create()
{
	return std::make_unique<ImguiUserInterface>();
}

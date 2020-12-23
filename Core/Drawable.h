#pragma once

#include <memory>

namespace Core
{
	class CommandList;

	class IDrawable
	{
	public:

		virtual void Draw(
			CommandList& commandList,
			uint32_t instanceCount = 1,
			uint32_t firstInstance = 0) = 0;

		virtual ~IDrawable() = default;
	};

}

#pragma once

#include <string>
#include <vector>

namespace Core
{
	class BinaryReader
	{
	public:
		static bool ReadFile(std::string const& filename, std::vector<char>& outByteData);
	};
}
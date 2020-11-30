#include "pch.h"
#include "File.h"

using namespace Core;

bool BinaryReader::ReadFile(std::string const& filename, std::vector<char>& outByteData)
{
	/*
	The advantage of starting to read at the end of the file is that
	we can use the read position to determine the size of the file and allocate a buffer:
	*/

	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		return false;
	}

	size_t fileSize = (size_t)file.tellg();
	outByteData.clear();
	outByteData.resize(fileSize);

	// Move back to the start
	file.seekg(0);
	file.read(outByteData.data(), fileSize);
	file.close();

	return true;
}

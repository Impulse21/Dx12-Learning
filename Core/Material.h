#pragma once

#include <DirectXMath.h>

namespace Core
{
	struct Material
	{
		Material(
			DirectX::XMFLOAT4 diffuse = { 1.0f, 1.0f, 1.0f, 1.0f })
			: Diffuse(diffuse)
		{
		}

		DirectX::XMFLOAT4 Diffuse;
		// --------------------------------------- (16 byte boundary)

		static const Material Red;
		static const Material Green;
		static const Material Blue;
	};
}
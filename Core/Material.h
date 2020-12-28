#pragma once

#include <DirectXMath.h>

namespace Core
{
	struct Material
	{
		Material(
			DirectX::XMFLOAT4 diffuse = { 1.0f, 1.0f, 1.0f, 1.0f },
			DirectX::XMFLOAT4 ambient = { 1.0f, 0.5f, 0.31f, 1.0f },
			DirectX::XMFLOAT4 specular = { 0.5f, 0.5f, 0.5f, 1.0f },
			float shininess = 32.0f)
			: Ambient(ambient)
			, Diffuse(diffuse)
			, Specular(specular)
			, Shininess(shininess)

		{
		}

		DirectX::XMFLOAT4 Ambient;
		// --------------------------------------- (16 byte boundary)

		DirectX::XMFLOAT4 Diffuse;
		// --------------------------------------- (16 byte boundary)

		DirectX::XMFLOAT4 Specular;
		// --------------------------------------- (16 byte boundary)
		float Shininess;
		float Padding[3];
		

		static const Material Red;
		static const Material Green;
		static const Material Blue;
	};
}
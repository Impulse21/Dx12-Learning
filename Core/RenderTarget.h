#pragma once

#include <DirectXMath.h>

#include "Dx12/GraphicResourceTypes.h"
namespace Core
{
	enum AttachmentPoint
	{
		Color0,
		Color1,
		Color2,
		Color3,
		Color4,
		Color5,
		Color6,
		Color7,
		Color8,
		DepthStencil,
		NumAttachmentPoints,
		NumColourAttachments = Color8,
	};

	class RenderTarget
	{
	public:
		RenderTarget();
		RenderTarget(RenderTarget const& copy) = default;
		RenderTarget(RenderTarget&& copy) = default;

		RenderTarget& operator=(const RenderTarget& other) = default;
		RenderTarget& operator=(RenderTarget && other) = default;

		void AttachTexture(AttachmentPoint attachmentPoint, Dx12Texture texture);
		const Dx12Texture& GetTexture(AttachmentPoint attachmentPoint) const { return this->m_textures[attachmentPoint]; }

		void Resize(DirectX::XMUINT2 size);
		void Resize(uint32_t width, uint32_t height);

		DirectX::XMUINT2 GetSize() const { return this->m_size; }
		uint32_t GetWidth() const { return this->m_size.x; }
		uint32_t GetHeight() const { return this->m_size.y; }

		// Get a viewport for this render target.
		// The scale and bias parameters can be used to specify a split-screen
		// viewport (the bias parameter is normalized in the range [0...1]).
		// By default, a fullscreen viewport is returned.
		D3D12_VIEWPORT GetViewport(
			DirectX::XMFLOAT2 scale = { 1.0f, 1.0f },
			DirectX::XMFLOAT2 bias = { 0.0f, 0.0f },
			float minDepth = 0.0f,
			float maxDepth = 1.0f) const;

		const std::vector<Dx12Texture>& GetTextures() const { return this->m_textures; }

		D3D12_RT_FORMAT_ARRAY GetRenderTargetForamts() const;

		DXGI_FORMAT GetDepthStencilFormat() const;

	private:
		std::vector<Dx12Texture> m_textures;
		DirectX::XMUINT2 m_size;
	};
}


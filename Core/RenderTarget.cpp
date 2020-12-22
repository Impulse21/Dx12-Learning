#include "pch.h"
#include "RenderTarget.h"


using namespace Core;

Core::RenderTarget::RenderTarget()
	: m_textures(AttachmentPoint::NumAttachmentPoints)
	, m_size(0,0)
{
}

void Core::RenderTarget::AttachTexture(AttachmentPoint attachmentPoint, Dx12Texture texture)
{
	this->m_textures[attachmentPoint] = std::move(texture);

	if (texture.GetDx12Resource())
	{
		auto desc = texture.GetDx12Resource()->GetDesc();
		this->m_size.x = static_cast<uint32_t>(desc.Width);
		this->m_size.y = static_cast<uint32_t>(desc.Height);
	}
}

void Core::RenderTarget::Resize(DirectX::XMUINT2 size)
{
	/*
	this->m_size = size;
	for (auto& texture : this->m_textures)
	{
		texture.Resize(size);
	}
	*/
}

void Core::RenderTarget::Resize(uint32_t width, uint32_t height)
{
	this->Resize(DirectX::XMUINT2(width, height));
}

D3D12_VIEWPORT Core::RenderTarget::GetViewport(DirectX::XMFLOAT2 scale, DirectX::XMFLOAT2 bias, float minDepth, float maxDepth) const
{
	UINT64 width = 0;
	UINT height = 0;
	
	for (int i = AttachmentPoint::Color0; i <= AttachmentPoint::Color7; ++i)
	{
		const Dx12Texture& texture = this->m_textures[i];
		if (texture.GetDx12Resource())
		{
			auto desc = texture.GetDx12Resource()->GetDesc();
			width = std::max(width, desc.Width);
			height = std::max(height, desc.Height);
		}
	}

	D3D12_VIEWPORT viewport = {
		(width * bias.x),       // TopLeftX
		(height * bias.y),      // TopLeftY
		(width * scale.x),      // Width
		(height * scale.y),     // Height
		minDepth,               // MinDepth
		maxDepth                // MaxDepth
	};

	return viewport;
}

D3D12_RT_FORMAT_ARRAY Core::RenderTarget::GetRenderTargetForamts() const
{
	D3D12_RT_FORMAT_ARRAY rtvFormats = {};

	for (int i = AttachmentPoint::Color0; i <= AttachmentPoint::Color7; ++i)
	{
		const Dx12Texture& texture = this->m_textures[i];
		if (texture.GetDx12Resource())
		{
			rtvFormats.RTFormats[rtvFormats.NumRenderTargets++] =
				texture.GetDx12Resource()->GetDesc().Format;
		}
	}

	return rtvFormats;
}

DXGI_FORMAT Core::RenderTarget::GetDepthStencilFormat() const
{
	DXGI_FORMAT dsvFormat = DXGI_FORMAT_UNKNOWN;
	const auto& depthStencilTexture = this->m_textures[AttachmentPoint::DepthStencil];
	if (depthStencilTexture.GetDx12Resource())
	{
		dsvFormat = depthStencilTexture.GetDx12Resource()->GetDesc().Format;
	}

	return dsvFormat;
}

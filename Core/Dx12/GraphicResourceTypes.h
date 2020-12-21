#pragma once

#include "d3dx12.h"
#include <wrl.h>

#include "DescriptorAllocation.h"

namespace Core
{
	enum class BufferUsage : uint8_t
	{
		Static = 0,
		Dynamic,
	};

	enum class TextureUsage : uint8_t
	{
		Albedo,
		Diffuse = Albedo,       // Treat Diffuse and Albedo textures the same.
		Heightmap,
		Depth = Heightmap,      // Treat height and depth textures the same.
		Normalmap,
		RenderTarget,           // Texture is used as a render target.
	};
	enum BindFlags : uint32_t
	{
		BIND_NONE				= 0x00,
		BIND_VERTEX_BUFFER		= 0x01,
		BIND_INDEX_BUFFER		= 0x02,
		BIND_CONSTANT_BUFFER	= 0x04,
		BIND_STRUCTURED_BUFFER  = 0x08,

		BIND_SHADER_RESOURCE	= 0x10,

		BIND_RENDER_TARGET		= 0x20,
		BIND_DEPTH_STENCIL		= 0x40,
		BIND_UNORDERED_ACCESS	= 0x80,
	};

	class Dx12RenderDevice;

	class Dx12Resrouce
	{
	public:
		virtual ~Dx12Resrouce() = default;

		Microsoft::WRL::ComPtr<ID3D12Resource> GetDx12Resource() const { return this->m_d3dResouce; } 
		void SetDx12Resource(Microsoft::WRL::ComPtr<ID3D12Resource> d3dResource) { this->m_d3dResouce = d3dResource; }

		/**
		 * Get the SRV for a resource.
		 *
		 * @param srvDesc The description of the SRV to return. The default is nullptr
		 * which returns the default SRV for the resource (the SRV that is created when no
		 * description is provided.
		 */
		virtual D3D12_CPU_DESCRIPTOR_HANDLE GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc = nullptr) const = 0;

		/**
		 * Get the UAV for a (sub)resource.
		 *
		 * @param uavDesc The description of the UAV to return.
		 */
		virtual D3D12_CPU_DESCRIPTOR_HANDLE GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc = nullptr) const = 0;


	protected:
		Dx12Resrouce() = default;
		Dx12Resrouce(
			std::shared_ptr<Dx12RenderDevice> renderDevice,
			Microsoft::WRL::ComPtr<ID3D12Resource> resource)
			: m_d3dResouce(resource)
			, m_renderDevice(renderDevice)
		{}

	protected:
		Microsoft::WRL::ComPtr<ID3D12Resource> m_d3dResouce = nullptr;

		std::shared_ptr<Dx12RenderDevice> m_renderDevice = nullptr;
	};

	// -- Buffer Description ---

	struct BufferDesc
	{
		BufferUsage Usage = BufferUsage::Static;

		BindFlags BindFlags = BIND_NONE;

		size_t NumElements = 0;

		uint32_t ElementByteStride = 0;

	};

	class Dx12Buffer : public Dx12Resrouce
	{
	public:
		Dx12Buffer(
			std::shared_ptr<Dx12RenderDevice> renderDevice,
			BufferDesc desc)
			: Dx12Resrouce(renderDevice, nullptr)
			, m_bufferDesc(std::move(desc))
		{};

		Dx12Buffer(
			std::shared_ptr<Dx12RenderDevice> renderDevice,
			BufferDesc desc, Microsoft::WRL::ComPtr<ID3D12Resource> resource)
			: Dx12Resrouce(renderDevice, resource)
			, m_bufferDesc(std::move(desc))
		{}

		size_t GetSizeInBytes() const { return this->GetNumElements() * this->GetElementByteStride(); }
		size_t GetNumElements() const { return this->m_bufferDesc.NumElements; }
		uint32_t GetElementByteStride() const { return this->m_bufferDesc.ElementByteStride; }
		BindFlags GetBindings() const { return this->m_bufferDesc.BindFlags; }

		D3D12_CPU_DESCRIPTOR_HANDLE GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc = nullptr) const override
		{
			throw std::runtime_error("This functions should never be called on buffers");
		}

		D3D12_CPU_DESCRIPTOR_HANDLE GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc = nullptr) const override
		{
			throw std::runtime_error("This functions should never be called on buffers");
		}

	private:
		BufferDesc m_bufferDesc;
	};

	struct TextureDesc
	{
		TextureUsage Usage = TextureUsage::Albedo;
		BindFlags Binding = BindFlags::BIND_SHADER_RESOURCE;
		D3D12_RESOURCE_DESC ResourceDesc = {};
	};

	class Dx12Texture : public Dx12Resrouce
	{
	public:
		Dx12Texture(
			std::shared_ptr<Dx12RenderDevice> renderDevice,
			TextureDesc desc)
			: Dx12Resrouce(renderDevice, nullptr)
			, m_textureDesc(std::move(desc))
		{};

		Dx12Texture(
			std::shared_ptr<Dx12RenderDevice> renderDevice,
			TextureDesc desc,
			Microsoft::WRL::ComPtr<ID3D12Resource> resource)
			: Dx12Resrouce(renderDevice, resource)
			, m_textureDesc(std::move(desc))
		{}

		/*
		bool CheckSRVSupport()
		{
			return CheckFormatSupport(D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE);
		}

		bool CheckRTVSupport()
		{
			return CheckFormatSupport(D3D12_FORMAT_SUPPORT1_RENDER_TARGET);
		}

		bool CheckUAVSupport()
		{
			return CheckFormatSupport(D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW) &&
				CheckFormatSupport(D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) &&
				CheckFormatSupport(D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE);
		}

		bool CheckDSVSupport()
		{
			return CheckFormatSupport(D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL);
		}
		*/

		D3D12_CPU_DESCRIPTOR_HANDLE GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc = nullptr) const override;

		D3D12_CPU_DESCRIPTOR_HANDLE GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc = nullptr) const override;

	private:
		DescriptorAllocation CreateShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc) const;
		DescriptorAllocation CreateUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc) const;

	private:
		TextureDesc m_textureDesc;

		mutable std::unordered_map<size_t, DescriptorAllocation> m_shaderResourceViews;
		mutable std::unordered_map<size_t, DescriptorAllocation> m_unorderedAccessViews;

		mutable std::mutex m_shaderResourceViewsMutex;
		mutable std::mutex m_unorderedAccessViewsMutex;
	};
}


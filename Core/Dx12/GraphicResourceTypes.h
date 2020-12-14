#pragma once

#include "d3dx12.h"
#include <wrl.h>

namespace Core
{
	enum class Usage : uint8_t
	{
		Static = 0,
		Dynamic,
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

	class Dx12Resrouce
	{
	public:
		virtual ~Dx12Resrouce() = default;

		Microsoft::WRL::ComPtr<ID3D12Resource> GetDx12Resource() { return this->m_d3dResouce; }
		void SetDx12Resource(Microsoft::WRL::ComPtr<ID3D12Resource> d3dResource) { this->m_d3dResouce = d3dResource; }
	protected:
		Dx12Resrouce() = default;
		Dx12Resrouce(Microsoft::WRL::ComPtr<ID3D12Resource> resource)
			: m_d3dResouce(resource)
		{}

	protected:
		Microsoft::WRL::ComPtr<ID3D12Resource> m_d3dResouce = nullptr;
	};

	// -- Buffer Description ---

	struct BufferDesc
	{
		Usage Usage = Usage::Static;

		BindFlags BindFlags = BIND_NONE;

		size_t NumElements = 0;

		uint32_t ElementByteStride = 0;

	};

	class Dx12Buffer : public Dx12Resrouce
	{
	public:
		Dx12Buffer(BufferDesc desc)
			: m_bufferDesc(std::move(desc))
		{};

		Dx12Buffer(BufferDesc desc, Microsoft::WRL::ComPtr<ID3D12Resource> resource)
			: Dx12Resrouce(resource)
			, m_bufferDesc(std::move(desc))
		{}

		size_t GetSizeInBytes() const { return this->GetNumElements() * this->GetElementByteStride(); }
		size_t GetNumElements() const { return this->m_bufferDesc.NumElements; }
		uint32_t GetElementByteStride() const { return this->m_bufferDesc.ElementByteStride; }
		BindFlags GetBindings() const { return this->m_bufferDesc.BindFlags; }

	private:
		BufferDesc m_bufferDesc;
	};
}


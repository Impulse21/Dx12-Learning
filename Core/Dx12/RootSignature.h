#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <string>
namespace Core
{
	class Dx12RenderDevice;
	class RootSignature
	{
	public:
		RootSignature(std::shared_ptr<Dx12RenderDevice> renderDevice, std::wstring debugName = L"");
		RootSignature(
			std::shared_ptr<Dx12RenderDevice> renderDevice,
			D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc,
			D3D_ROOT_SIGNATURE_VERSION rootSignatureVersion,
			std::wstring debugName = L"");

		~RootSignature();

		void Destory();

		Microsoft::WRL::ComPtr<ID3D12RootSignature> GetRootSignature() const
		{
			return this->m_rootSignature;
		}

		void SetRootSignatureDesc(
			D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc,
			D3D_ROOT_SIGNATURE_VERSION rootSignatureVersion);

		const D3D12_ROOT_SIGNATURE_DESC1& GetRootSignatureDesc() const
		{
			return this->m_rootSignatureDesc;
		}

		uint32_t GetDescriptorTableBitMask(D3D12_DESCRIPTOR_HEAP_TYPE descriptorHeapType) const;
		uint32_t GetNumDescriptors(uint32_t rootIndex) const;

	private:
		std::shared_ptr<Dx12RenderDevice> m_renderDevice;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
		D3D12_ROOT_SIGNATURE_DESC1 m_rootSignatureDesc;

		// Need to know the number of descriptors per descriptor table.
		// A maximum of 32 descriptor tables are supported (since a 32-bit
		// mask is used to represent the descriptor tables in the root signature.
		uint32_t m_numDescriptorsPerTable[32];

		// A bit mask that represents the root parameter indices that are 
		// descriptor tables for Samplers.
		uint32_t m_samplerTableBitMask;

		// A bit mask that represents the root parameter indices that are 
		// CBV, UAV, and SRV descriptor tables.
		uint32_t m_descriptorTableBitMask;

		std::wstring m_debugName;
	};
}


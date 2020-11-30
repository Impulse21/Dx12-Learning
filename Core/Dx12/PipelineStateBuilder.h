#pragma once

#include "Dx12/d3dx12.h"
#include <wrl.h>
namespace Core
{
	class PipelineStateBuilder
	{
	public:
		PipelineStateBuilder();
		~PipelineStateBuilder() = default;

		Microsoft::WRL::ComPtr<ID3D12PipelineState> Build(ID3D12Device2* device, std::wstring const& debugName = L"");

		void SetRootSignature(ID3D12RootSignature* rootSignature) { this->m_psoDesc.pRootSignature = rootSignature; }

		void SetInputElementDesc(std::vector<D3D12_INPUT_ELEMENT_DESC> const& inputElementDescs);

		void SetVertexShader(std::vector<char> const& bytecode);
		void SetPixelShader(std::vector<char> const& bytecode);

		void SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE topology);

		void SetRenderTargetFormat(DXGI_FORMAT format);

		
	private:
		D3D12_GRAPHICS_PIPELINE_STATE_DESC m_psoDesc = {};

	};
}


#include "pch.h"
#include "PipelineStateBuilder.h"

using namespace Core;

Core::PipelineStateBuilder::PipelineStateBuilder()
{
    {
        DXGI_SAMPLE_DESC sampleDesc = {};
        sampleDesc.Count = 1; // multisample count (no multisampling, so we just put 1, since we still need 1 sample)
        this->m_psoDesc.SampleDesc = sampleDesc; // must be the same sample description as the swapchain and depth/stencil buffer
    }

    this->m_psoDesc.SampleMask = 0xffffffff; // sample mask has to do with multi-sampling. 0xffffffff means point sampling is done
    this->m_psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
    this->m_psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
    this->m_psoDesc.NumRenderTargets = 1; // we are only binding one render target

}

Microsoft::WRL::ComPtr<ID3D12PipelineState> Core::PipelineStateBuilder::Build(ID3D12Device2* device, std::wstring const& debugName)
{
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStateObject;
    ThrowIfFailed(
        device->CreateGraphicsPipelineState(&this->m_psoDesc, IID_PPV_ARGS(&pipelineStateObject)));

    if (debugName != L"")
    {
        ThrowIfFailed(
            pipelineStateObject->SetName(debugName.c_str()));
    }

    return pipelineStateObject;
}

void Core::PipelineStateBuilder::SetInputElementDesc(std::vector<D3D12_INPUT_ELEMENT_DESC> const& inputElementDescs)
{
    D3D12_INPUT_LAYOUT_DESC inputDesc = {};
    inputDesc.NumElements = inputElementDescs.size();
    inputDesc.pInputElementDescs = inputElementDescs.data();

    this->m_psoDesc.InputLayout = inputDesc;
}

void Core::PipelineStateBuilder::SetVertexShader(std::vector<char> const& bytecode)
{
    this->m_psoDesc.VS = { bytecode.data(), bytecode.size() };
}

void Core::PipelineStateBuilder::SetPixelShader(std::vector<char> const& bytecode)
{
    this->m_psoDesc.PS = { bytecode.data(), bytecode.size() };
}

void Core::PipelineStateBuilder::SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE topology)
{
    this->m_psoDesc.PrimitiveTopologyType = topology;
}

void Core::PipelineStateBuilder::SetRenderTargetFormat(DXGI_FORMAT format)
{
    this->m_psoDesc.RTVFormats[0] = format;
}

